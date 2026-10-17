#### Pull Request Description

- [x] Have you signed the [Contributor License Agreement (CLA)](https://cla.opensource.microsoft.com/)?
- [x] Have you checked that there aren't other open [pull requests](https://github.com/microsoft/winget-pkgs/pulls) for the same manifest update/change?
- [x] This PR only modifies one (1) manifest
- [x] Have you [validated](https://learn.microsoft.com/windows/package-manager/winget/validate) your manifest locally with \`winget validate --manifest <path>\`?
- [ ] Have you tested your manifest locally with \`winget install --manifest <path>\`?
- [x] Does your manifest conform to the [1.6 schema](https://learn.microsoft.com/en-us/windows/package-manager/winget/manifest/schema/1.6.0)?

---

**Description:**
New version: EazyMake.EazyMake version 1.4.11

- EazyMake is a simple C/C++ build tool (CLI named \`ezmk\`), GCC/Clang/MSVC.
- v1.4.11 is the fourth code-quality audit round (47 fixes: P1 4 / P2 20 / P3 23): process safety (\`fork\` + \`setenv\`), Lua run-permission segment checks, atomic artifact replacement, import condition chains/platform constants, test-cache invalidation, lockfile/precompiled pinning, config/link/i18n robustness, CMake export quoting, workspace/project/CLI hardening.
- Behavior tightening: not-found/unknown commands exit non-zero; corrupted gzip/JSON and shell-metacharacter \`ezmk.run\` now error explicitly.
- No breaking public API change.
- Portable zip (\`ezmk.exe\`) from the v1.4.11 GitHub Release; \`InstallerSha256\` taken from the release asset digest.
