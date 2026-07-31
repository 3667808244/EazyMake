-- ezmk-official-utils: link — .ezmk/links.json management tool
--
-- Usage:
--   ezmk utils link add    <name> <path>     # Add a link
--   ezmk utils link remove <name>            # Remove a link
--   ezmk utils link list                     # List all links
--   ezmk utils link show   <name>            # Show link details
--
-- Link names must contain only [A-Za-z0-9_-].
-- Paths must be relative (no absolute paths) to preserve project portability.

local LINKS_DIR = ".ezmk"
local LINKS_FILE = LINKS_DIR .. "/links.json"

function help()
    return [[
usage: ezmk utils link <subcommand> [args]

Manage .ezmk/links.json for cross-directory source sharing.

Subcommands:
  ezmk utils link add    <name> <path>     Add a new link
  ezmk utils link remove <name>            Remove an existing link
  ezmk utils link list                     List all links
  ezmk utils link show   <name>            Show details for a link

Link names may only contain letters, digits, underscores, and hyphens.
Paths must be relative (e.g. "../common/src") — absolute paths are not
supported to keep the project portable across machines.

Examples:
  ezmk utils link add shared ../common-lib/src
  ezmk utils link list
  ezmk utils link show shared
  ezmk utils link remove shared
]]
end

-- Load links.json, return empty table if not found
local function load_links(root)
    local path = root .. "/" .. LINKS_FILE
    if not ezmk.file_exists(path) then
        return {}
    end
    local content = ezmk.file_read(path)
    if content == nil or content == "" then
        return {}
    end
    local ok, links = pcall(ezmk.json_decode, content)
    if not ok then
        error("failed to parse " .. LINKS_FILE .. ": " .. tostring(links))
    end
    -- json_decode may return a list for empty array; ensure we have a table
    if type(links) ~= "table" then
        return {}
    end
    return links
end

-- Save links to links.json
local function save_links(root, links)
    local ok, json_str = pcall(ezmk.json_encode, links)
    if not ok then
        error("failed to encode links: " .. tostring(json_str))
    end
    -- Ensure .ezmk/ directory exists
    local dir = root .. "/" .. LINKS_DIR
    local ok2 = pcall(ezmk.make_directory, dir)
    -- make_directory may error if it doesn't exist as a Lua API; we ignore
    local ok3, err = ezmk.file_write(root .. "/" .. LINKS_FILE, json_str)
    if not ok3 then
        error("failed to write " .. LINKS_FILE .. ": " .. (err or "unknown error"))
    end
end

-- Validate link name: [A-Za-z0-9_-]+
local function validate_name(name)
    if name == nil or name == "" then
        error("link name cannot be empty")
    end
    if name:match("[^%w_%-]") then
        error("link name '" .. name .. "' contains invalid characters (use only A-Z, a-z, 0-9, _, -)")
    end
end

-- Validate link path: relative only, no .. traversal
local function validate_path(root, path)
    if path == nil or path == "" then
        error("link path cannot be empty")
    end
    -- Absolute paths not allowed
    if path:sub(1, 1) == "/" or path:sub(2, 2) == ":" then
        error("absolute paths are not allowed in links (use relative paths for portability)")
    end
    -- Check that the target exists
    local full = root .. "/" .. path
    if not ezmk.file_exists(full) then
        ezmk.warn("link target does not exist: " .. path)
    end
end

function run(args)
    local root = ezmk.project_root()
    if root == nil or root == "" then
        error("could not determine project root")
    end

    if #args == 0 then
        print(help())
        return 0
    end

    local cmd = args[1]
    local links = load_links(root)

    if cmd == "add" then
        if #args < 3 then
            error("usage: ezmk utils link add <name> <path>")
        end
        local name = args[2]
        local path = args[3]
        validate_name(name)
        validate_path(root, path)
        links[name] = path
        save_links(root, links)
        ezmk.info("link '" .. name .. "' -> '" .. path .. "' added")
        return 0

    elseif cmd == "remove" then
        if #args < 2 then
            error("usage: ezmk utils link remove <name>")
        end
        local name = args[2]
        if links[name] == nil then
            ezmk.warn("link '" .. name .. "' not found in " .. LINKS_FILE)
            return 0
        end
        links[name] = nil
        save_links(root, links)
        ezmk.info("link '" .. name .. "' removed")
        return 0

    elseif cmd == "list" then
        local count = 0
        for name, path in pairs(links) do
            count = count + 1
        end
        if count == 0 then
            ezmk.info("no links defined in " .. LINKS_FILE)
            return 0
        end
        ezmk.info("links (" .. count .. "):")
        -- Sort for deterministic output
        local sorted = {}
        for name, path in pairs(links) do
            sorted[#sorted + 1] = {name = name, path = path}
        end
        table.sort(sorted, function(a, b) return a.name < b.name end)
        for _, entry in ipairs(sorted) do
            local full = root .. "/" .. entry.path
            local status = ezmk.file_exists(full) and "" or " [missing]"
            print("  " .. entry.name .. " -> " .. entry.path .. status)
        end
        return 0

    elseif cmd == "show" then
        if #args < 2 then
            error("usage: ezmk utils link show <name>")
        end
        local name = args[2]
        local path = links[name]
        if path == nil then
            error("link '" .. name .. "' not found in " .. LINKS_FILE)
        end
        print("Name: " .. name)
        print("Path: " .. path)
        local full = root .. "/" .. path
        if ezmk.file_exists(full) then
            print("Status: exists")
            -- Check if it's a directory and list contents
            -- ezmk.file_exists works for both files and dirs
            -- Try to get a summary
            print("Full path: " .. full)
        else
            print("Status: target not found")
        end
        return 0

    else
        error("unknown subcommand '" .. cmd .. "'. Use 'ezmk utils link' for help.")
    end
end
