# cgit Architecture Overview

## What This Is

cgit is a reimplementation of git's core commands in C. The project serves two goals: understanding git's internals by building them, and learning how to write structured, maintainable C.

The codebase started as a 486-line monolithic `main.c` and was systematically refactored into a layered architecture. This document describes the resulting structure and the principles behind it.

## Layered Architecture

```
┌─────────────────────────────────────────┐
│  Layer 3: main.c                        │
│  Command dispatch table                 │
│  Parses argv, routes to handler         │
├─────────────────────────────────────────┤
│  Layer 2: commands/*.c                  │
│  Command handlers                       │
│  Validate input, orchestrate core       │
│  calls, format output, manage cleanup   │
├─────────────────────────────────────────┤
│  Layer 1: core/*.c                      │
│  Reusable building blocks               │
│  Object I/O, compression, hashing,      │
│  tree parsing, utilities                │
└─────────────────────────────────────────┘
```

**Dependency rule:** dependencies only point downward. Commands call core, never the reverse. Commands never call each other. Core modules may call other core modules at the same level.

**Rate of change:** each layer absorbs a different kind of change. Adding a new user-facing feature means adding a command — core is untouched. Changing an internal data structure means changing core — commands are untouched. Adding a new command to the CLI means adding one line to the dispatch table — everything else is untouched.

## Module Map

```
src/
├── main.c                          # Dispatch table + entry point
├── commands/
│   ├── init.c                      # Repository initialization
│   ├── cat_file.c                  # Object inspection
│   ├── hash_object.c               # Object creation from files
│   ├── ls_tree.c                   # Tree listing
│   ├── write_tree.c                # Tree creation from working directory
│   └── commit_tree.c               # Commit creation
├── core/
│   ├── object.c                    # read_object, write_object, parse_tree,
│   │                               # build_commit_content, object_exists
│   ├── compression.c               # zlib compress/decompress wrappers
│   ├── hash.c                      # SHA-1 computation (OpenSSL)
│   └── utils.c                     # Path building, file I/O, hash validation,
│                                   # header parsing, hex/byte conversion
└── include/
    ├── common.h                    # Error codes, constants, shared types
    ├── core.h                      # Core function declarations
    └── commands.h                  # Command handler declarations
```

## Conventions

### Error Handling
All core functions return `cgit_error_t`. The return value is always the status code; results are passed through output pointers. Command handlers translate core errors into user-facing messages and return 0 (success) or 1 (failure) to the dispatch table.

### Resource Ownership
Every allocation has a single, clear owner. Ownership transfers from callee to caller on success only. On failure, the callee frees its own partial allocations — the caller receives nothing. Every resource-owning type has a matching free function that is NULL-safe.

### Cleanup Pattern
Functions that allocate resources use `goto cleanup` with a single exit path. Resources are freed in reverse order of creation. This pattern is used consistently across all command handlers and core functions.

### Named Constants
All meaningful values are defined as macros in `common.h`. Buffer sizes, path components, and hash lengths are derived from base constants where possible (e.g., `CGIT_OBJ_NAME_BUF_SIZE` is derived from `CGIT_HASH_HEX_LEN`).

## Data Flow: Reading an Object

```
User: cgit cat-file blob abc123...

main.c
  → dispatch table lookup → handle_cat_file()

handle_cat_file (commands/cat_file.c)
  → validates argc, parses flags
  → calls read_object("abc123...", &obj)

read_object (core/object.c)
  → is_valid_hash()                     → utils.c
  → build_object_path()                 → utils.c
  → read_file()                         → utils.c
  → decompress_data()                   → compression.c
  → parse_object_header()               → utils.c
  → populates git_object_t, returns CGIT_OK

handle_cat_file
  → verifies type matches
  → prints content
  → free_object() in cleanup
```

## Data Flow: Writing a Tree

```
User: cgit write-tree

handle_write_tree (commands/write_tree.c)
  → calls write_tree_recursive(".")

write_tree_recursive (core/object.c)
  → opendir/readdir to scan directory
  → for each file: read_file → write_object("blob") → get hash
  → for each dir: write_tree_recursive(subdir) → get hash (recurse)
  → sort entries alphabetically
  → serialize entries to binary tree format
  → write_object("tree", content) → return tree hash

handle_write_tree
  → prints root tree hash
```

## Studied Reference: Real Git

The architecture was informed by studying git's source code, specifically:
- `git.c` — dispatch table pattern with `commands[]` array and `get_builtin`/`run_builtin` separation
- Bitwise option flags for per-command configuration (`RUN_SETUP`, `NEED_WORK_TREE`)
- Layered separation between `builtin/` (commands) and core library files (plumbing)
