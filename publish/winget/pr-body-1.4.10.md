#### Pull Request Description

- [x] Have you signed the [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/)?
- [x] Have you checked that there aren't other open [pull requests](https://github.com/microsoft/winget-pkgs/pulls) for the same manifest update/change?
- [x] This PR only modifies one (1) manifest
- [x] Have you [validated](https://learn.microsoft.com/windows/package-manager/winget/validate) your manifest locally with `winget validate --manifest <path>`?
- [ ] Have you tested your manifest locally with `winget install --manifest <path>`?
- [x] Does your manifest conform to the [1.6 schema](https://learn.microsoft.com/en-us/windows/package-manager/winget/manifest/schema/1.6.0)?

---

**Description:**
New version: EazyMake.EazyMake version 1.4.10

- EazyMake is a simple C/C++ build tool (CLI named `ezmk`), GCC/Clang/MSVC.
- v1.4.10 adds **optional recursive source discovery**: `[compile].src_dirs` / `[test].dirs` entries ending in `/**` are searched recursively (e.g. `["src/**", "lib"]`), while plain entries keep the existing flat scan. Default behavior is unchanged.
- `project export cmake` now uses `GLOB` for flat entries and `GLOB_RECURSE` for `/**` entries, so an exported CMake build collects the same sources as `ezmk build`.
- No breaking public API change.
- Portable zip (`ezmk.exe`) from the v1.4.10 GitHub Release; `InstallerSha256` taken from the release asset digest.
