#!/usr/bin/env bash
# Smoke test: run this after every refactoring step to verify nothing broke.
# Usage: ./test_smoke.sh [path-to-cgit-binary]
set -euo pipefail

CGIT="$(cd "$(dirname "${1:-./build/cgit}")" && pwd)/$(basename "${1:-./build/cgit}")"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

cd "$TMPDIR"

PASS=0
FAIL=0

ok() {
  PASS=$((PASS + 1))
  printf "  PASS: %s\n" "$1"
}
fail() {
  FAIL=$((FAIL + 1))
  printf "  FAIL: %s\n" "$1"
}

# testing git init
echo "--- init ---"
"$CGIT" init >/dev/null
[ -d .cgit/objects ] && [ -d .cgit/refs ] && [ -f .cgit/HEAD ] &&
  ok "directories and HEAD created" ||
  fail "directories or HEAD missing"

# testing hash-object
echo -n "hello world" >testfile.txt
HASH=$("$CGIT" hash-object -w testfile.txt)
[ ${#HASH} -eq 40 ] &&
  ok "hash is 40 hex chars ($HASH)" ||
  fail "unexpected hash length: '$HASH'"

# testing cat-file -t
echo "--- cat-file -t ---"
TYPE=$("$CGIT" cat-file -t "$HASH")
[ "$TYPE" = "blob" ] &&
  ok "type is blob" ||
  fail "expected 'blob', got '$TYPE'"

# testing cat-file -s
echo "--- cat-file -s ---"
SIZE=$("$CGIT" cat-file -s "$HASH")
[ "$SIZE" = "11" ] &&
  ok "size is 11" ||
  fail "expected '11', got '$SIZE'"

# testing cat-file -p
echo "--- cat-file -p ---"
CONTENT=$("$CGIT" cat-file -p "$HASH")
[ "$CONTENT" = "hello world" ] &&
  ok "content matches" ||
  fail "expected 'hello world', got '$CONTENT'"

# testing cat-file -e (existence check)
echo "--- cat-file -e ---"
"$CGIT" cat-file -e "$HASH" &&
  ok "existing object exits 0" ||
  fail "existing object should exit 0"

"$CGIT" cat-file -e 0000000000000000000000000000000000000000 2>/dev/null &&
  fail "missing object should exit non-zero" ||
  ok "missing object exits non-zero"

echo "--- error handling ---"
"$CGIT" nosuchcmd 2>/dev/null && fail "unknown command should exit non-zero" || ok "unknown command rejected"

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
