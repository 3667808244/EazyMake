#### Pull Request Description

- [x] Have you signed the [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/)?
- [x] Have you checked that there aren't other open [pull requests](https://github.com/microsoft/winget-pkgs/pulls) for the same manifest update/change?
- [x] This PR only modifies one (1) manifest
- [x] Have you [validated](https://learn.microsoft.com/windows/package-manager/winget/validate) your manifest locally with `winget validate --manifest <path>`?
- [ ] Have you tested your manifest locally with `winget install --manifest <path>`?
- [x] Does your manifest conform to the [1.6 schema](https://learn.microsoft.com/en-us/windows/package-manager/winget/manifest/schema/1.6.0)?

---

**Description:**
New version: EazyMake.EazyMake version 1.4.9

- EazyMake is a simple C/C++ build tool (CLI named `ezmk`), GCC/Clang/MSVC.
- v1.4.9 fixes build hooks so that `ctx.profile` reports the resolved active profile (an explicit `--profile` wins, otherwise `[compile].default_profile`) instead of the raw CLI value.
- It also adds `[compile.profile.<name>].export_objs` (boolean or archive path): a build with that profile packs every project object file into `build/obj_files.zip` (or a `.tar.gz` path). The generated `release` profile enables it by default.
- No breaking public API change.
- Portable zip (`ezmk.exe`) from the v1.4.9 GitHub Release; `InstallerSha256` taken from the release asset digest.
