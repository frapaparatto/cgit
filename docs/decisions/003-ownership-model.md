# 003: Explicit Ownership Model and Resource Lifecycle

## Context

The original code had inconsistent resource management: `cat-file` used scattered `free(); return 1;` at each error point while `hash-object` used `goto cleanup`. This inconsistency caused memory leaks in error paths — each early return had to remember which resources had been allocated so far, and missing one meant a leak.

Additionally, the zlib `inflateEnd`/`deflateEnd` calls were made unconditionally in cleanup, even when `inflateInit`/`deflateInit` had never run or had failed — undefined behavior per the zlib spec.

## Decision

Adopted an explicit ownership model with three rules:

1. **Every allocation has a single, clear owner.** At any point, exactly one piece of code is responsible for freeing a given resource. Ownership can transfer (callee → caller on success) but is never ambiguous.

2. **Every resource-owning type gets a matching free function.** `read_object` → `free_object`. `parse_tree` → `free_tree_entries`. `buffer_create` → `buffer_free`. The free function must be safe to call with NULL/zeroed input.

3. **One exit path per function through `goto cleanup`.** All resources are freed in reverse order of creation. Resources that were never successfully created are safe to "free" because the destroy functions handle NULL.

**Ownership transfer rule:** ownership moves from callee to caller at the moment the function returns success. If the function fails, it cleans up its own allocations — the caller receives nothing and its state is unchanged.

**Initialization guard for conditional resources:** resources that may or may not be initialized (like zlib streams) use a boolean guard (`int strm_initialized = 0;`) set to 1 after successful init. The cleanup section checks the guard before calling the teardown function.

## Consequences

- `git_object_t obj = {0};` at declaration means cleanup is always safe — `free_object` on a zeroed struct is a no-op.
- Adding a new resource to a function means: declare it at the top (zeroed), add its cleanup at the bottom, use it in between. No other changes needed.
- The zlib undefined behavior is eliminated — `inflateEnd` is only called when `inflateInit` actually succeeded.
- The pattern is consistent across all handlers and core functions, making the codebase predictable.
