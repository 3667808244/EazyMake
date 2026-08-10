---
name: ezmk-i18n
description: How to add or modify translations in EazyMake — X-macro i18n_keys.def mechanism, en.json/zh.json bilingual maintenance.
---

# EazyMake Internationalization (i18n)

## Architecture

EazyMake uses a **compile-time embedded JSON** approach for translations. All user-visible strings are keyed via an enum and translated through JSON locale files embedded into the binary at build time.

### Single source of truth (0.2.6+)

```
include/ezmk/i18n_keys.def   ←  X-macro: one EZMK_I18N_KEY(name) per key
        ↓                              ↓
   I18nKey enum               key_name() mapping
   (i18n.hpp)                 (i18n.cpp)
        ↓                              ↓
   C++ code uses              JSON files use
   I18nKey::build_success     "build_success"
```

The X-macro approach guarantees the enum and the JSON key name mapping can never drift out of sync.

### Build-time embedding

```
locale/en.json  ──┐
                   ├── scripts/embed_locale.py ──→ src/locale_data.cpp ──→ binary
locale/zh.json  ──┘
```

`build.sh` runs `scripts/embed_locale.py` before compilation, which reads both JSON files and generates a C++ source file with the locale data embedded as compile-time strings.

## Adding a new translatable string

### Step 1: Add key to `i18n_keys.def`

Open `include/ezmk/i18n_keys.def` and add a new line:

```c
EZMK_I18N_KEY(my_new_key)
```

- Place it in the correct section (grouped by module: `// ---- build ----`, `// ---- pkg ----`, etc.)
- The key name MUST match the JSON key string exactly (case-sensitive)

### Step 2: Add translations to JSON files

In `locale/en.json`:
```json
"my_new_key": "This is the English text."
```

In `locale/zh.json`:
```json
"my_new_key": "这是中文文本。"
```

- Use `%s` and `%d` for format arguments (printf-style)
- Keep the JSON structure flat — no nested objects

### Step 3: Rebuild

```bash
bash build.sh
```

This re-runs `scripts/embed_locale.py` and recompiles. The new key is now available as `I18nKey::my_new_key` in C++ code.

## Using i18n keys in C++ code

```cpp
#include "ezmk/i18n.hpp"

// Simple string
util::info_line(i18n::get(I18nKey::build_success));

// With format arguments
util::info_line(i18n::get(I18nKey::compiling, source_name, n, total));
```

## Debug: audit_missing_keys()

In debug builds, `i18n::init()` runs `audit_missing_keys()` which warns once per key that exists in the enum but is missing from the loaded locale. If you see warnings like:

```
[i18n] missing key in locale: my_new_key
```

…it means you added the key to `i18n_keys.def` but forgot to add it to `en.json` and/or `zh.json`.

## File roles

| File | Role | Edit? |
|------|------|-------|
| `include/ezmk/i18n_keys.def` | X-macro key list — single source of truth | **Yes** (add new keys here) |
| `locale/en.json` | English translations | **Yes** (add new strings here) |
| `locale/zh.json` | Chinese translations | **Yes** (add new strings here) |
| `src/i18n.cpp` | `key_name()` mapping + `init()` | No (auto-derived from `.def`) |
| `include/ezmk/i18n.hpp` | `I18nKey` enum + `get()` API | No (auto-derived from `.def`) |
| `scripts/embed_locale.py` | Compile-time JSON → C++ embedder | No (infrastructure) |
| `src/locale_data.cpp` | Generated embedded data | No (auto-generated) |

## Supported languages

| Language | File | Env override |
|----------|------|-------------|
| English (default) | `locale/en.json` | `EZMK_LANG=en` |
| Chinese | `locale/zh.json` | `EZMK_LANG=zh` |

If `EZMK_LANG` is unset, the system locale is auto-detected. English is the fallback when no translation is available.
