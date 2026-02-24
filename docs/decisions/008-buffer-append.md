# 008: Growable Buffer Pattern and the Reinitialization Bug

## Context

Building git object content (particularly commit objects) requires assembling a string incrementally, one line at a time. This needs a growable buffer that can be appended to repeatedly.

The initial `buffer_append_fmt` implementation had a critical bug: it called `malloc` and reset `buf->size = 0` unconditionally on every call. This meant each append leaked the previous allocation and discarded all previously written data. A commit object built with multiple calls would only contain the last line.

## Decision

Fixed the buffer pattern with a clear contract:

- **Initialize once**: the buffer is set up on the first call only (`if (!buf->data)`). Subsequent calls append to existing content.
- **Measure before writing**: `vsnprintf(NULL, 0, fmt, args)` computes the exact byte count needed without writing. This avoids fixed intermediate buffers that could silently truncate.
- **Grow on demand**: if remaining capacity is insufficient, `realloc` doubles the buffer (or grows to fit, whichever is larger).
- **Write at offset**: new content is written at `buf->data + buf->size`, and `buf->size` is incremented by the formatted length.

The function is `static` inside `object.c`, it's only used by `build_commit_content` and doesn't need external visibility.

## Consequences

- `build_commit_content` is clean: one `buffer_append_fmt` call per line of the commit format.
- The buffer grows automatically, callers don't manage capacity.
- The double-`vsnprintf` pattern (measure then write) is a standard C idiom for safe formatted output into dynamic buffers.
- The bug reinforced an important lesson: every function must be designed with its *calling pattern* in mind, not just its single-invocation behavior.
