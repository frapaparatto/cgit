#!/usr/bin/env bash

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

# testing ls-tree
# We need a tree object in .cgit/objects. Create a real git repo, build a tree,
# then copy its objects into .cgit/objects so cgit can read them.
echo "--- ls-tree ---"
LSDIR="$TMPDIR/ls-tree-test"
mkdir -p "$LSDIR" && cd "$LSDIR"
git init --quiet
mkdir -p subdir
echo "file content" >hello.txt
echo "nested content" >subdir/nested.txt
git add .
git commit -m "test commit" --quiet
TREE_HASH=$(git rev-parse HEAD^\{tree\})

# Bootstrap cgit repo and copy git objects into it
"$CGIT" init >/dev/null
cp -r .git/objects/* .cgit/objects/

EXPECTED=$(git ls-tree "$TREE_HASH")
ACTUAL=$("$CGIT" ls-tree "$TREE_HASH")
[ "$EXPECTED" = "$ACTUAL" ] &&
  ok "ls-tree output matches git" ||
  fail "ls-tree output differs (expected: '$EXPECTED', got: '$ACTUAL')"

# testing ls-tree --name-only
echo "--- ls-tree --name-only ---"
EXPECTED=$(git ls-tree --name-only "$TREE_HASH")
ACTUAL=$("$CGIT" ls-tree --name-only "$TREE_HASH")
[ "$EXPECTED" = "$ACTUAL" ] &&
  ok "ls-tree --name-only output matches git" ||
  fail "ls-tree --name-only output differs (expected: '$EXPECTED', got: '$ACTUAL')"

cd "$TMPDIR"

# testing write-tree (flat directory)
echo "--- write-tree (flat) ---"
WTDIR="$TMPDIR/write-tree-flat"
mkdir -p "$WTDIR" && cd "$WTDIR"
"$CGIT" init >/dev/null
echo "hello world" >hello.txt
echo "foo bar" >readme.md

# Build a reference tree with real git (same files, no .cgit contamination)
WTGIT="$TMPDIR/write-tree-flat-git"
mkdir -p "$WTGIT"
cp hello.txt readme.md "$WTGIT/"
git -C "$WTGIT" init --quiet
git -C "$WTGIT" add .
GIT_FLAT_HASH=$(git -C "$WTGIT" write-tree)

WT_HASH=$("$CGIT" write-tree)
[ ${#WT_HASH} -eq 40 ] &&
  ok "write-tree produces 40-char hash" ||
  fail "write-tree did not produce 40-char hash, got '$WT_HASH'"

[ "$WT_HASH" = "$GIT_FLAT_HASH" ] &&
  ok "write-tree hash matches git ($WT_HASH)" ||
  fail "write-tree hash mismatch (cgit: '$WT_HASH', git: '$GIT_FLAT_HASH')"

TYPE=$("$CGIT" cat-file -t "$WT_HASH")
[ "$TYPE" = "tree" ] &&
  ok "write-tree object type is 'tree'" ||
  fail "expected type 'tree', got '$TYPE'"

"$CGIT" cat-file -e "$WT_HASH" &&
  ok "write-tree object exists (cat-file -e)" ||
  fail "write-tree object not found via cat-file -e"

EXPECTED=$(git -C "$WTGIT" ls-tree "$GIT_FLAT_HASH")
ACTUAL=$("$CGIT" ls-tree "$WT_HASH")
[ "$EXPECTED" = "$ACTUAL" ] &&
  ok "ls-tree on write-tree output matches git" ||
  fail "ls-tree mismatch (expected: '$EXPECTED', got: '$ACTUAL')"

WT_HASH2=$("$CGIT" write-tree)
[ "$WT_HASH" = "$WT_HASH2" ] &&
  ok "write-tree is idempotent" ||
  fail "write-tree not idempotent (first: '$WT_HASH', second: '$WT_HASH2')"

# testing write-tree (recursive — directory with subdirectory)
echo "--- write-tree (recursive) ---"
WTSUBDIR="$TMPDIR/write-tree-subdir"
mkdir -p "$WTSUBDIR" && cd "$WTSUBDIR"
"$CGIT" init >/dev/null
echo "top level" >top.txt
mkdir -p subdir
echo "nested content" >subdir/nested.txt

WTSUBGIT="$TMPDIR/write-tree-subdir-git"
mkdir -p "$WTSUBGIT/subdir"
cp top.txt "$WTSUBGIT/"
cp subdir/nested.txt "$WTSUBGIT/subdir/"
git -C "$WTSUBGIT" init --quiet
git -C "$WTSUBGIT" add .
GIT_SUB_HASH=$(git -C "$WTSUBGIT" write-tree)

SUB_HASH=$("$CGIT" write-tree)
[ ${#SUB_HASH} -eq 40 ] &&
  ok "write-tree (recursive) produces 40-char hash" ||
  fail "write-tree (recursive) did not produce 40-char hash, got '$SUB_HASH'"

[ "$SUB_HASH" = "$GIT_SUB_HASH" ] &&
  ok "write-tree (recursive) hash matches git ($SUB_HASH)" ||
  fail "write-tree (recursive) hash mismatch (cgit: '$SUB_HASH', git: '$GIT_SUB_HASH')"

EXPECTED=$(git -C "$WTSUBGIT" ls-tree "$GIT_SUB_HASH")
ACTUAL=$("$CGIT" ls-tree "$SUB_HASH")
[ "$EXPECTED" = "$ACTUAL" ] &&
  ok "ls-tree on recursive write-tree output matches git" ||
  fail "ls-tree (recursive) mismatch (expected: '$EXPECTED', got: '$ACTUAL')"

# testing write-tree outside a repo (no .cgit)
echo "--- write-tree outside repo ---"
NOREPODIR="$TMPDIR/no-repo"
mkdir -p "$NOREPODIR" && cd "$NOREPODIR"
echo "some file" >file.txt
"$CGIT" write-tree 2>/dev/null &&
  fail "write-tree outside repo should exit non-zero" ||
  ok "write-tree outside repo exits non-zero"

cd "$TMPDIR"

# testing commit-tree
echo "--- commit-tree (root commit) ---"
CTDIR="$TMPDIR/commit-tree-test"
mkdir -p "$CTDIR" && cd "$CTDIR"
"$CGIT" init >/dev/null
echo "hello" >hello.txt
TREE_H=$("$CGIT" write-tree)

COMMIT_H=$("$CGIT" commit-tree "$TREE_H" -m "initial commit")
[ ${#COMMIT_H} -eq 40 ] &&
  ok "commit-tree produces 40-char hash" ||
  fail "commit-tree did not produce 40-char hash, got '$COMMIT_H'"

TYPE=$("$CGIT" cat-file -t "$COMMIT_H")
[ "$TYPE" = "commit" ] &&
  ok "commit-tree object type is 'commit'" ||
  fail "expected type 'commit', got '$TYPE'"

CONTENT=$("$CGIT" cat-file -p "$COMMIT_H")
echo "$CONTENT" | grep -q "^tree $TREE_H$" &&
  ok "commit content has correct tree hash" ||
  fail "commit content missing correct tree line"

echo "$CONTENT" | grep -q "^initial commit$" &&
  ok "commit content has correct message" ||
  fail "commit content missing correct message"

echo "$CONTENT" | grep -q "^parent " &&
  fail "root commit should not have parent line" ||
  ok "root commit has no parent line"

# testing commit-tree with parent
echo "--- commit-tree (with parent) ---"
COMMIT_H2=$("$CGIT" commit-tree "$TREE_H" -p "$COMMIT_H" -m "second commit")
[ ${#COMMIT_H2} -eq 40 ] &&
  ok "commit-tree with parent produces 40-char hash" ||
  fail "commit-tree with parent did not produce 40-char hash, got '$COMMIT_H2'"

CONTENT2=$("$CGIT" cat-file -p "$COMMIT_H2")
echo "$CONTENT2" | grep -q "^tree $TREE_H$" &&
  ok "commit with parent has correct tree hash" ||
  fail "commit with parent missing correct tree line"

echo "$CONTENT2" | grep -q "^parent $COMMIT_H$" &&
  ok "commit content has correct parent hash" ||
  fail "commit content missing correct parent line"

echo "$CONTENT2" | grep -q "^second commit$" &&
  ok "commit with parent has correct message" ||
  fail "commit with parent missing correct message"

# testing commit-tree error cases
echo "--- commit-tree (error cases) ---"
"$CGIT" commit-tree "$TREE_H" 2>/dev/null &&
  fail "commit-tree without -m should exit non-zero" ||
  ok "commit-tree without -m exits non-zero"

"$CGIT" commit-tree "not-a-valid-hash" -m "test" 2>/dev/null &&
  fail "commit-tree with invalid tree hash should exit non-zero" ||
  ok "commit-tree with invalid tree hash exits non-zero"

"$CGIT" commit-tree "$TREE_H" -p "not-a-valid-hash" -m "test" 2>/dev/null &&
  fail "commit-tree with invalid parent hash should exit non-zero" ||
  ok "commit-tree with invalid parent hash exits non-zero"

cd "$TMPDIR"

echo "--- error handling ---"
"$CGIT" nosuchcmd 2>/dev/null && fail "unknown command should exit non-zero" || ok "unknown command rejected"

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
