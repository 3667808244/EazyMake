#!/usr/bin/env python3
"""
check_man_sync.py - man page / CLI drift gate (1.4.3).

The man pages in man/ are hand-written roff, but the CLI surface is defined in
C++ (src/cli.cpp option specs + the kAliases table + the help rows) and the
configuration keys in src/config.cpp. This script keeps the two sides in sync:
it extracts the authoritative sets from the sources and compares them with what
the man pages document, in BOTH directions (a missing entry and a stale entry
are both failures).

Checks performed
  1. long options  - every OptionSpec long name is documented in ezmk(1);
                     every "--name" in the man pages is a real option or is
                     explicitly allow-listed (global / top-level / placeholders).
  2. short options - same, for single-letter options.
  3. shorthands    - every kAliases entry appears in ezmk(1)'s SHORTHANDS block.
  4. commands      - every command row from cli::print_help() is documented.
  5. environment   - every std::getenv() variable is either documented in
                     ezmk(1)'s ENVIRONMENT block or explicitly classified as
                     internal (so a NEW variable fails the gate until it is
                     classified deliberately).
  6. config keys   - every key read by src/config.cpp is documented in
                     ezmk.toml(5).

Anti-silent-failure rule: extraction has a built-in BASELINE. If a refactor of
src/cli.cpp or src/config.cpp makes the regular expressions match fewer entries
than the baseline, the script FAILS and tells you to update the extractor
instead of quietly passing a smaller comparison.

Usage
  python scripts/check_man_sync.py            # from the repo root (auto-detected)
  python scripts/check_man_sync.py <repo_root>

Exit code 0 = in sync, 1 = drift found, 2 = usage error.
"""

import re
import sys
from pathlib import Path

MAN_PAGES = ["man/ezmk.1", "man/ezmk.toml.5"]
CLI_SOURCE = "src/cli.cpp"
CONFIG_SOURCE = "src/config.cpp"

# ---------------------------------------------------------------- baselines --
# Deliberately updated by hand. Lower extraction numbers mean the regexes no
# longer understand the source, NOT that the CLI shrank silently.
BASELINE = {
    "long_options": 37,
    "short_options": 12,    # includes -V (test --verbose alias)
    "shorthands": 28,       # 25 two-letter + 3 single-letter (u/h/v)
    "top_aliases": 7,       # build run clean watch install test pack
    "commands": 32,         # rows in cli::print_help()
    "env_vars": 14,
    "config_keys": 55,
}

# Long options that legitimately appear in the man pages without being an
# entry of an OptionSpec table.
EXTRA_LONG_OK = {
    "color",    # global option, consumed before per-command parsing
    "help",     # top-level; also an example-command option
    "version",  # top-level
    "flag",     # placeholder used by the OPTION SYNTAX section
}
# Short options documented although no OptionSpec declares them.
EXTRA_SHORT_OK = {
    "h",  # top-level help / example help
    "V",  # top-level version; also test --verbose
    "s",  # mentioned as "Catch2 is not given -s"
}

# Environment variables that are intentionally NOT part of ezmk(1): either the
# platform resolves them for us, or they belong to the test harness. Adding a
# variable to the code therefore forces a deliberate decision here.
ENV_INTERNAL = {
    "LOCALAPPDATA": "platform user-data directory",
    "APPDATA": "platform user-data directory",
    "HOME": "home directory resolution",
    "USERPROFILE": "home directory resolution",
    "HOMEDRIVE": "home directory resolution",
    "HOMEPATH": "home directory resolution",
    "TMPDIR": "system temp directory",
    "LANG": "system locale detection",
    "LC_ALL": "system locale detection",
    "EZMK_TEST_BIN": "integration-test harness only",
}
ENV_DOCUMENTED = {
    "EZMK_LANG",
    "NO_COLOR",
    "CXX",
    "CC",
    "SOURCE_DATE_EPOCH",
    "EDITOR",
    "VISUAL",
}

# Config keys that may appear as a bare ".B key" tag without being read by
# config.cpp (none today; the list keeps the reverse check honest).
CONFIG_KEY_EXTRA_OK = set()


# ------------------------------------------------------------- roff helpers --
ROFF_ESCAPES = [
    ("\\(ha", "^"),
    ("\\(ti", "~"),
    ("\\(dq", '"'),
    ("\\-", "-"),
    ("\\e", "\\"),
    ("\\&", ""),
]


def de_roff(text):
    """Turn the escapes used in man/*.1|5 back into plain characters."""
    for src, dst in ROFF_ESCAPES:
        text = text.replace(src, dst)
    return text


def read(path, root):
    file = root / path
    if not file.is_file():
        raise FileNotFoundError(f"required file not found: {file}")
    return file.read_text(encoding="utf-8")


def sections(page_text):
    """Split a man page into {SECTION NAME: text} on .SH boundaries."""
    out = {}
    current = None
    buf = []
    for line in page_text.splitlines():
        m = re.match(r"^\.SH\s+(.*\S)\s*$", line)
        if m:
            if current:
                out[current] = "\n".join(buf)
            current = m.group(1).strip().strip('"')
            buf = []
        elif current:
            buf.append(line)
    if current:
        out[current] = "\n".join(buf)
    return out


def roff_tags(block):
    """Collect the literal tokens used as .TP tags inside one section."""
    tags = []
    for line in block.splitlines():
        m = re.match(r'^\.(?:B|BR|BI|IB|RB|RI|IR)\s+(.*)$', line)
        if not m:
            continue
        body = m.group(1)
        # quoted arguments first, then bare words
        parts = re.findall(r'"([^"]*)"', body)
        parts += [p for p in re.split(r'\s+', re.sub(r'"[^"]*"', " ", body)) if p]
        tags.extend(p.strip() for p in parts if p.strip())
    return tags


# --------------------------------------------------------------- extractors --
def cli_options(cli_text):
    """OptionSpec entries: {short_char, "long-name", takes_value}."""
    longs, shorts = set(), set()
    for m in re.finditer(r"\{\s*'(\\0|[A-Za-z])'\s*,\s*\"([^\"]*)\"\s*,", cli_text):
        short, long_name = m.group(1), m.group(2)
        if long_name:
            longs.add(long_name)
        if short != "\\0":
            shorts.add(short)
    return longs, shorts


def cli_aliases(cli_text):
    """kAliases entries -> (single[set], two_letter[set], top_level[set])."""
    block = re.search(r"kAliases\s*=\s*\{(.*?)\n\s*\};", cli_text, re.S)
    if not block:
        raise ValueError("kAliases table not found in src/cli.cpp")
    text = block.group(1)
    single, two, top = set(), set(), set()
    pattern = re.compile(r'\{\s*"([^"]+)"\s*,\s*\{\s*"([^"]+)"\s*,\s*(nullptr|"([^"]*)")\s*\}\s*\}')
    for m in pattern.finditer(text):
        alias, group, _, sub = m.group(1), m.group(2), m.group(3), m.group(4)
        if len(alias) == 1:
            single.add(alias)        # "u" -> {"utils", nullptr}
        elif len(alias) == 2:
            two.add(alias)           # "pn" -> {"project", "new"}
        elif group == "project":
            top.add(alias)           # "build" -> {"project", "build"}
    return single, two, top


def cli_commands(cli_text):
    """Command phrases from print_help()'s row("ezmk ...") calls."""
    commands = set()
    for m in re.finditer(r'row\(\s*"(ezmk[^"]*)"', cli_text):
        text = m.group(1)
        text = re.split(r"\s+\[|\s+<|\s+-\-", text)[0]
        text = re.sub(r"\s+", " ", text).strip()
        if text:
            commands.add(text)
    return commands


def code_env_vars(sources):
    """Every std::getenv("NAME") in the first-party sources."""
    found = set()
    for text in sources:
        for m in re.finditer(r'getenv\(\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*\)', text):
            found.add(m.group(1))
    return found


def config_keys(config_text):
    """Keys read by src/config.cpp: table["key"] and table.get("key")."""
    keys = set()
    for m in re.finditer(r'\["([A-Za-z_][A-Za-z0-9_]*)"\]', config_text):
        keys.add(m.group(1))
    for m in re.finditer(r'get\(\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*\)', config_text):
        keys.add(m.group(1))
    return keys


# ------------------------------------------------------------------- checks --
class Report:
    def __init__(self):
        self.problems = []
        self.notes = []

    def fail(self, group, message):
        self.problems.append((group, message))

    def note(self, message):
        self.notes.append(message)

    def ok(self):
        return not self.problems


def check_baseline(report, counts):
    for name, value in sorted(counts.items()):
        floor = BASELINE.get(name)
        if floor is None:
            continue
        if value < floor:
            report.fail(
                "baseline",
                f"extracted only {value} {name} (baseline {floor}) — src/ layout "
                f"probably changed; update the extractor in this script",
            )


def main():
    if len(sys.argv) > 2:
        print("Usage: python check_man_sync.py [repo_root]", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parent.parent

    report = Report()
    try:
        cli_text = read(CLI_SOURCE, root)
        config_text = read(CONFIG_SOURCE, root)
        pages = {p: read(p, root) for p in MAN_PAGES}
        src_texts = [p.read_text(encoding="utf-8") for p in sorted((root / "src").glob("*.cpp"))]
    except (FileNotFoundError, ValueError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1

    man_plain = {p: de_roff(t) for p, t in pages.items()}
    ezmk1 = man_plain["man/ezmk.1"]
    ezmk1_sections = sections(pages["man/ezmk.1"])
    t5 = man_plain["man/ezmk.toml.5"]

    # ---- extraction -------------------------------------------------------
    longs, shorts = cli_options(cli_text)
    singles, two_letters, tops = cli_aliases(cli_text)
    commands = cli_commands(cli_text)
    env_vars = code_env_vars(src_texts)
    keys = config_keys(config_text)

    counts = {
        "long_options": len(longs),
        "short_options": len(shorts),
        "shorthands": len(singles) + len(two_letters),
        "top_aliases": len(tops),
        "commands": len(commands),
        "env_vars": len(env_vars),
        "config_keys": len(keys),
    }
    check_baseline(report, counts)

    # ---- 1) long options --------------------------------------------------
    man_longs = set(m.group(1) for m in re.finditer(r"(?<![\w-])--([A-Za-z][A-Za-z0-9-]*)", ezmk1))
    for name in sorted(longs - man_longs):
        report.fail("long options", f"--{name} is in src/cli.cpp but not documented in ezmk(1)")
    for name in sorted(man_longs - longs - EXTRA_LONG_OK):
        report.fail("long options", f"--{name} is documented in ezmk(1) but does not exist in src/cli.cpp")

    # ---- 2) short options -------------------------------------------------
    man_shorts = set(m.group(1) for m in re.finditer(r"(?<![\w-])-([A-Za-z])(?![\w-])", ezmk1))
    for name in sorted(shorts - man_shorts):
        report.fail("short options", f"-{name} is in src/cli.cpp but not documented in ezmk(1)")
    for name in sorted(man_shorts - shorts - EXTRA_SHORT_OK):
        report.fail("short options", f"-{name} is documented in ezmk(1) but does not exist in src/cli.cpp")

    # ---- 3) shorthands + top-level aliases --------------------------------
    block = ezmk1_sections.get("SHORTHANDS", "")
    if not block:
        report.fail("shorthands", "ezmk(1) has no SHORTHANDS section")
    else:
        missing = [a for a in sorted(singles | two_letters) if not re.search(rf"(?<![\w-]){re.escape(a)}(?![\w-])", block)]
        for alias in missing:
            report.fail("shorthands", f"alias '{alias}' is in kAliases but missing from the SHORTHANDS block")
    for alias in sorted(tops):
        if not re.search(rf"(?<![\w-])ezmk {re.escape(alias)}(?![\w-])", ezmk1):
            report.fail("commands", f"top-level alias 'ezmk {alias}' is not documented in ezmk(1)")

    # ---- 4) commands ------------------------------------------------------
    for command in sorted(commands):
        if not re.search(re.escape(command) + r"(?![\w-])", ezmk1):
            report.fail("commands", f"'{command}' comes from cli::print_help() but is not documented in ezmk(1)")

    # ---- 5) environment ---------------------------------------------------
    env_block = ezmk1_sections.get("ENVIRONMENT", "")
    if not env_block:
        report.fail("environment", "ezmk(1) has no ENVIRONMENT section")
    else:
        documented = set(t for t in roff_tags(env_block) if re.fullmatch(r"[A-Z][A-Z0-9_]*", t))
        for name in sorted(ENV_DOCUMENTED - documented):
            report.fail("environment", f"{name} is read by the code but not documented in the ENVIRONMENT block")
        for name in sorted(documented - env_vars - set(ENV_INTERNAL)):
            report.fail("environment", f"{name} is documented in the ENVIRONMENT block but never read by src/")
        for name in sorted(env_vars - documented - set(ENV_INTERNAL)):
            if name not in ENV_DOCUMENTED:
                report.fail(
                    "environment",
                    f"{name} is read by src/ but is neither documented nor classified in "
                    f"ENV_INTERNAL — decide which, then update this script",
                )

    # ---- 6) config keys ---------------------------------------------------
    for key in sorted(keys):
        if not re.search(rf"(?<![\w-]){re.escape(key)}(?![\w-])", t5):
            report.fail("config keys", f"[{key}] is read by src/config.cpp but not documented in ezmk.toml(5)")
    # Reverse direction: inside the field-table section, a ".B key" that follows
    # ".TP" is a field tag, so it must be a key the parser really reads. Prose
    # mentions elsewhere (.B fmt, .B false, ...) are not tags and are ignored.
    t5_sections = sections(pages["man/ezmk.toml.5"])
    lines = t5_sections.get("SECTIONS", "").splitlines()
    for index, line in enumerate(lines):
        if line != ".TP" or index + 1 >= len(lines):
            continue
        m = re.match(r"^\.B\s+([a-z_]+)\s*$", lines[index + 1])
        if m and m.group(1) not in keys and m.group(1) not in CONFIG_KEY_EXTRA_OK:
            report.fail("config keys", f"ezmk.toml(5) documents key '{m.group(1)}' which src/config.cpp never reads")

    # ---- summary ----------------------------------------------------------
    print("man page sync check")
    for name in sorted(counts):
        print(f"  extracted {name:14s}: {counts[name]}")
    print(f"  man pages           : {', '.join(MAN_PAGES)}")
    print()
    if report.ok():
        print(f"OK: man pages are in sync with {CLI_SOURCE} and {CONFIG_SOURCE}.")
        return 0

    grouped = {}
    for group, msg in report.problems:
        grouped.setdefault(group, []).append(msg)
    for group in sorted(grouped):
        print(f"[{group}] {len(grouped[group])} problem(s)")
        for msg in grouped[group]:
            print(f"  - {msg}")
    print(f"\nFAIL: {len(report.problems)} drift problem(s) found.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
