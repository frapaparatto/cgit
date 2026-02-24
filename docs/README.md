# cgit Documentation

## How to Read This

This project was built with deliberate attention to engineering decisions. The documentation is structured to show not just what was built, but why each choice was made and what alternatives were considered.

### Architecture

Start here to understand the system design:

- **[Architecture Overview](architecture/overview.md)** — Layered structure, module map, data flow diagrams, conventions.
- **[Decision Records](architecture/decisions/)** — Each significant design choice documented with context, rationale, and alternatives:
  - [001 - Dispatch Table](architecture/decisions/001-dispatch-table.md) — why data-driven dispatch over if/else chains
  - [002 - Error Handling Strategy](architecture/decisions/002-error-handling-strategy.md) — two-level error model with typed error codes
  - [003 - Ownership Model](architecture/decisions/003-ownership-model.md) — explicit resource lifecycle and the goto cleanup pattern
  - [004 - Object Existence Check](architecture/decisions/004-object-existence-check.md) — separating a fast path from a general-purpose function
  - [005 - Tree Parsing in Core](architecture/decisions/005-tree-parsing-in-core.md) — why parsing belongs in core, not in command handlers
  - [006 - Write Idempotency](architecture/decisions/006-write-idempotency.md) — performance-conscious design decisions
  - [007 - Header Parsing Extraction](architecture/decisions/007-header-parsing-extraction.md) — extracting a pure utility from mixed I/O code
  - [008 - Buffer Append Pattern](architecture/decisions/008-buffer-append-pattern.md) — growable buffer design and the bug that revealed it
  - [009 - Derived Constants](architecture/decisions/009-derived-constants.md) — self-documenting values over magic numbers
  - [010 - SHA-1 Implementation](architecture/decisions/010-sha1-implementation.md) — OpenSSL choice and API deprecation tradeoff
- **[Bugs and Lessons](architecture/bugs-and-lessons.md)** — Concrete bugs found during development, their root causes, and what they taught.

### Learning Notes

Personal reference material created while studying git internals. These capture the understanding that informed the implementation:

- `learning/errno.md` — errno semantics, common bugs, perror vs strerror
- `learning/ls-tree.md` — tree object format and ls-tree command analysis
- `learning/write-tree.md` — recursive tree creation and bottom-up hashing

## Development Approach

The project followed a deliberate process:

1. **Understand before implementing** — each command was studied conceptually (how git does it, what the binary formats look like) before writing code.
2. **Explain to verify understanding** — the Feynman Technique was used throughout: explain the concept, receive feedback, correct misunderstandings, then implement.
3. **Study real git source** — architectural decisions were validated against git's actual codebase (`git.c` dispatch table, `builtin/` command separation, core library layering).
4. **Refactor toward principles** — the codebase evolved from a monolithic script to a layered architecture by applying seven identified principles: module pattern, opaque types, resource lifecycle, error codes, dispatch tables, layered dependencies, and named constants.
5. **Document bugs honestly** — bugs were recorded with root causes and lessons rather than quietly fixed, creating a record of growing understanding.
