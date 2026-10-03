#### Pull Request Description

- [x] Have you signed the [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/)?
- [x] Have you checked that there aren't other open [pull requests](https://github.com/microsoft/winget-pkgs/pulls) for the same manifest update/change?
- [x] This PR only modifies one (1) manifest
- [x] Have you [validated](https://learn.microsoft.com/windows/package-manager/winget/validate) your manifest locally with `winget validate --manifest <path>`?
- [ ] Have you tested your manifest locally with `winget install --manifest <path>`?
- [x] Does your manifest conform to the [1.6 schema](https://learn.microsoft.com/en-us/windows/package-manager/winget/manifest/schema/1.6.0)?

---

**Description:**
New version: EazyMake.EazyMake version 1.4.7

- EazyMake is a simple C/C++ build tool (CLI named `ezmk`), GCC/Clang/MSVC.
- v1.4.7 fixes MSVC toolchain support: VS Build Tools are now detected (vswhere `-products *` + BuildTools fallback), the vcvars probe/loader command is quoted correctly, and the vcvars environment (with CRLF-stripped values) is injected into compile/link/archive subprocesses with absolute cl/link/lib paths — so MSVC works without a "Developer Command Prompt". Toolchain priority is now `$CXX`/`$CC` → system g++/clang++ → MSVC fallback, with a new `EZMK_TOOLCHAIN=gcc|clang|msvc` override. No breaking public API change.
- Portable zip (`ezmk.exe`) from the v1.4.7 GitHub Release; `InstallerSha256` taken from the release asset digest.
