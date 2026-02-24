# ADR-007: Extracting Header Parsing as a Pure Utility Function

## Context

`read_object` was doing two conceptually separate things: I/O operations (reading file, decompressing) and parsing the object header (`type SP size NUL`). The parsing logic was embedded directly in the I/O flow, making it impossible to reuse for cases where you have a raw buffer but didn't read it from disk (e.g., parsing objects received in a packfile).

## Decision

Extracted `parse_object_header` into `utils.c` as a pure parsing function. It takes a raw buffer and length, returns the parsed type string, content size, and payload offset through output pointers.

Key design properties:
- **No allocations, no I/O**: purely operates on the buffer it receives. This makes it easy to test and impossible to leak.
- **Safe bounds checking**: takes `buf_len` to prevent reading beyond the buffer, even on malformed input.
- **Uses `const unsigned char *buf`**: matches `buffer_t.data` type for natural composition.

Simplified `read_object` to delegate header parsing and use `strdup` for the type string instead of manual `malloc` + `memcpy`.

Added `CGIT_MAX_TYPE_LEN` constant (set to 16) to replace the magic number `32` used for the type buffer. Covers all standard git types (blob, tree, commit, tag) with margin.

## Alternatives Considered

- **Keep parsing inline in `read_object`**: works until you need header parsing elsewhere. The packfile parsing stage of `clone` will need to inspect object headers without going through `read_object`'s file I/O path.
- **Return a struct instead of multiple output pointers**: would require allocating the struct or having the caller declare it. Output pointers are simpler for a function that returns three independent scalar values.

## Consequences

- `read_object` is now a clean orchestrator: read → decompress → parse header → validate size → populate struct.
- `parse_object_header` is available for any code path that has a raw object buffer, independent of how that buffer was obtained.
- The allocation-free design means callers don't need cleanup logic for the parsing step itself.
