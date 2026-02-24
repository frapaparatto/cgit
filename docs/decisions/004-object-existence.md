# 004: Dedicated Object Existence Check Over Flag in read_object

## Context

`cat-file -e` only needs to verify that an object exists on disk. The initial implementation added an `opt_e` parameter to `read_object` that, when set, skipped decompression and parsing. This worked but had two problems:

1. **Performance**: even with the flag, `read_object` still opened and read the file. An existence check only needs `stat()` or `access()`.
2. **Layered architecture violation**: `read_object` is a core function used by multiple commands. Adding a command-specific flag (`opt_e` is meaningless outside `cat-file`) made a core function aware of a UI concern.

The problem became concrete when implementing `ls-tree`, which also called `read_object`. The `opt_e` parameter was irrelevant to `ls-tree` but cluttered every call site.

## Decision

Removed `opt_e` from `read_object`. Created a standalone `object_exists` function in `core/object.c` that validates the hash, builds the path, and calls `access(path, F_OK)`. No file reading, no decompression, no parsing.

The `cat-file` handler now calls `object_exists` for `-e` and `read_object` for all other flags. The decision of *which* core function to call is made in the command layer, not the core layer.

## Alternatives Considered

- **Keep the flag**: simpler short-term, but every new command calling `read_object` would carry a meaningless parameter. The signature would accumulate command-specific flags over time.
- **Check existence in the handler using stat() directly**: would work but duplicates path-building logic. `object_exists` wraps the common sequence (validate hash → build path → check) in one reusable function.

## Consequences

- `read_object` has a clean, single-purpose signature: read and parse an object, always.
- `object_exists` is a lightweight primitive available to any command that needs an existence check without I/O overhead.
- The core layer provides clean primitives; the command layer composes them based on what the user requested.
