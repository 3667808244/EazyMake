# 包预编译缓存

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
   - 编译命令（如 `[compile] flags`）与上次记录相同？  
     （编译选项影响输出，必须作为 key 的一部分）
4. **检查所有依赖的头文件**：
   - 遍历上次记录的每个头文件路径，计算当前该头文件的哈希，与上次记录的哈希比较。
   - 若任一哈希改变或头文件路径集合发生变化（增/删），则失效。
5. **判定结果**：
   - 若全部匹配 → **命中缓存**，直接使用已有的 `.o` 文件，不重新编译。
   - 否则 → **缓存失效**，重新编译源文件，并更新 `record.json` 和 `.o` 文件。

### 流程图

```
对每个源文件:
    cur_source_hash = hash(file)
    cur_compile_opts = get_current_flags()
    从 record.json 读取 last_entry
    if last_entry 存在 
        && last_entry.source_hash == cur_source_hash
        && last_entry.compile_opts == cur_compile_opts:
        # 检查头文件
        all_header_match = true
        for each (hdr, last_hash) in last_entry.headers:
            cur_hash = hash(hdr)
            if cur_hash != last_hash:
                all_header_match = false; break
        if all_header_match && header_set_size_equal:
            # 命中缓存
            continue
    # 未命中：重新编译
    compile source -> .o
    generate .d 获取头文件列表及哈希
    更新 record.json
```

## 缓存记录结构（`record.json`）

```json
{
  "version": 1,
  "compile_options_signature": "sha256_of_flags_and_env",  // 全局编译选项指纹
  "files": {
    "src/main.cpp": {
      "source_hash": "a3f5c9...",
      "object_file": ".ezmk/cache/obj/main.o",
      "compiler": "g++",
      "compile_opts": ["-Wall", "-O2"],   // 可与全局指纹冗余，便于调试
      "dependencies": [
        {"path": "include/foo.h", "hash": "b4e8d2..."},
        {"path": "/usr/include/iostream", "hash": "c6a0b1..."}
      ],
      "last_build_time": "2025-03-01T12:34:56Z"
    },
    "src/utils.cpp": { ... }
  }
}
```

### 字段说明
- `version`：用于缓存格式演进。
- `compile_options_signature`：全局编译选项（如 `[compile] flags`、环境变量 `CXXFLAGS`）的整体哈希，任一变化导致所有源文件缓存失效（安全但较粗）。更细粒度可与每个条目的 `compile_opts` 结合。
- `object_file` 相对路径。
- 系统头文件（如 `/usr/include/iostream`）的哈希也需记录，因为系统头文件升级可能改变编译结果。

## 缓存一致性维护

### 头文件更新后自动触发
依赖头文件哈希比较机制天然保证：修改任何头文件，引用它的所有源文件都会重新编译。

### 编译选项变更
- 全局标志变化 → 清空所有缓存条目（简单实现）或逐条比较 `compile_opts`（复杂）。
- 推荐：在 `record.json` 中存储全局标志指纹，构建前对比当前指纹，不同则丢弃整个缓存，重新全量编译。

### 文件移动/重命名
- 若源文件路径改变，旧缓存条目保留但不会被命中（因为构建扫描路径产生新条目），用户可通过 `ezmk clean` 手动清理。
- 头文件路径变更：旧记录中的路径失效，编译时会因缺失头文件而报错，用户需确保路径正确。

## 缓存禁用

`ezmk build --disable-cache`：
- 忽略 `record.json`，所有源文件重新编译。
- 编译完成后**覆盖**更新缓存（这样下次启用缓存时可受益）。也可选择不更新，但更新更合理。

## 增量构建的原子性
构建过程中若中途失败，不应破坏缓存一致性。做法：
- 编译新目标文件时先写入临时文件（如 `.tmp.o`），编译成功后再原子替换旧文件。
- 更新 `record.json` 时先写临时文件再 `rename`。

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