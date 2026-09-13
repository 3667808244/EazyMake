#### Pull Request Description

- [x] Have you signed the [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/)?
- [x] Have you checked that there aren't other open [pull requests](https://github.com/microsoft/winget-pkgs/pulls) for the same manifest update/change?
- [x] This PR only modifies one (1) manifest
- [x] Have you [validated](https://learn.microsoft.com/windows/package-manager/winget/validate) your manifest locally with `winget validate --manifest <path>`?
- [ ] Have you tested your manifest locally with `winget install --manifest <path>`?
- [x] Does your manifest conform to the [1.6 schema](https://learn.microsoft.com/en-us/windows/package-manager/winget/manifest/schema/1.6.0)?

---

**Description:**
New version: EazyMake.EazyMake version 1.4.2

- EazyMake is a simple C/C++ build tool (CLI named `ezmk`), GCC/Clang/MSVC.
- v1.4.2 is a pure fix release (second round of the code-quality audit; no new features, no breaking public API change): Lua non-string error objects no longer crash the CLI, `workspace watch` starts every selected member, MSVC dependency tracking actually invalidates on header changes, `--locked` verifies the downloaded archive hash instead of the built artifact, the file watcher detects a dead worker instead of hanging silently, Windows non-ASCII paths go through the wide APIs, `repo` paths stay inside the repo directory, and `--disable-cache` no longer re-arms the cache it bypassed.
- Portable zip (`ezmk.exe`) from the v1.4.2 GitHub Release; `InstallerSha256` taken from the release asset digest.
