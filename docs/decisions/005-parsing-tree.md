# 005: Tree Parsing in Core, Not in Command Handlers

## Context

When implementing `ls-tree`, the tree content parsing logic (walking the binary format `<mode> <name>\0<20-byte-sha>`) needed a home. Two options: put it in `commands/ls_tree.c` (only place that uses it now) or in `core/object.c` (alongside `read_object` and `write_object`).

## Decision

Placed `parse_tree` and `free_tree_entries` in `core/object.c`. The function takes raw bytes and length (not a `git_object_t`), allocates an array of `tree_entry_t` structs, and returns them through output pointers with a count.

Type derivation (`type_from_mode`) is a `static` helper inside `object.c` — it's git object knowledge (mode 40000 means "tree", 100644 means "blob"), not command logic. `parse_tree` returns fully populated entries with type already set.

The function signature takes only `const unsigned char *data` and `size_t len`, not the full `git_object_t`. This follows the principle of minimal dependency, `parse_tree` doesn't need to know about the object struct, only about the raw content bytes.

## Alternatives Considered

- **Parse in the handler**: would work for `ls-tree` alone, but `checkout`, `diff`, `merge`, and any tree-walking command would need the same parsing. Duplicating binary format knowledge across commands violates the single-concept-single-place principle.

## Consequences

- Any command that needs tree data calls `parse_tree` and gets back a clean array. No binary format knowledge leaks into the command layer.
- The ownership model is the same as `read_object`: `parse_tree` allocates internally, ownership transfers to caller on success, caller frees with `free_tree_entries`.
- Adding new object-type parsers (tag objects, for example) follows the same pattern in the same location.
