#### Pull Request Description

- [x] Have you signed the [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/)?
- [x] Have you checked that there aren't other open [pull requests](https://github.com/microsoft/winget-pkgs/pulls) for the same manifest update/change?
- [x] This PR only modifies one (1) manifest
- [x] Have you [validated](https://learn.microsoft.com/windows/package-manager/winget/validate) your manifest locally with `winget validate --manifest <path>`?
- [ ] Have you tested your manifest locally with `winget install --manifest <path>`?
- [x] Does your manifest conform to the [1.6 schema](https://learn.microsoft.com/en-us/windows/package-manager/winget/manifest/schema/1.6.0)?

---

**Description:**
New version: EazyMake.EazyMake version 1.4.8

- EazyMake is a simple C/C++ build tool (CLI named `ezmk`), GCC/Clang/MSVC.
- v1.4.8 bumps the embedded dependencies: miniz 2.2.0 → 3.1.2 (ZIP robustness fixes: central-directory offset overflow, `tinfl` infinite loop, Windows `mz_utf8z_to_widechar` overflow), Lua 5.4.7 → 5.4.9 (final 5.4 release; 6 upstream bug fixes), Catch2 3.8.0 → 3.16.0 (test-only), plus GitHub Actions major bumps (checkout/upload-artifact → v7, action-gh-release → v3). No breaking public API change.
- Portable zip (`ezmk.exe`) from the v1.4.8 GitHub Release; `InstallerSha256` taken from the release asset digest.
