#### Pull Request Description

- [x] Have you signed the [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/)?
- [x] Have you checked that there aren't other open [pull requests](https://github.com/microsoft/winget-pkgs/pulls) for the same manifest update/change?
- [x] This PR only modifies one (1) manifest
- [x] Have you [validated](https://learn.microsoft.com/windows/package-manager/winget/validate) your manifest locally with `winget validate --manifest <path>`?
- [ ] Have you tested your manifest locally with `winget install --manifest <path>`?
- [x] Does your manifest conform to the [1.6 schema](https://learn.microsoft.com/en-us/windows/package-manager/winget/manifest/schema/1.6.0)?

---

**Description:**
New version: EazyMake.EazyMake version 1.4.5

- EazyMake is a simple C/C++ build tool (CLI named `ezmk`), GCC/Clang/MSVC.
- v1.4.5 unifies the format of EazyMake's generated files: the dependency lockfile is now `ezmk.lock.json` (was `ezmk.lock`) and the repository registry is now `list.json` (was `list.toml`) — both JSON and both written atomically. The pre-1.4.5 TOML files are still read and are migrated automatically on the next write (`ezmk pkg install` / `ezmk repo add|remove|update`), so upgrades are transparent; no CLI behaviour change, no configuration semantics change, no breaking public API change. Windows binaries are unchanged in behaviour.
- Portable zip (`ezmk.exe`) from the v1.4.5 GitHub Release; `InstallerSha256` taken from the release asset digest.
