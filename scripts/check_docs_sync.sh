#!/usr/bin/env bash
# check_docs_sync.sh — Verify docs/ and tutorial/ en↔zh file correspondence
# Exit 1 if any mismatch is found.

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

failures=0

check_dir() {
    local dir="$1"
    local en_dir="$dir/en"
    local zh_dir="$dir/zh"

    if [[ ! -d "$en_dir" ]]; then
        echo -e "${RED}MISSING: $en_dir${NC}"
        failures=$((failures + 1))
        return
    fi
    if [[ ! -d "$zh_dir" ]]; then
        echo -e "${RED}MISSING: $zh_dir${NC}"
        failures=$((failures + 1))
        return
    fi

    local en_files zh_files
    en_files=$(cd "$en_dir" && find . -name '*.md' | sort)
    zh_files=$(cd "$zh_dir" && find . -name '*.md' | sort)

    if [[ "$en_files" != "$zh_files" ]]; then
        echo -e "${RED}MISMATCH in $dir:${NC}"
        diff <(echo "$en_files") <(echo "$zh_files") || true
        failures=$((failures + 1))
    else
        echo -e "${GREEN}OK: $dir${NC} ($(echo "$en_files" | wc -l) files matched)"
    fi
}

echo "=== EazyMake docs sync check ==="
echo ""

check_dir "docs"
check_dir "tutorial"

echo ""
if [[ $failures -eq 0 ]]; then
    echo -e "${GREEN}All checks passed.${NC}"
    exit 0
else
    echo -e "${RED}$failures check(s) failed.${NC}"
    exit 1
fi
