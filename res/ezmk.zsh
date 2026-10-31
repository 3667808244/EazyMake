#compdef ezmk

# EazyMake zsh completion — 1.1.0-pre.2+
# Install:
#   System-wide:  cp res/ezmk.zsh /usr/share/zsh/site-functions/_ezmk
#   User-local:   mkdir -p ~/.zsh/completions && cp res/ezmk.zsh ~/.zsh/completions/_ezmk
#                 Then add to ~/.zshrc: fpath=(~/.zsh/completions $fpath)
#   Restart shell or run: autoload -Uz compinit && compinit

# ===================================================================
# Dynamic helpers
# ===================================================================

# List build profiles from current project's ezmk.toml
_ezmk_profiles() {
    local -a profiles
    if [[ -f ezmk.toml ]]; then
        profiles=(${(f)"$(grep -E '^\s*\[compile\.profile\.' ezmk.toml 2>/dev/null | sed 's/.*\.profile\.//;s/\]//')"})
    fi
    _describe -t profiles 'profile' profiles
}

# List installed package names from .ezmk/pkg/
_ezmk_installed_pkgs() {
    local -a pkgs
    local pkg_dir=".ezmk/pkg"
    if [[ -d "$pkg_dir" ]]; then
        pkgs=(${(f)"$(find "$pkg_dir" -maxdepth 1 -mindepth 1 -type d -exec basename {} \; 2>/dev/null)"})
    fi
    _describe -t packages 'installed package' pkgs
}

# List registered repo names from the registry (list.json since 1.4.5, list.toml before).
# NOTE: this script is installed statically into fpath — it does not update with
# the binary, so BOTH formats must be understood: an older project may still
# carry list.toml, and a 1.4.5+ project writes list.json.
_ezmk_repo_names() {
    local -a repos
    if [[ -f ".ezmk/repo/list.json" ]]; then
        repos=(${(f)"$(grep -E '"name"\s*:' ".ezmk/repo/list.json" 2>/dev/null | sed 's/.*"name"[[:space:]]*:[[:space:]]*"\(.*\)".*/\1/')"})
    elif [[ -f ".ezmk/repo/list.toml" ]]; then
        repos=(${(f)"$(grep -E '^name\s*=\s*"' ".ezmk/repo/list.toml" 2>/dev/null | sed 's/.*"\(.*\)".*/\1/')"})
    fi
    _describe -t repos 'registered repo' repos
}

# List installed utils tool names
_ezmk_utils_tools() {
    local -a tools
    local pkg_dir=".ezmk/pkg"
    if [[ -d "$pkg_dir" ]]; then
        tools=(${(f)"$(find "$pkg_dir" -path '*/utils/*.lua' -type f 2>/dev/null | sed 's|.*/utils/||;s|\.lua$||' | sort -u)"})
    fi
    _describe -t tools 'utils tool' tools
}

# ===================================================================
# Top-level commands
# ===================================================================

_ezmk_commands() {
    local -a commands
    commands=(
        # 1.1.0-pre.1: top-level aliases (daily build commands)
        'build:Incremental build (alias: project build)'
        'run:Build and execute (alias: project run)'
        'clean:Remove cache and temp files (alias: project clean)'
        'watch:Watch mode — auto rebuild on changes (alias: project watch)'
        'install:Install build artifacts (alias: project install)'
        'test:Build and run project tests (alias: project test)'
        'pack:Create distributable package (alias: project pack)'
        # group commands
        'project:Project management (new, build, run, watch, clean, install, pack, test, cc, export, import)'
        'pkg:Package management (install, remove, search, info, list, update)'
        'repo:Repository management (add, remove, update, list, info)'
        'workspace:Workspace commands (list, build, test, watch, clean, scan)'
        'example:Scaffold a built-in example (1.2.3+)'
        'utils:Run Lua-based utility tools'
        'version:Show version information'
        'help:Show help message'
    )
    _describe -t commands 'ezmk command' commands
}

# ===================================================================
# project subcommands
# ===================================================================

_ezmk_project() {
    local -a subcmds
    subcmds=(
        'new:Create a new project'
        'build:Incremental build'
        'run:Build and execute'
        'watch:Watch mode — auto rebuild on changes'
        'clean:Remove cache and temp files'
        'install:Install build artifacts to prefix'
        'pack:Create distributable package'
        'test:Build and run project tests'
        'cc:Generate compile_commands.json for clangd'
        'export:Export CMakeLists.txt or .vscode/ debug config'
        'import:Import a CMake project into ezmk.toml'
    )
    _describe -t subcommands 'project subcommand' subcmds
}

_ezmk_project_new() {
    _arguments \
        '--type[Project type]:type:(executable static shared utils)' \
        '--disable-git-init[Skip git init]' \
        '--disable-gitignore[Skip .gitignore creation]' \
        '1:project name:'
}

_ezmk_project_build() {
    _arguments \
        '(--verbose)-v[Verbose output]' \
        '--verbose[Verbose output]' \
        '(-j)--jobs[Max parallel compile jobs]:jobs:' \
        '-j[Max parallel compile jobs (short)]:jobs:' \
        '--disable-cache[Force recompilation of all sources]' \
        '--profile[Build profile]:profile:_ezmk_profiles' \
        '--auto-update[Auto-update repo indices before build]'
}

_ezmk_project_run() {
    _arguments \
        '(--verbose)-v[Verbose output]' \
        '--verbose[Verbose output]' \
        '(-j)--jobs[Max parallel compile jobs]:jobs:' \
        '-j[Max parallel compile jobs (short)]:jobs:' \
        '--disable-cache[Force recompilation of all sources]' \
        '--profile[Build profile]:profile:_ezmk_profiles' \
        '--auto-update[Auto-update repo indices before build]' \
        '*:::program args:_normal'
}

_ezmk_project_watch() {
    _arguments \
        '(--verbose)-v[Verbose output]' \
        '--verbose[Verbose output]' \
        '(-j)--jobs[Max parallel compile jobs]:jobs:' \
        '-j[Max parallel compile jobs (short)]:jobs:' \
        '--disable-cache[Force recompilation of all sources]' \
        '--profile[Build profile]:profile:_ezmk_profiles' \
        '--auto-update[Auto-update repo indices before build]' \
        '--no-build-on-start[Skip initial build on watch start]'
}

_ezmk_project_clean() {
    # No arguments
}

# 1.1.0+: project install
_ezmk_project_install() {
    _arguments \
        '(--verbose)-v[Verbose output]' \
        '--verbose[Verbose output]' \
        '--prefix[Install prefix directory]:directory:_files -/' \
        '--dry-run[Show what would be installed without doing it]' \
        '--no-headers[Skip header installation]' \
        '--no-data[Skip data file installation]'
}

# 1.1.0-dev.2: project pack
_ezmk_project_pack() {
    _arguments \
        '(--verbose)-v[Verbose output]' \
        '--verbose[Verbose output]' \
        '--output[Output directory]:directory:_files -/'
}

# 1.1.0-dev.6: project test
_ezmk_project_test() {
    _arguments \
        '(-f)--framework[Test framework]:framework:(catch2 ezmk)' \
        '-f[Test framework (short)]:framework:(catch2 ezmk)' \
        '--filter[Pattern to filter test cases]:filter:' \
        '(-V)--verbose[Verbose test output]' \
        '-V[Verbose test output (short)]'
}

# 1.5.0-pre.1: project cc / export / import were missing from completion.
_ezmk_project_cc() {
    _arguments \
        '--output[Output path]:file:_files' \
        '-o[Output path (short)]:file:_files' \
        '--profile[Build profile]:profile:_ezmk_profiles'
}

_ezmk_project_export() {
    _arguments \
        '--overwrite[Overwrite existing generated files]' \
        '--profile[Build profile]:profile:_ezmk_profiles' \
        '1:target:(cmake vscode)'
}

_ezmk_project_import() {
    _arguments \
        '--from[Source format]:format:(cmake)' \
        '--overwrite[Overwrite an existing ezmk.toml]'
}

_ezmk_workspace() {
    local -a subcmds
    subcmds=(
        'list:List workspace members'
        'build:Build all members'
        'test:Run member tests'
        'watch:Watch all members'
        'clean:Clean member caches'
        'scan:Adopt existing projects into a workspace'
    )
    _describe -t subcommands 'workspace subcommand' subcmds
}

_ezmk_workspace_build() {
    _arguments \
        '-j[Max parallel members]:jobs:' \
        '--jobs[Max parallel members]:jobs:' \
        '--stop-on-error[Stop after the first failure]' \
        '--member[Only this member and its dependency closure]:member:'
}

_ezmk_workspace_test() {
    _ezmk_workspace_build
}

_ezmk_workspace_watch() {
    _arguments \
        '-j[Max parallel members]:jobs:' \
        '--jobs[Max parallel members]:jobs:' \
        '--stop-on-error[Stop after the first failure]' \
        '--member[Only this member and its dependency closure]:member:' \
        '--run[Run the rebuilt executable]'
}

_ezmk_workspace_clean() {
    _arguments '--member[Only this member]:member:'
}

_ezmk_workspace_scan() {
    _arguments \
        '--dry-run[Show what would be written]' \
        '(-y)--yes[Skip confirmation prompts]' \
        '-y[Skip confirmation prompts (short)]' \
        '1:directory:_files -/'
}

# ===================================================================
# pkg subcommands
# ===================================================================

_ezmk_pkg() {
    local -a subcmds
    subcmds=(
        'install:Install a package'
        'remove:Remove a package'
        'search:Search for a package'
        'info:Show package details'
        'list:List installed packages'
        'update:Update installed package(s)'
    )
    _describe -t subcommands 'pkg subcommand' subcmds
}

_ezmk_pkg_install() {
    _arguments \
        '(-p -u -g)-p[Project scope]' \
        '(-p -u -g)-u[User scope]' \
        '(-p -u -g)-g[Global scope]' \
        '--sha256[Verify against SHA-256]:hash:' \
        '(-y)--yes[Skip interactive prompts]' \
        '(-y)-y[Skip interactive prompts (short)]' \
        '--locked[Use locked versions from ezmk.lock.json]' \
        '--no-lock[Do not write ezmk.lock.json]' \
        '1:package file, URL, or name:_files'
}

_ezmk_pkg_remove() {
    _arguments \
        '(-p)-p[Project scope only]' \
        '(-u)-u[User scope only]' \
        '(-g)-g[Global scope only]' \
        '1:package name:_ezmk_installed_pkgs'
}

_ezmk_pkg_search() {
    _arguments \
        '(-p)-p[Project scope only]' \
        '(-u)-u[User scope only]' \
        '(-g)-g[Global scope only]' \
        '1:package name:'
}

_ezmk_pkg_info() {
    _arguments \
        '(-p)-p[Project scope only]' \
        '(-u)-u[User scope only]' \
        '(-g)-g[Global scope only]' \
        '1:package name:_ezmk_installed_pkgs'
}

_ezmk_pkg_list() {
    _arguments \
        '(-p)-p[Project scope only]' \
        '(-u)-u[User scope only]' \
        '(-g)-g[Global scope only]'
}

_ezmk_pkg_update() {
    _arguments \
        '(-p)-p[Project scope only]' \
        '(-u)-u[User scope only]' \
        '(-g)-g[Global scope only]' \
        '--all[Update all installed packages]' \
        '1:package name:_ezmk_installed_pkgs'
}

# ===================================================================
# repo subcommands
# ===================================================================

_ezmk_repo() {
    local -a subcmds
    subcmds=(
        'add:Register and clone a repository'
        'remove:Unregister and delete cache'
        'update:Git pull to refresh'
        'list:List registered repositories'
        'info:Show repository details'
    )
    _describe -t subcommands 'repo subcommand' subcmds
}

_ezmk_repo_add() {
    _arguments \
        '(-p -u -g)-p[Project scope]' \
        '(-p -u -g)-u[User scope]' \
        '(-p -u -g)-g[Global scope]' \
        '--name[Repository name override]:name:' \
        '--branch[Git branch to track]:branch:("main" "master" "stable" "develop")' \
        '1:git URL or local path:_files -/'
}

_ezmk_repo_remove() {
    _arguments \
        '(-p)-p[Project scope only]' \
        '(-u)-u[User scope only]' \
        '(-g)-g[Global scope only]' \
        '1:repo name:_ezmk_repo_names'
}

_ezmk_repo_update() {
    _arguments \
        '(-p)-p[Project scope only]' \
        '(-u)-u[User scope only]' \
        '(-g)-g[Global scope only]' \
        '1:repo name:_ezmk_repo_names'
}

_ezmk_repo_list() {
    _arguments \
        '(-p)-p[Project scope only]' \
        '(-u)-u[User scope only]' \
        '(-g)-g[Global scope only]'
}

_ezmk_repo_info() {
    _arguments \
        '(-p)-p[Project scope only]' \
        '(-u)-u[User scope only]' \
        '(-g)-g[Global scope only]' \
        '1:repo name:_ezmk_repo_names'
}

# ===================================================================
# utils
# ===================================================================

_ezmk_utils() {
    _arguments \
        '1:tool name:_ezmk_utils_tools' \
        '*:::tool args:_normal'
}

# ===================================================================
# Global options (1.5.0-pre.1)
# ===================================================================

_ezmk_global_options() {
    _arguments -s \
        '--config[Use FILE instead of the user-level config.toml]:file:_files' \
        '--no-config[Skip global and user config.toml]' \
        '--lang[UI language tag]:tag:(en zh zh-TW)' \
        '--color[Color output mode]:mode:(always auto never enable default disable on off 16colors 256colors truecolor)'
}

# ===================================================================
# Main dispatch
# ===================================================================

_ezmk() {
    local curcontext="$curcontext" state line
    typeset -A opt_args

    # 1.5.0-pre.1: global config / language / color options.
    if [[ "${words[2]}" == --* ]]; then
        _ezmk_global_options
        return
    fi

    if (( CURRENT == 2 )); then
        _ezmk_commands
        return
    fi

    local cmd=${words[2]}
    local subcmd

    case "$cmd" in
        # 1.1.0-pre.1: top-level aliases — dispatch directly to project completions
        build)
            _ezmk_project_build
            ;;
        run)
            _ezmk_project_run
            ;;
        clean)
            _ezmk_project_clean
            ;;
        watch)
            _ezmk_project_watch
            ;;
        install)
            _ezmk_project_install
            ;;
        test)
            _ezmk_project_test
            ;;
        pack)
            _ezmk_project_pack
            ;;
        project)
            if (( CURRENT == 3 )); then
                _ezmk_project
            else
                subcmd=${words[3]}
                case "$subcmd" in
                    new)     _ezmk_project_new ;;
                    build)   _ezmk_project_build ;;
                    run)     _ezmk_project_run ;;
                    watch)   _ezmk_project_watch ;;
                    clean)   _ezmk_project_clean ;;
                    install) _ezmk_project_install ;;
                    pack)    _ezmk_project_pack ;;
                    test)    _ezmk_project_test ;;
                    cc)      _ezmk_project_cc ;;
                    export)  _ezmk_project_export ;;
                    import)  _ezmk_project_import ;;
                    *)       _ezmk_project ;;
                esac
            fi
            ;;
        pkg)
            if (( CURRENT == 3 )); then
                _ezmk_pkg
            else
                subcmd=${words[3]}
                case "$subcmd" in
                    install) _ezmk_pkg_install ;;
                    remove)  _ezmk_pkg_remove ;;
                    search)  _ezmk_pkg_search ;;
                    info)    _ezmk_pkg_info ;;
                    list)    _ezmk_pkg_list ;;
                    update)  _ezmk_pkg_update ;;
                    *)       _ezmk_pkg ;;
                esac
            fi
            ;;
        repo)
            if (( CURRENT == 3 )); then
                _ezmk_repo
            else
                subcmd=${words[3]}
                case "$subcmd" in
                    add)    _ezmk_repo_add ;;
                    remove) _ezmk_repo_remove ;;
                    update) _ezmk_repo_update ;;
                    list)   _ezmk_repo_list ;;
                    info)   _ezmk_repo_info ;;
                    *)      _ezmk_repo ;;
                esac
            fi
            ;;
        workspace)
            if (( CURRENT == 3 )); then
                _ezmk_workspace
            else
                subcmd=${words[3]}
                case "$subcmd" in
                    list)  ;;
                    build) _ezmk_workspace_build ;;
                    test)  _ezmk_workspace_test ;;
                    watch) _ezmk_workspace_watch ;;
                    clean) _ezmk_workspace_clean ;;
                    scan)  _ezmk_workspace_scan ;;
                    *)     _ezmk_workspace ;;
                esac
            fi
            ;;
        example)
            _arguments '1:example name:(hello greeter with-hooks cmake-interop with-tests with-packages list)' '-o[Output directory]:directory:_files -/'
            ;;
        utils)
            _ezmk_utils
            ;;
        version|help)
            ;;
        *)
            _ezmk_commands
            ;;
    esac
}

_ezmk "$@"
