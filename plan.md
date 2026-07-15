# EazyMake 0.9.3 执行计划 — 捆绑包迁移

> 来源: [`plans/0.9.3.md`](plans/0.9.3.md)（详细设计）、[`plans/README.md`](plans/README.md)（版本路线图）。
> 官方仓库: `E:\claude_workspace\ezmk-repo`（GitHub: `https://github.com/3667808244/ezmk-repo.git`）。

---

## 版本状态

```
0.9.0 (done) → 0.9.1 (done) → 0.9.2 (done) → 0.9.3 (current)
  发布正式版      默认仓库创建      文档多语言          捆绑包迁移
```

---

## 一、目标

将 7 个 `pkg/*.tar.gz` 捆绑预编译库包迁移到官方仓库（`ezmk-repo`），清理主项目冗余文件，`ezmk-cc` 保留内置。

**背景**: 捆绑包是仓库系统成熟之前的过渡方案——用户装完 ezmk 就有包可用。0.9.1 已建立官方仓库并预注册到 `install.sh`，这些包应改为通过标准的 `repo → search → install` 流程使用。

### 当前问题

| 问题                  | 说明                                                                   |
| --------------------- | ---------------------------------------------------------------------- |
| 捆绑包无版本号        | `ezmk.toml` 缺少 `version` 字段，`pkg info` 无法显示版本               |
| 旧 TOML 格式          | 使用 `include_dir`（单数）而非 `include_dirs`（复数），缺少 `src_dirs` |
| 仓库与二进制重复      | 同样的包存两份：`pkg/`（捆绑）+ `packages/`（仓库迁移后）              |
| 无源码可审计          | 只有归档，没有 `sources/` 源工程，不符合"源→归档→哈希"一致原则         |
| `install.sh` 多余拷贝 | 已预注册官方仓库，再拷贝 `pkg/` 冗余                                   |

---

## 二、仓库当前状态

`E:\claude_workspace\ezmk-repo` 已有：
- `sources/hello-lib/` + `sources/example-utils/` — 2 个示例源工程
- `packages/hello-lib-0.1.0.tar.gz` + `packages/example-utils-0.1.0.tar.gz` — 2 个归档
- `index.toml` — 含 2 个包的条目
- `pack.sh` / `validate.sh` — 打包 + 校验脚本

`E:\cpp\projects\EazyMake\pkg\` 待迁移：
```
catch2.tar.gz         (199 KB)
fmt.tar.gz            (137 KB)
lua.tar.gz            (225 KB)
nlohmann_json.tar.gz  (135 KB)
spdlog.tar.gz         (204 KB)
sqlite3.tar.gz        (2,450 KB)
tinyxml2.tar.gz       (34 KB)
ezmk-cc/              (目录 — 保留，不迁移)
```

---

## 三、待迁移包详情

| 包              | 类型     | 实际版本   | 大小     | 特殊处理                                                          |
| --------------- | -------- | ---------- | -------- | ----------------------------------------------------------------- |
| `catch2`        | `static` | **3.6.0**  | 199 KB   | header-only + 少量编译单元                                        |
| `fmt`           | `static` | 10.2.1     | 137 KB   | 成熟格式化库                                                      |
| `lua`           | `static` | 5.4.7      | 225 KB   | 纯 C，与 ezmk 内嵌同版本，`language = "C"`                        |
| `nlohmann_json` | `static` | 3.11.3     | 135 KB   | header-only + stub `.cpp`                                         |
| `spdlog`        | `static` | 1.14.1     | 204 KB   | 依赖 `fmt`（`depends.lib = ["fmt"]`），需 `-DSPDLOG_COMPILED_LIB` |
| `sqlite3`       | `static` | **3.46.0** | 2,450 KB | 单文件 amalgamation，`language = "C"`                             |
| `tinyxml2`      | `static` | **11.0.0** | 34 KB    | 仅 2 个文件（`.h` + `.cpp`）                                      |

> 版本号通过查阅各包头文件版本宏确认（CATCH_VERSION_MAJOR/MINOR/PATCH, FMT_VERSION, LUA_VERSION_*, NLOHMANN_JSON_VERSION_*, SPDLOG_VER_*, SQLITE_VERSION, TINYXML2_*_VERSION）。
> catch2/sqlite3/tinyxml2 实际版本与计划估计不同，已按实际版本打包。

### 依赖关系

```
spdlog ──→ fmt
```

其它包无互相依赖。

### 不迁移

| 名称      | 原因                                                                                          |
| --------- | --------------------------------------------------------------------------------------------- |
| `ezmk-cc` | **内置工具**，编译进 ezmk 二进制（`src/lua_api.cpp` 内置注册），`pkg/ezmk-cc/` 保留为源码参考 |

---

## 四、逐包通用修正

每个迁移包统一做以下修正：

1. **补 `version`** — 查阅上游版本号（头文件版本宏），写入 `ezmk.toml`
2. **TOML 格式更新** — `include_dir` → `include_dirs`，补 `src_dirs`
3. **补 `language`** — C 库标记 `"C99"`（ezmk 格式为 `<Lang><Version>`，不支持纯 `"C"`），C++ 库标记 `"C++17"`
4. **审查 `compile.flags`** — 去掉硬编码的 `-Wall -O2`（优化级别由用户项目控制，包不应硬编码）

---

## 五、执行步骤

### 阶段一：仓库侧 — 逐包重建源工程（7 个包）

对每个包按以下流程操作：

```
1. 解压 pkg/<name>.tar.gz 到临时目录
2. 提取源文件 → ezmk-repo/sources/<name>/
3. 查阅上游版本号（头文件版本宏）→ 确认 version
4. 更新 ezmk.toml（include_dirs, src_dirs, language, version, 清理 flags）
5. 运行 pack.sh 生成 packages/<name>-<version>.tar.gz + sha256
6. 手动审查生成的归档内容是否正确
```

**逐包执行清单**：

- [x] **catch2** — 解压 → `sources/catch2/`，确认 **3.6.0**，修正 TOML，打包
- [x] **fmt** — 解压 → `sources/fmt/`，确认 10.2.1，修正 TOML，打包
- [x] **lua** — 解压 → `sources/lua/`，确认 5.4.7，`language = "C"`，修正 TOML，打包
- [x] **nlohmann_json** — 解压 → `sources/nlohmann_json/`，确认 3.11.3，修正 TOML，打包
- [x] **spdlog** — 解压 → `sources/spdlog/`，确认 1.14.1，添加 `depends.lib = ["fmt"]` + SPDLOG 宏，打包
- [x] **sqlite3** — 解压 → `sources/sqlite3/`，确认 **3.46.0**，`language = "C"`，修正 TOML，打包
- [x] **tinyxml2** — 解压 → `sources/tinyxml2/`，确认 **11.0.0**，修正 TOML，打包

**阶段一完成标志**: 7 个包全部在 `sources/` 有源工程，`packages/` 有对应归档，`index.toml` 包含全部 9 个包条目（2 原有 + 7 新增）。

### 阶段二：仓库侧 — 校验

- [x] 运行 `validate.sh` — 确认所有包的 sha256 匹配、归档可解压（**4/4 检查通过，9 个包**）
- [x] 端到端测试：逐包 `ezmk pkg install <name> -u -y` → 编译 → 安装成功（**7/7 通过**）
- [x] `spdlog` + `fmt` 依赖链验证：`pkg install spdlog` → 自动拉取并编译 `fmt` → 成功
- [x] `pkg info <name>` 逐包验证版本号显示正确（**7/7 通过**）

### 阶段三：主项目 — 清理

- [x] 删除 `pkg/catch2.tar.gz`
- [x] 删除 `pkg/fmt.tar.gz`
- [x] 删除 `pkg/lua.tar.gz`
- [x] 删除 `pkg/nlohmann_json.tar.gz`
- [x] 删除 `pkg/spdlog.tar.gz`
- [x] 删除 `pkg/sqlite3.tar.gz`
- [x] 删除 `pkg/tinyxml2.tar.gz`
- [x] 确认 `pkg/ezmk-cc/` 目录保留（内置工具源码参考）
- [x] 更新 `install.sh`：移除捆绑包拷贝逻辑（已预注册官方仓库，不再需要拷贝 `pkg/`）

### 阶段四：文档更新

- [x] 官方仓库 `README.md` 更新包列表（从 2 个示例包 → 9 个包，含版本号、语言、依赖图）
- [x] 主项目 `README.md` / `README_ZH.md` — 检查完毕，无需修改（无捆绑包引用）
- [x] 主项目 `docs/en/cli.md` / `docs/zh/cli.md` — 检查完毕，无需修改（无捆绑包引用）
- [ ] 如有离线使用场景，在文档中给出替代方案（1.0.0 前补充）

---

## 六、兼容性矩阵

| 场景                 | 影响                                          | 处理方式                                                          |
| -------------------- | --------------------------------------------- | ----------------------------------------------------------------- |
| 新用户 `install.sh`  | `pkg/` 不再有捆绑归档                         | 已预注册官方仓库，`pkg install <name>` 自动从仓库拉取，**无感知** |
| 旧用户已安装的捆绑包 | 包已安装在 `<exe_dir>/pkg/` 或用户/项目作用域 | **不受影响**，`pkg list` 仍可见；`pkg update` 从仓库拉取新版本    |
| 离线环境             | 无网络时无法从仓库安装                        | 文档给出离线方案（`repo add` 本地镜像或手动下载归档）             |
| `ezmk utils cc`      | 内置工具，不走仓库                            | **不受影响**，始终可用                                            |

---

## 七、测试清单

- [ ] 逐包 `pkg install <name>` → 测试项目 `[depends]` 引用 → `project build` 通过
- [ ] `spdlog` 安装时自动拉取依赖 `fmt`（依赖链正确）
- [ ] `pkg info <name>` 显示版本号正确（全部 7 个包）
- [ ] SHA-256 校验全部通过（`pkg install --sha256 <hash>` 或自动校验）
- [ ] 旧版 `install.sh` 安装的用户 → 新仓库仍然可 `pkg install`（向后兼容）
- [ ] `ezmk utils cc` 在新安装下仍正常运行
- [ ] 全新 `install.sh` 安装 → `pkg list` 无捆绑包残留 → `pkg install catch2` 从仓库安装成功

---

## 八、开放问题（待确认）

1. **版本号确认** ✅ — 已通过头文件版本宏确认：catch2 3.6.0, sqlite3 3.46.0, tinyxml2 11.0.0（与原始估计有差异）。
2. **`install.sh` 过渡策略** ✅ — 已移除捆绑包拷贝，保留 `ezmk-cc/` 单独拷贝（`ezmk utils cc` 需要 Lua 脚本）。
3. **`language` 格式** ✅ — ezmk 要求 `<Lang><Version>` 格式（如 `C99`, `C++17`），不支持纯 `"C"`。lua/sqlite3 使用 `C99`。
4. **Catch2 编译单元** ✅ — 保持原样（含 `catch2_stub.cpp`），兼容既有使用方式。
5. **离线方案文档** — 推迟到 1.0.0 前补充。

---

## 九、跨版本关注点

- **向后兼容**: 捆绑包删除后，新用户通过已预注册的仓库安装，旧用户已安装的包不受影响。`pkg update` 从仓库拉取新版本。
- **安全模型**: SHA-256 校验在仓库分发中强制使用；预注册在用户作用域且显式可见（`repo list`），可关闭可撤销。
- **文档一致性**: 0.9.2 建立的 `docs/en/` ↔ `docs/zh/` 对应关系需持续维护；术语表随新功能扩展更新。
- **1.0.0 准备**: 本版本是 1.0.0 正式发布前的最后一步功能迁移，完成后即可进入版本号终局化、变更日志补全、三平台 QA 阶段。
