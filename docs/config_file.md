# 配置文件`ezmk.toml`

---

## `project` 节

 - `name` : 项目名称
 - `type` : 项目类型(目前仅支持`"executable"`,未来可能支持`"static"`和`"shared"`)

---

## `compile` 节

 - `flags` : 编译时添加的标志 
 - `include_dir` : 编译时的包含目录

---

## `link` 节

 - `flags` : 链接时添加的标志
 - `systam_target` : 需要链接的系统库

--- 

## `depends` 节

 - `lib` : 依赖的库名