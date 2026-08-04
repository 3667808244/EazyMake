#!/usr/bin/env python3
"""
check_i18n.py — Three-way i18n consistency check (1.1.0-pre.3, 3.2.4).

The single source of truth for translatable strings is include/ezmk/i18n_keys.def
(X-macro). Every key declared there MUST exist in both locale/en.json and
locale/zh.json (under the top-level "strings" object), and neither JSON may
contain keys that are not declared in the .def — otherwise the C++ side emits
`{???}` / missing-key warnings at runtime.

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
LOCALES = ["en", "zh"]


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


def load_locale_keys(root: Path, lang: str):
    """Extract the key set from the top-level 'strings' object of a locale file."""
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
    return set(strings.keys())


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


def main():
    if len(sys.argv) > 2:
        print("Usage: python check_i18n.py [repo_root]", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parent.parent

    def_keys = load_def_keys(root)
    locale_keys = {lang: load_locale_keys(root, lang) for lang in LOCALES}

    print(f"i18n_keys.def  : {len(def_keys)} keys")
    for lang in LOCALES:
        print(f"locale/{lang}.json: {len(locale_keys[lang])} keys")

    problems = False
    for lang in LOCALES:
        only_def = def_keys - locale_keys[lang]
        only_locale = locale_keys[lang] - def_keys
        if diff_report(lang, only_def, only_locale, lang):
            problems = True

    if problems:
        print("\nFAIL: i18n keys are NOT three-way consistent.", file=sys.stderr)
        return 1
    print("\nOK: i18n_keys.def, locale/en.json, locale/zh.json are consistent "
          f"({len(def_keys)} keys).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
