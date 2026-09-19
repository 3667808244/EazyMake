# EazyMake 1.4.3 — 离线 man 手册 + 防漂移闸门


为 `ezmk` 补上 Unix 惯例的离线手册：手写 man 页 + 构建期防漂移校验 + 三渠道分发。**零 CLI 行为变更、零功能新增、公共 API 无破坏性变更**（唯一 CLI 相关改动是 `ezmk help` 末行新增一行 See also 文案）。

### 新增

- **手册页（`man/`，手写 roff，共 4 页）**：`ezmk(1)`（12 节：命令表、选项组、scope flags、28 个简写、GNU 选项语法、环境变量、退出码、FILES、SEE ALSO）、`ezmk.toml(5)`（13 个小节字段表 + 版本约束 + 确定性构建 + 示例）、`ezmk-workspace.toml(5)`（`[workspace]` / `[workspace.options]` 字段表、成员依赖与产物注入规则）与 `ezmk-lua(1)`（独立钩子运行时：选项、`run(ctx)` 契约、无沙箱环境的边界与退出码）。仅英文，定位为"精简速查"，细节仍以 `docs/` 为准。
- **防漂移闸门 `scripts/check_man_sync.py`（零依赖 Python 3）**：从 `src/cli.cpp` / `src/config.cpp` / `src/workspace.cpp` / `src/ezmk_lua_main.cpp` / `src/*.cpp` 提取长/短选项、简写、顶层别名、命令、环境变量、配置键、workspace 键与 `ezmk-lua` 选项，与 4 份 man 双向比对；含"提取基线断言"（提取数低于基线即失败并提示更新提取器），环境变量做三分法（文档化 / 内部 / 未归类即失败）。`CONTRIBUTING.md` 新增「Man pages」流程。
- **CI 闸门**：ubuntu job 增装 `groff man-db`，新增静态 lint（`groff -z -ww` + CRLF / 未转义 `-` / 裸 `^`~` / 宏参数 `\\` / `\"` / 标签内裸引号 6 条 grep 陷阱）与 CLI 漂移检查，插在构建前快速失败；新增 `man pages (1.4.3)` job（渲染 4 页 + 分发断言 + 安装冒烟：临时 `PREFIX` 后 `man ezmk` / `man 1 ezmk-lua` / `man 5 ezmk.toml` / `man 5 ezmk-workspace.toml` 均可命中）。
- **三渠道分发**：`install.sh` 安装 4 页到 `$PREFIX/share/man/man{1,5}`（新增 `EZMK_NO_MAN=1` 跳过，非标准前缀打印 `MANPATH` 提示）；`publish/arch/PKGBUILD` 四条 `install -Dm644 man/…`；Release 的 linux-x64 / macos-arm64 资产含 `man/`（Windows zip 不含），`publish/homebrew/ezmk.rb` 经四条 `man1.install` / `man5.install` 安装。

### 行为变更

- **`ezmk help` 末行新增 See also**：`See also: man ezmk (man 5 ezmk.toml for the config file)`（新 i18n 键 `help_see_also_man`，405 → 406 键，en / zh 双语，`zh-TW` 按变体惯例继承）。非公共 API，无需迁移。

### 文档

- `README.md` / `README_ZH.md`（安装选项表与自定义说明补 `EZMK_NO_MAN`）、`docs/{en,zh}/cli.md`（安装节补手册页位置、环境变量表补 `EZMK_NO_MAN`）、`docs/{en,zh}/technical.md`（新增 Man Pages 一节：路径、三渠道、`MANPATH` 提示、离线定位、防漂移）、`CONTRIBUTING.md`（man 改动流程 + `.TH` 日期随发布 commit 更新）。

### 已知限制

- **仅英文**：man 不做 i18n（`EZMK_LANG` 不影响手册页）；`man/zh_CN` 等多语言留待后续评估。
- **Windows 无 man**：原生 Windows 无 `man` 命令，`install.ps1` 与 Windows 压缩包不含手册页；MSYS2 用户经 `install.sh` 获得。
- **无 `ezmk man` 子命令**：CLI 面保持不变。
