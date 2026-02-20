
int handle_ls_tree(int argc, char *argv[]) {
  int result = 1;
  /*
   *
handle_write_tree (commands/write_tree.c)
  │
  └── write_tree_recursive (core/object.c)
        │
        ├── for each file:
        │     read_file → write_object("blob", ...) → get hash
        │
        ├── for each directory:
        │     write_tree_recursive(subdir) → get hash    ← recurse
        │
        ├── sort entries alphabetically
        ├── serialize entries to binary format (using hex_to_bytes for hashes)
        └── write_object("tree", content, ...) → return tree hash

You don't need a wrapper. You already have the pattern from `parse_tree` — a
dynamically allocated array of `tree_entry_t` and a `size_t count`. Use the same
approach.

Your `write_tree_recursive` function scans a directory, builds up an array of
`tree_entry_t` (one per entry), sorts the array by name, serializes to binary
content, writes the tree object, and returns the hash. The array and count are
local to that function call — each directory level creates its own, uses it,
frees it.

The one mismatch to keep in mind: `tree_entry_t` stores the hash as 40-char hex,
but the binary format needs 20 raw bytes. You handle that at serialization time
with `hex_to_bytes` — not a reason to create a different struct.
   */

  return result;
}
