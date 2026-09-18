# 构建缓存

## 缓存内容

| 类型                         | 存储位置                  | 说明                             |
| ---------------------------- | ------------------------- | -------------------------------- |
| 编译目标文件（`.o`/`.obj`）  | `.ezmk/cache/obj/`        | 每个源文件对应的目标文件         |
| 编译依赖信息（含头文件哈希） | `.ezmk/cache/record.json` | 记录编译元数据，用于决定是否命中 |

## 缓存命中判断流程

当执行 `ezmk build` 时，对每个源文件 `src/foo.cpp`：

1. **计算源文件当前哈希**（基于文件内容）。
2. **读取缓存记录** `record.json` 中该源文件的条目。
3. **比较基本条件**：
   - 源文件哈希与上次记录相同？
   - **全局编译选项签名**（`compile_options_signature`，由 `[compile] flags`、`msvc_flags`、`include_dirs`、std 标志、stdlib 与 PIC 推导）与记录相同？  
     （编译选项影响输出，必须作为 key 的一部分——但这是**全局**比较而非逐条目比较：条目里的 `compile_opts` 数组仅供诊断，代码从不比较它。）
4. **检查所有依赖的头文件**：
   - 遍历上次记录的每个头文件路径，计算当前该头文件的哈希，与上次记录的哈希比较。
   - 若任一已记录头文件的哈希改变，则失效。记录中的路径**集合**不参与比较：新增/删除的头文件只有在源文件重新编译后才会写进记录，因此「集合变化」本身不会被判定为失效。
5. **判定结果**：
   - 若全部匹配 → **命中缓存**，直接使用已有的 `.o` 文件，不重新编译。
   - 否则 → **缓存失效**，重新编译源文件，并更新 `record.json` 和 `.o` 文件。

### 流程图

```
对每个源文件:
    cur_source_hash = hash(file)
    cur_sig = compile_options_signature(...)
    从 record.json 读取 last_entry
    if last_entry 存在 
        && last_entry.source_hash == cur_source_hash
        && record.compile_options_signature == cur_sig:
        # 检查头文件
        all_header_match = true
        for each (hdr, last_hash) in last_entry.headers:
            cur_hash = hash(hdr)
            if cur_hash != last_hash:
                all_header_match = false; break
        if all_header_match:
            # 命中缓存
            continue
    # 未命中：重新编译
    compile source -> .o
    generate .d 获取头文件列表及哈希
    更新 record.json
```

> **为什么用内容哈希而不是时间戳？** 选择内容哈希，是为了让缓存命中真正表示文件内容未变——
> `touch` 或 `git checkout` 可能改变 mtime 而不改变内容，基于 mtime 的缓存会误判或失效。

## 缓存记录结构（`record.json`）

```json
{
  "version": 2,
  "compiler": "g++",
  "compiler_version": "g++ (GCC) 14.2.0",             // 检测到的编译器版本串；此处变化会使整份缓存失效
  "compile_options_signature": "sha256_of_flags_include_dirs_std_flag_and_env",  // 全局编译选项指纹（含 msvc_flags、std_flag、include_dirs）
  "deterministic": false,
  "files": {
    "src/main.cpp": {
      "source_hash": "a3f5c9...",
      "object_file": ".ezmk/cache/obj/main.o",
      "compiler": "g++",
      "compile_opts": ["-Wall", "-O2"],   // 可与全局指纹冗余，便于调试
      "dependencies": [
        {"path": "include/foo.h", "hash": "b4e8d2..."},
        {"path": "/usr/include/iostream", "hash": "c6a0b1..."}   // 仅在 MSVC 路径下出现——GCC/Clang 的 `-MMD` 不含系统头
      ],
      "last_build_time": "2025-03-01T12:34:56Z"
    },
    "src/utils.cpp": { ... }
  }
}
```

> **为什么记录里要有 `version` 字段？** 缓存格式会随 ezmk 版本演进；版本号让不同版本的 ezmk
> 能识别磁盘上较新的格式并重建缓存，而不是错误地解析它。1.4.2 起该校验被强制执行：记录中的
> `version` **高于**当前 ezmk 支持的版本（2）时整份记录作废——所有源文件都按未命中处理，
> 即一次全量重建——避免降级后只信任部分字段；版本**更低**的记录仍可加载，较新字段取默认值。

### 字段说明
- `version`：用于缓存格式演进；当前格式为 `2`（1.1.0 新增 `compiler`、`compiler_version` + `deterministic`）。版本高于当前支持的记录会被整份忽略（见上）。
- `compiler` / `compiler_version`：写入记录的编译器名称与检测到的版本串。真正使缓存失效的是**版本串**：与当前检测到的编译器版本不一致时，构建前会丢弃全部条目。`compiler` 名称仅用于诊断，判断命中时不参与比较（见下）。
- `deterministic`：写入记录时是否启用了确定性构建；切换该开关会使缓存失效。
- `compile_options_signature`：全局编译选项的 SHA-256 指纹，涵盖 `[compile] flags`、`msvc_flags`、`include_dirs`、`std_flag`、`extra_includes` 等。任一变化导致所有源文件缓存失效。该签名是**唯一**与选项相关的 key：条目里的 `compile_opts` 只用于诊断，从不参与比较。
- `object_file` 相对路径。
- `dependencies` 里最终有哪些头文件取决于工具链。GCC/Clang 走 `-MMD`，按定义只把**用户**头文件写进依赖文件，因此系统头（如 `/usr/include/iostream`）**不会**被记录；MSVC 没有依赖文件，改为解析 `/showIncludes` 输出，其中**包含**系统头。

> **跨工具链时靠什么失效？** 失效依据是检测到的编译器**版本串**（`compiler_version`）：
> 它一变，构建前所有缓存条目都会被丢弃。记录中的 `compiler` 字段只写不读，
> 目前不参与命中判断。

## 缓存一致性维护

### 头文件更新后自动触发
依赖头文件哈希比较机制天然保证：修改任何头文件，引用它的所有源文件都会重新编译。

### 编译选项变更
- 全局标志变化 → 清空所有缓存条目（简单实现）或逐条比较 `compile_opts`（复杂）。
- 推荐：在 `record.json` 中存储全局标志指纹，构建前对比当前指纹，不同则丢弃整个缓存，重新全量编译。

> **为什么用粗粒度的全局指纹而不是逐条比对？** 标志变更很少见，指纹变化时全量重编一次代价很低；
> 相比逐条核对每个条目，这更简单、更不易出错。

### 文件移动/重命名
- 若源文件路径改变，旧缓存条目保留但不会被命中（因为构建扫描路径产生新条目），用户可通过 `ezmk clean` 手动清理。
- 头文件路径变更：旧记录中的路径失效，编译时会因缺失头文件而报错，用户需确保路径正确。

## 缓存禁用

`ezmk build --disable-cache`：
- 忽略 `record.json`，所有源文件重新编译。
- 本次构建**不写入任何缓存条目**，磁盘上的记录被清空（1.4.2）。1.4.2 之前会把本次编译结果并入
  记录，于是「禁用缓存」的构建反而为**下一次**构建重新武装了缓存，与开关承诺相反。

> **为什么清空记录而不是保持原样？** 保留旧条目会让下一次正常构建命中那些描述「本次被刻意绕过
> 的构建」的条目；清空后，下一次构建从干净且一致的增量状态开始。

## 缓存调试（`--verbose`）

`ezmk build --verbose` / `ezmk build -v` 会打印每个源文件的缓存判断详情：

- **命中时**：输出 `[cached]` + 匹配的源文件哈希和头文件数量
- **未命中时**：输出具体原因（源码哈希变化 / 编译选项签名变化 / 某个头文件哈希变化 / 缓存记录缺失）

示例输出：
```
[ezmk]   [cached] src/utils.cpp  (source hash matches, 5 headers unchanged)
[ezmk]   cache miss: header hash changed — include/foo.h
[ezmk]   Compiling src/main.cpp...
[ezmk]     cmd: g++ -std=c++17 -c "src/main.cpp" -o ".ezmk/temp/main.o" ...
```

## 增量构建的原子性
构建过程中若中途失败，不应破坏缓存一致性。做法：
- 编译新目标文件时先写入临时文件（如 `.tmp.o`），编译成功后再原子替换旧文件。
- 更新 `record.json` 时先写临时文件再 `rename`。

> **为什么要"临时文件 + rename"？** `rename` 是原子的，读者永远不会看到写了一半的 `.o` 或
> `record.json`。否则构建中途失败会留下被截断的记录，被后续构建误当作有效缓存。

## 缓存工作流程

1. 首次 `ezmk build`：
   - 扫描 `src/`，对 `main.cpp` 编译，生成 `main.o`，记录依赖头文件及哈希到 `record.json`。
   - 同样处理其他源文件。
   - 链接生成可执行文件。

2. 修改 `include/foo.h`（如改变一个宏）：
   - 再次 `ezmk build`，遍历源文件，发现 `main.cpp` 的依赖头文件 `foo.h` 哈希改变 → 重新编译 `main.cpp`。
   - 其他未依赖 `foo.h` 的源文件（如 `utils.cpp`）仍命中缓存。
   - 重新链接。

3. 执行 `ezmk clean`：
   - 删除 `.ezmk/cache/` 目录及 `record.json`。
   - 下次构建全量编译。

---

## 实现说明

`record.json` 的读写使用 [nlohmann/json](https://github.com/nlohmann/json) 库（单头文件 `include/vendor/nlohmann_json.hpp`），替代了早期版本的手写 JSON 解析器。原子写入策略不变：先序列化到 `.tmp` 文件，再 `rename` 覆盖。