#!/usr/bin/env python3
"""
check_i18n.py — i18n consistency check (1.1.0-pre.3, 3.2.4; 1.3.0-dev.4 variant rules).

The single source of truth for translatable strings is include/ezmk/i18n_keys.def
(X-macro). Every key declared there MUST exist in both base locale files
(locale/en.json, locale/zh.json) under the top-level "strings" object, and
neither base file may contain keys that are not declared in the .def —
otherwise the C++ side emits `{???}` / missing-key warnings at runtime.

Locale VARIANT files (every other locale/<tag>.json, e.g. zh-TW.json) follow
inheritance rules (1.3.0-dev.4):
  * keys ⊆ def keys is LEGAL — missing keys are inherited from the base
    language (variant files only list their differences);
  * keys ⊄ def (extra/unknown keys) is an ERROR — drift guard;
  * meta.language must equal the file tag;
  * meta.extends is optional — defaults to the tag's first segment; when
    declared explicitly the referenced base file must exist.

Usage:
  python scripts/check_i18n.py            # from the repo root (auto-detected)
  python scripts/check_i18n.py <repo_root>  # explicit root

Exit code 0 = consistent, 1 = mismatch found. Intended to be wired into CI
alongside the build.sh test workflow.
"""

import json
import re
import sys
from pathlib import Path

DEF_PATH = "include/ezmk/i18n_keys.def"
BASE_LOCALES = ["en", "zh"]


def load_def_keys(root: Path):
    """Extract all EZMK_I18N_KEY(...) names from the X-macro .def file."""
    def_file = root / DEF_PATH
    if not def_file.is_file():
        raise FileNotFoundError(f"i18n key definition not found: {def_file}")

    keys = set()
    pat = re.compile(r"EZMK_I18N_KEY\(([A-Za-z0-9_]+)\)")
    for line in def_file.read_text(encoding="utf-8").splitlines():
        # Skip `//` comment lines — header docs contain EZMK_I18N_KEY(name)
        # examples that must not be counted as real keys.
        if line.lstrip().startswith("//"):
            continue
        for m in pat.finditer(line):
            keys.add(m.group(1))
    return keys


def load_locale(root: Path, lang: str):
    """Load a locale file, returning (data, keyset). Raises on malformed files."""
    file = root / "locale" / f"{lang}.json"
    if not file.is_file():
        raise FileNotFoundError(f"locale file not found: {file}")

    try:
        data = json.loads(file.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        raise ValueError(f"{file} is not valid JSON: {e}")

    strings = data.get("strings")
    if not isinstance(strings, dict):
        raise ValueError(f"{file} has no 'strings' object")
    return data, set(strings.keys())


def diff_report(label, only_def, only_locale, name):
    """Print a per-locale diff; return True if a mismatch exists."""
    problems = False
    if only_def:
        problems = True
        print(f"  [{name}] in i18n_keys.def but missing from locale: {sorted(only_def)}")
    if only_locale:
        problems = True
        print(f"  [{name}] in locale but not declared in i18n_keys.def: {sorted(only_locale)}")
    return problems


def check_variant(root: Path, tag: str, def_keys: set):
    """Validate one variant file (1.3.0-dev.4 inheritance rules)."""
    problems = False
    try:
        data, keys = load_locale(root, tag)
    except (FileNotFoundError, ValueError) as e:
        print(f"  [{tag}] {e}")
        return True

    # meta.language must equal the file tag.
    meta = data.get("meta")
    if isinstance(meta, dict):
        declared = meta.get("language")
        if declared != tag:
            problems = True
            print(f"  [{tag}] meta.language '{declared}' != file tag '{tag}'")
    else:
        problems = True
        print(f"  [{tag}] missing 'meta' object")

    # Subset rule: missing keys = inherit (legal); EXTRA keys = drift (error).
    extra = keys - def_keys
    if extra:
        problems = True
        print(f"  [{tag}] keys not in i18n_keys.def (drift — variants may only "
              f"inherit, never add): {sorted(extra)}")

    # meta.extends: optional; default = first segment of the tag; if declared,
    # the referenced base file must exist.
    if isinstance(meta, dict):
        extends = meta.get("extends", tag.split("-")[0])
        if not isinstance(extends, str) or not extends:
            problems = True
            print(f"  [{tag}] meta.extends must be a non-empty string")
        elif extends not in BASE_LOCALES:
            problems = True
            print(f"  [{tag}] meta.extends '{extends}' is not a base locale "
                  f"({BASE_LOCALES})")
        elif not (root / "locale" / f"{extends}.json").is_file():
            problems = True
            print(f"  [{tag}] meta.extends '{extends}' has no locale file")
    return problems


def main():
    if len(sys.argv) > 2:
        print("Usage: python check_i18n.py [repo_root]", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parent.parent

    def_keys = load_def_keys(root)
    locale_dir = root / "locale"

    # Base locales: full three-way consistency (existing rule).
    base_keys = {}
    problems = False
    print(f"i18n_keys.def  : {len(def_keys)} keys")
    for lang in BASE_LOCALES:
        try:
            _, keys = load_locale(root, lang)
        except (FileNotFoundError, ValueError) as e:
            print(f"  [{lang}] {e}")
            problems = True
            continue
        base_keys[lang] = keys
        print(f"locale/{lang}.json: {len(keys)} keys")
        only_def = def_keys - keys
        only_locale = keys - def_keys
        if diff_report(lang, only_def, only_locale, lang):
            problems = True

    # Variant locales: auto-discovered (all *.json except base locales).
    variants = sorted(
        p.name[:-5]
        for p in locale_dir.glob("*.json")
        if p.name[:-5] not in BASE_LOCALES
    )
    for tag in variants:
        print(f"locale/{tag}.json: variant (inherits {tag.split('-')[0]})")
        if check_variant(root, tag, def_keys):
            problems = True

    if problems:
        print("\nFAIL: i18n files are NOT consistent.", file=sys.stderr)
        return 1
    print(f"\nOK: i18n_keys.def, base locales and {len(variants)} variant(s) are "
          f"consistent ({len(def_keys)} keys).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
