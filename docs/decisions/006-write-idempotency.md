# 006: Idempotent Object Writes and Performance-Conscious Design

## Context

Several places in the codebase were doing unnecessary work. `write_object` unconditionally compressed and wrote to disk even when the object already existed (content-addressed storage means identical hashes guarantee identical content). `hash-object` without `-w` still went through the full write path. `cat-file -e` read and decompressed an entire object just to confirm it exists.

These weren't bugs, they produced correct results but they represented wasted I/O and computation.

## Decision

Applied three targeted optimizations:

1. **`write_object` idempotency guard**: after computing the hash and building the path, a `stat()` call checks if the file already exists. If it does, the function skips directory creation, compression, and file writing, jumping straight to cleanup. The hash is still computed and returned via `hash_out`. This matches real git's behavior.

2. **`hash-object` persistence flag**: the `-w` flag controls whether the object is written to disk. Without it, only the hash is computed and printed. The write path is skipped entirely, avoiding unnecessary compression and I/O.

3. **`object_exists` for `-e` flag** (see ADR-004): existence checks use `access()` instead of full `read_object`.

## Alternatives Considered

- **Always write unconditionally**: correct but wasteful. In `write-tree`, the same blob might be encountered in multiple directories. Without the idempotency guard, each occurrence triggers a redundant compress + write cycle.

## Consequences

- Object writes are safe to call repeatedly with the same content, no performance penalty, no race condition issues.
- `hash-object` without `-w` is purely computational, no disk side effects.
- The performance-conscious decisions don't add complexity to callers. The optimization is internal to core functions; the API is unchanged.
