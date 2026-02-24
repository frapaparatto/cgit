# 002: Error Codes + Output Parameters as the Universal Convention

## Context

The original code had inconsistent error handling: mixed use of `fprintf(stderr, ...)` and `perror()`, no centralized error vocabulary, and callers couldn't distinguish between failure reasons (file not found vs invalid hash vs out of memory).

## Decision

Adopted a two-level error handling convention:

**Core functions** return `cgit_error_t` (an enum) with specific error codes: `CGIT_OK`, `CGIT_ERROR_INVALID_ARGS`, `CGIT_ERROR_FILE_NOT_FOUND`, `CGIT_ERROR_MEMORY`, `CGIT_ERROR_INVALID_OBJECT`, `CGIT_ERROR_IO`, `CGIT_ERROR_COMPRESSION`. Results are delivered through output pointer parameters, never through return values.

**Command handlers** translate `cgit_error_t` into user-facing error messages (using `fprintf(stderr, ...)` with `strerror()` for system errors) and return 0 or 1 to the dispatch table.

This creates a clean separation: core communicates *what went wrong* with precise codes, handlers decide *how to tell the user*.

Chose `fprintf + strerror` over `perror` as the default for user-facing messages because it allows including specific context (filenames, hashes, paths) in the error output. `perror` only accepts a static prefix string.

## Consequences
- Every core function has the same shape: returns `cgit_error_t`, writes results through pointers. This makes the API predictable.
- Callers can make decisions based on specific error codes (e.g., `CGIT_ERROR_FILE_NOT_FOUND` triggers a different message than `CGIT_ERROR_INVALID_OBJECT`).
- The `goto cleanup` pattern works naturally because every function returns an error code that can be set at any failure point and checked at cleanup.
- Operations are transactional: on failure, caller state is untouched. Either the operation fully succeeds and delivers a result, or it fails and changes nothing.
