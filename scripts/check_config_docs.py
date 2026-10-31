#!/usr/bin/env python3
"""Config-docs drift gate (1.5.0-pre.1).

Checks that every key accepted by the two-level config.toml parser is
documented in the user/global config docs and the man page, and that every
key read by parse_settings_table() is declared in is_known_settings_key().

The parser remains the single source of truth; docs must follow it.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "src" / "config.cpp"

DOCS = {
    "docs/zh/user_config.md": ROOT / "docs/zh/user_config.md",
    "docs/en/user_config.md": ROOT / "docs/en/user_config.md",
    "man/ezmk.toml.5": ROOT / "man/ezmk.toml.5",
    "docs/zh/global_config.md": ROOT / "docs/zh/global_config.md",
    "docs/en/global_config.md": ROOT / "docs/en/global_config.md",
}


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"FAIL: cannot read {path}: {exc}", file=sys.stderr)
        raise SystemExit(1)


def main() -> int:
    source = read_text(CONFIG)

    known_block = re.search(
        r"bool\s+is_known_settings_key\s*\([^)]*\)\s*\{(.*?)\n\}",
        source,
        re.S,
    )
    if not known_block:
        print("FAIL: cannot locate is_known_settings_key() in src/config.cpp", file=sys.stderr)
        return 1

    known = set(re.findall(r'"([a-z_]+)"', known_block.group(1)))

    parse_block = re.search(
        r"void\s+parse_settings_table\s*\([^)]*\)\s*\{(.*?)\n\}",
        source,
        re.S,
    )
    if not parse_block:
        print("FAIL: cannot locate parse_settings_table() in src/config.cpp", file=sys.stderr)
        return 1
    body = parse_block.group(1)
    read_keys = set(re.findall(r'read_(?:string|bool|int)\([^,]+,\s*"([a-z_]+)"', body))
    read_keys.add("initial_commit")  # special tri-state reader
    read_keys.update(re.findall(r't\.get\("([a-z_]+)"\)', body))

    unknown_read = sorted(read_keys - known)
    unread = sorted(known - read_keys)
    failures: list[str] = []

    if unknown_read:
        failures.append(
            "parse_settings_table() reads key(s) missing from "
            "is_known_settings_key(): " + ", ".join(unknown_read)
        )
    if unread:
        failures.append(
            "is_known_settings_key() declares key(s) never read by "
            "parse_settings_table(): " + ", ".join(unread)
        )

    for label, path in DOCS.items():
        text = read_text(path)
        missing = [k for k in sorted(known) if not re.search(r"\b" + re.escape(k) + r"\b", text)]
        # global_config.md intentionally lists only scope-specific semantics, so
        # it is not required to repeat every settings key.
        if label.startswith("docs/") and label.endswith("global_config.md"):
            continue
        if missing:
            failures.append(f"{label} is missing settings key(s): " + ", ".join(missing))

    if failures:
        for msg in failures:
            print("FAIL: " + msg, file=sys.stderr)
        return 1

    print(f"OK: {len(known)} settings keys documented in user_config + man5; parser readers consistent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
