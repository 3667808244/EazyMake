-- with-hooks 示例 — 链接成功后钩子（教程 07「监视模式与钩子」）。
-- ctx.output 为构建产物路径；ctx.profile 为当前 profile（无则为空）。
function run(ctx)
    ezmk.info("post_build hook: 构建产物 " .. ctx.output)
    if ctx.profile ~= "" then
        ezmk.info("post_build hook: profile = " .. ctx.profile)
    end
end
