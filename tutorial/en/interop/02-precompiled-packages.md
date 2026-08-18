# 14. Multi-platform, multi-toolchain precompiled packages

Most libraries should ship **source** (`src/`): a source package compiles on any platform and compiler. But some libraries can't be built with a simple `gcc`/`g++` command (they need CMake, autotools, OpenSSL's `Configure`, ...), or take a very long time to compile (gRPC, Qt) — that's when you use `precompiled = true` and provide prebuilt artifacts under `lib/`.

> ⚠️ **The classic trap**: drop a `.a` built with GCC 11 into your repo and ask a colleague to link it with GCC 13 — the linker spews `std::__cxx11` undefined references and you debug all day. C++ has no "pure C ABI": mismatched compiler family, toolchain version, or standard-library ABI fails at link time. This chapter shows how to distribute multiple artifacts safely using the 1.2.0-dev.10 naming convention.

## Minimal package

```toml
[project]
name = "sdl2"
version = "2.32.10"
type = "static"
precompiled = true

# No src/ directory — prebuilt artifacts go directly in lib/
```

## Naming convention: `os-arch[-compiler][-abi]`

`lib<name>.<os>-<arch>[-<compiler>][-<abi>].<ext>` (1.2.0-dev.10+)

| OS | Arch | Tag |
|----|------|-----|
| Windows | x86_64 | `win-x64` |
| Windows | x86 | `win-x86` |
| Linux | x86_64 | `linux-x64` |
| Linux | aarch64 | `linux-arm64` |
| macOS | x86_64 | `mac-x64` |
| macOS | aarch64 | `mac-arm64` |

- **Compiler tag** (optional): `gcc<major>` (e.g. `gcc13`), `clang<major>` (e.g. `clang18`), `msvc143` (VS toolset lookup: 140/141/142/143).
- **ABI tag** (optional): GCC / Clang (Linux, libstdc++ default) → `abi11` (CXX11 ABI); Apple Clang (libc++ default) and MSVC → none.

A "multi-platform, multi-toolchain" package:

```
sdl2/
├── ezmk.toml
├── include/       # headers (shared across platforms)
└── lib/           # prebuilt static libraries
    ├── libSDL2.win-x64-msvc143.a
    ├── libSDL2.linux-x64-gcc13-abi11.a
    ├── libSDL2.mac-arm64-clang15.a
    └── libSDL2.win-x64.a          # no toolchain tag (legacy, fallback match)
```

## Selection priority: ABI-safe 4-level matching

At install time `ezmk` matches against the current toolchain, highest first:

1. **L4 full tag**: `os-arch-compiler-abi` all equal (e.g. `linux-x64-gcc13-abi11`)
2. **L3 same compiler**: `os-arch-compiler` equal and the artifact has no abi segment (same compiler = same default ABI)
3. **L2 platform**: only `os-arch` equal (legacy artifacts without a toolchain tag)
4. **L1 bare name**: suffixless `lib<name>.a` (backward compatible single-platform packages)

- Same compiler but explicitly different abi segment (e.g. `gcc11-abi8` vs `gcc11-abi11`) → **ABI-incompatible, skipped**.
- Falling to L2/L1 (possibly cross-toolchain) → **explicit warning** naming the current toolchain tag and the available artifacts — no more silently grabbing a wrong-ABI library that explodes at link time.
- `[project].precompiled_strict = true` → fallback becomes a **fail-fast error**.

## Failure case: `std::__cxx11` undefined reference

The docs did say "platform and architecture". But as a developer who has been burned by the C++ ABI countless times, the instinctive reading of "platform and architecture" is: "oh, a `.a` built on Windows just can't be used on Linux". So you confidently drop a `.a` built with GCC 11 into the repo and have a colleague link it with GCC 13 — the linker spews `std::__cxx11` undefined references and you debug for a whole day.

The cause: libstdc++'s CXX11 ABI (`_GLIBCXX_USE_CXX11_ABI`) — `abi11` (new) / `abi8` (old) can't mix. That's why since dev.10 artifacts carry compiler and ABI tags, letting `ezmk` pick a matching artifact at **install time** instead of failing at link time.

## Best practice

> **Best practice (precompiled packages only)**: within one package, place multiple toolchain/ABI artifacts side by side using `os-arch[-compiler][-abi]` names, and let `ezmk` auto-select for the current toolchain. **Source distribution (`src/`) is still far better than precompiled** — precompiled artifacts only work on the platforms/toolchains/ABIs you declared; source packages compile everywhere.

## Pitfalls

- **MSVC runtime**: a static library's CRT binding (`/MD` vs `/MT`) must match the consumer — noted in the docs; dev.10 does not add a runtime-dimension tag yet.
- **Apple Clang / clang-cl**: Apple Clang version numbers don't align with LLVM (ABI can change within the same major); clang-cl doesn't emit `msvc1xx` tags — use real MSVC builds when you need the MSVC ABI.
- **Legacy-ABI consumers**: when a consumer builds with explicit `-D_GLIBCXX_USE_CXX11_ABI=0`, ezmk does not auto-detect — authors can name a separate `abi8` artifact for that scenario; matching defaults to `abi11`.
