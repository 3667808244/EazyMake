-- with-hooks 示例 — 编译前钩子（教程 07「监视模式与钩子」）。
-- 钩子脚本必须定义 run(ctx)；ctx.project_root 为项目根目录。
function run(ctx)
    ezmk.info("pre_build hook: 开始编译 " .. ctx.project_root)
end
