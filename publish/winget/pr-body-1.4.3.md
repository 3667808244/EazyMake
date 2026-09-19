#### Pull Request Description

- [x] Have you signed the [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/)?
- [x] Have you checked that there aren't other open [pull requests](https://github.com/microsoft/winget-pkgs/pulls) for the same manifest update/change?
- [x] This PR only modifies one (1) manifest
- [x] Have you [validated](https://learn.microsoft.com/windows/package-manager/winget/validate) your manifest locally with `winget validate --manifest <path>`?
- [ ] Have you tested your manifest locally with `winget install --manifest <path>`?
- [x] Does your manifest conform to the [1.6 schema](https://learn.microsoft.com/en-us/windows/package-manager/winget/manifest/schema/1.6.0)?

---

**Description:**
New version: EazyMake.EazyMake version 1.4.3

- EazyMake is a simple C/C++ build tool (CLI named `ezmk`), GCC/Clang/MSVC.
- v1.4.3 is a documentation/distribution patch (no CLI behaviour change beyond one extra help line, no breaking public API change): hand-written man pages (`ezmk(1)`, `ezmk-lua(1)`, `ezmk.toml(5)`, `ezmk-workspace.toml(5)`) plus a build-time drift gate (`scripts/check_man_sync.py`) that keeps them in sync with the CLI/config parsers, wired into CI; the manual pages ship in the Linux/macOS release tarballs and install into `share/man/man{1,5}`. `ezmk help` now ends with a `See also: man ezmk` line. Windows has no `man` command, so the Windows zip is intentionally unchanged.
- Portable zip (`ezmk.exe`) from the v1.4.3 GitHub Release; `InstallerSha256` taken from the release asset digest.
