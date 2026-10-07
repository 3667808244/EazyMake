# EazyMake 1.4.9 — 构建钩子 profile 解析修复 + 按 profile 导出对象归档

本版修掉一个构建钩子缺陷，并新增按 profile 导出对象归档的能力（含默认模板启用），**零命令新增、公共 API 无破坏性变更**。

### 修复

- **构建钩子的 `ctx.profile` 现在返回解析后的生效 profile**：`pre_build` / `post_build` / `on_failure` 此前只拿到 CLI 原始 `--profile`，未回填 `[compile].default_profile`——未传 `-p` 时钩子看到空串，而实际按默认 profile 编译。现在与 `project export cmake` 传给 `ezmk-lua` 的值一致。
- **行为提醒**：若既有钩子用 `ctx.profile == ""` 判断“未指定 profile”，请改为判断具体文件名。

### 新功能

- **`[compile.profile.<name>].export_objs`**：该 profile 的构建在编译成功后、链接之前，把项目全部源文件对象打包为单个归档。`true` → 默认 `build/obj_files.zip`；字符串路径的后缀决定格式（`.zip` / `.tar.gz` / `.tgz`），非法后缀配置期报错。归档条目镜像源码目录结构（`src/main.o`），且只收录本次编译的对象（不含已删除源文件的陈旧对象）。
- **默认模板**：`ezmk project new` / `project import` 生成的 `release` profile 默认 `export_objs = true`，因此 `ezmk build --profile release` 会额外产出 `build/obj_files.zip`（`default_profile = "debug"` 不变，裸构建不导出）。

**测试**：全量 **1137 用例 / 6622 断言 / 4 跳过 / 0 失败**（立项基线 1133/6541；新增 4 用例）；`check_i18n.py`（418 键）、`check_man_sync.py`、`check_docs_sync.sh` 通过，groff 4 页零告警。

**安装**：`install.sh`（Linux/macOS）、`install.ps1`（Windows）、`winget install EazyMake.EazyMake`、`brew install 3667808244/eazymake/ezmk`、`publish/arch/PKGBUILD`（Arch/MSYS2）。

**完整变更**：见 `CHANGES.md` 的 1.4.9 节；设计与执行计划见 `plans/1.4.x/1.4.9.md`。
