# cgit

A reimplementation of git's core commands in C. I built this to deeply understand git's internals and object model by studying the real git source — not to clone it, but to understand *why* it is designed the way it is. Every architectural decision here was made after reading the original code and asking what problem it was solving.

## Architecture

The project is organized in three layers, each with a strict contract:

```
src/
├── main.c              — entry point, dispatch table
├── commands/           — argument parsing and validation, delegates to core
├── core/               — object model: hashing, compression, read/write, tree/commit
└── include/
    ├── common.h        — shared primitives (buffer_t, cgit_error_t, constants)
    ├── core.h          — object model interface
    └── commands.h      — command handler declarations
```

**Dispatch table** (`main.c`): commands are registered in a `command_t` array — each entry holds a name, a function pointer, and a usage string. A null-sentinel terminates the array. The main loop iterates it with `strcmp` and calls the matching handler. This mirrors how real git resolves builtins in `git.c` via `get_builtin()`: lookup and execution are separated, and the table is the only place where a command's identity is defined.

**Layered headers**: `common.h` defines primitives visible everywhere (`buffer_t`, `cgit_error_t`, path constants). `core.h` exposes the object model to the command layer. `commands.h` exposes only handler signatures to `main.c`. Each layer sees exactly what it needs and nothing more.

## Implemented Commands

| Command | Usage |
|---|---|
| `init` | `cgit init` |
| `hash-object` | `cgit hash-object [-w] <file>` |
| `cat-file` | `cgit cat-file <type \| -p \| -t \| -e \| -s> <object>` |
| `ls-tree` | `cgit ls-tree [--name-only] <object>` |
| `write-tree` | `cgit write-tree` |
| `commit-tree` | `cgit commit-tree <tree-hash> [-p <parent-hash>] -m <message>` |

## Design Decisions

**`buffer_t` and `goto cleanup`**

Every function that allocates memory uses a single `buffer_t` (a growable byte buffer: `data`, `size`, `capacity`) and a single `goto cleanup` label at the end. Instead of placing `free()` calls at every error path, all cleanup is centralized. This is the pattern used throughout the real git codebase and it keeps error-handling paths readable and correct — a mistake at any intermediate step jumps to the same place.

**`cgit_error_t` as the universal return type**

All core functions return a typed enum (`CGIT_OK`, `CGIT_ERROR_IO`, `CGIT_ERROR_MEMORY`, etc.) rather than raw integers or booleans. Callers propagate errors explicitly without relying on global state. This forced me to think about every failure mode at the boundary between layers.

**What I learned about git's object format**

A loose object is stored as: a zlib-compressed payload whose uncompressed form is `"<type> <size>\0<content>"`. The SHA-1 hash is computed over the *uncompressed* header+content, before any compression happens. The object is then written to `.git/objects/<first-2-hex>/<remaining-38-hex>`. Implementing `write_object` and `read_object` from scratch — including the zlib decompression and the header parsing — made this format completely concrete for me in a way that reading docs alone never would have.

**Opaque tree serialization**

`write-tree` recursively walks the working directory, builds an array of `tree_entry_t` structs, serializes them in git's binary format (mode + space + name + null byte + 20-byte raw SHA-1), then writes the result as an object. Understanding why the binary hash (not hex) is embedded in the tree body — and that the hex representation is only for human-facing output — was a non-obvious detail that took reading `tree.c` in the real source to fully understand.

**Dependency choices**

OpenSSL for SHA-1, zlib for compression — the same libraries real git uses. The CMake build auto-detects the OpenSSL prefix on macOS via `brew --prefix openssl` so the build works without manual configuration.

## Limitations & Future Work

- **Hardcoded identity**: author and committer name/email are compile-time constants in `common.h`. There is no config file parsing yet.
- **No ref resolution**: objects are always addressed by their full SHA-1 hex hash. There is no `HEAD` dereferencing, no branch tracking, no `refs/` resolution.
- **No index**: the staging area (`.git/index`) is not implemented. `write-tree` operates directly on the working directory.
- **No history traversal**: `log`, `diff`, and `status` are not yet implemented.
- **Single-threaded, no pack files**: only loose objects are supported.

## Build

```bash
cmake -B build && cmake --build build
./build/cgit <command>
```

**Dependencies**: CMake ≥ 4.2, OpenSSL, zlib. On macOS, OpenSSL is detected automatically via Homebrew.
