-- ezmk-cc: compile_commands.json generator
-- Generates a clangd-compatible JSON Compilation Database for the current project.
--
-- Usage:
--   ezmk utils cc                  # output to <project_root>/compile_commands.json
--   ezmk utils cc -o <path>        # output to custom path
--   ezmk utils cc --help           # show this help

function help()
    return [[
usage: ezmk utils cc [options]

Generate compile_commands.json for the current project.

The output follows the clangd JSON Compilation Database specification:
  https://clang.llvm.org/docs/JSONCompilationDatabase.html

Options:
  -o, --output  <path>   Output path (default: <project_root>/compile_commands.json)
  -h, --help             Show this help
]]
end

function run(args)
    -- Parse arguments
    local output_path = nil
    local i = 1
    while i <= #args do
        local a = args[i]
        if a == "-o" or a == "--output" then
            i = i + 1
            if i <= #args then
                output_path = args[i]
            else
                error("-o/--output requires a path argument")
            end
        elseif a == "-h" or a == "--help" then
            print(help())
            return 0
        end
        i = i + 1
    end

    local root = ezmk.project_root()

    -- Default output path
    if not output_path then
        output_path = root .. "/compile_commands.json"
    elseif output_path:sub(1, 1) ~= "/" and output_path:sub(2, 1) ~= ":" then
        output_path = root .. "/" .. output_path
    end

    -- Collect project info
    local sources = ezmk.list_sources()
    if #sources == 0 then
        ezmk.warn("no source files found in src/")
        return 0
    end

    local include_dirs = ezmk.include_dirs()
    local compile_flags = ezmk.compile_flags()

    -- Build include flags
    local inc_flags = ""
    for _, d in ipairs(include_dirs) do
        inc_flags = inc_flags .. " -I" .. d
    end

    -- Build other compile flags (skip -I)
    local cflags = ""
    for _, f in ipairs(compile_flags) do
        if f:sub(1, 2) ~= "-I" then
            cflags = cflags .. " " .. f
        end
    end

    -- Determine language standard
    local std_flag = "-std=c++17"
    for _, f in ipairs(compile_flags) do
        if f:match("^-std=") then
            std_flag = f
            break
        end
    end

    -- Determine compiler from project config
    local compiler = "g++"
    local config_path = root .. "/ezmk.toml"
    if ezmk.file_exists(config_path) then
        local content = ezmk.file_read(config_path)
        -- Simple TOML parsing: find language in [project] section
        local lang = content:match('language%s*=%s*"([^"]*)"')
        if not lang then
            lang = content:match("language%s*=%s*'([^']*)'")
        end
        if lang and lang:match("^C$") then
            compiler = "gcc"
        end
    end

    -- Build compilation database entries
    local entries = {}
    for _, src in ipairs(sources) do
        -- Make path relative to project root
        local rel_src = src
        if rel_src:sub(1, #root) == root then
            rel_src = rel_src:sub(#root + 2)
        end

        -- Object file path
        local obj_name = rel_src:gsub("%.[^.]*$", ".o")
        local obj_path = root .. "/build/" .. obj_name

        -- Full compile command
        local cmd = compiler .. " " .. std_flag .. inc_flags .. cflags
        cmd = cmd .. " -c " .. src .. " -o " .. obj_path

        entries[#entries + 1] = {
            directory = root,
            command = cmd,
            file = rel_src
        }
    end

    -- Write output
    local json_str = ezmk.json_encode(entries)
    local ok, err = ezmk.file_write(output_path, json_str)
    if not ok then
        error("failed to write " .. output_path .. ": " .. (err or "unknown error"))
    end

    ezmk.info("Generated " .. output_path .. " (" .. #entries .. " source(s))")
    return 0
end
