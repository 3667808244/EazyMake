#### Pull Request Description

- [x] Have you signed the [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/)?
- [x] Have you checked that there aren't other open [pull requests](https://github.com/microsoft/winget-pkgs/pulls) for the same manifest update/change?
- [x] This PR only modifies one (1) manifest
- [x] Have you [validated](https://learn.microsoft.com/windows/package-manager/winget/validate) your manifest locally with `winget validate --manifest <path>`?
- [ ] Have you tested your manifest locally with `winget install --manifest <path>`?
- [x] Does your manifest conform to the [1.6 schema](https://learn.microsoft.com/en-us/windows/package-manager/winget/manifest/schema/1.6.0)?

---

**Description:**
New version: EazyMake.EazyMake version 1.4.4

- EazyMake is a simple C/C++ build tool (CLI named `ezmk`), GCC/Clang/MSVC.
- v1.4.4 is a maintenance patch (no CLI behaviour change, no configuration semantics change, no breaking public API change): it fixes the Windows installer's documented `-DryRun` preview (it used to abort with an empty temp path before printing anything; now exit code 0, side-effect free, no network), clears the 7 remaining first-party compile warnings under strict flags, tidies `.gitignore`, replaces an outdated "to 1.2.0" TODO with an unversioned known limitation, and defaults the Intel macOS release job to skipped so Release runs no longer sit queued. Windows binaries are unchanged in behaviour.
- Portable zip (`ezmk.exe`) from the v1.4.4 GitHub Release; `InstallerSha256` taken from the release asset digest.
