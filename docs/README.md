# cgit Documentation

## How to Read This

This project was built with deliberate attention to engineering decisions. The documentation is structured to show not just what was built, but why each choice was made and what alternatives were considered.

### Architecture

Start here to understand the system design:

- **Architecture Overview**: Layered structure, module map, data flow diagrams, conventions.
- **Decision Records**: Each significant design choice documented with context, rationale, and alternatives:
  - 001 - Dispatch Table: why data-driven dispatch over if/else chains
  - 002 - Error Handling Strategy: two-level error model with typed error codes
  - 003 - Ownership Model: explicit resource lifecycle and the goto cleanup pattern
  - 004 - Object Existence Check: separating a fast path from a general-purpose function
  - 005 - Tree Parsing: why parsing belongs in core, not in command handlers
  - 006 - Write Idempotency: performance-conscious design decisions
  - 007 - Header Parsing Extraction: extracting a pure utility from mixed I/O code
  - 008 - Buffer Append Pattern: growable buffer design and the bug that revealed it

## Development Approach

The project followed a deliberate process:

1. **Understand before implementing**: each command was studied conceptually (how git does it, what the binary formats look like) before writing code.
2. **Explain to verify understanding**: the Feynman Technique was used throughout: explain the concept, receive feedback, correct misunderstandings, then implement.
3. **Study real git source**: architectural decisions were validated against git's actual codebase (`git.c` dispatch table, `builtin/` command separation, core library layering).
4. **Refactor toward principles**: the codebase evolved from a monolithic script to a layered architecture by applying seven identified principles: module pattern, opaque types, resource lifecycle, error codes, dispatch tables, layered dependencies, and named constants.
5. **Document bugs**: bugs were recorded with root causes and lessons rather than quietly fixed, creating a record of growing understanding.
