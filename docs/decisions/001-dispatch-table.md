# 001: Command Dispatch Table Over If/Else Chains

## Context

The original `main.c` used an `if/else if` chain to route commands to their implementation code. All command logic lived inline within these branches, creating a 486-line monolithic function. Adding a command meant modifying `main()` directly, with risk of breaking existing branches.

## Decision

Replaced the `if/else if` chain with a static array of `command_t` structs (name, function pointer, usage string) and a generic lookup loop. Each command's implementation lives in a separate file under `commands/`.

Studied git's `git.c` to validate the pattern. Git uses the same approach but with a two-step split: `get_builtin` (finds the command in the table, returns the struct) and `run_builtin` (reads option flags, performs conditional setup, then calls the handler). This separation exists because different commands need different pre-execution setup (working tree check, repository detection), and centralizing that logic in the runner avoids duplicating it in every handler.

Also studied git's use of bitwise flags for per-command configuration, each command carries an `option` field (unsigned int) where each bit represents a requirement (`RUN_SETUP`, `NEED_WORK_TREE`, etc.). Flags are combined with `|` and tested with `&`. This is the standard C pattern for packing multiple booleans into a single integer.

## Alternatives Considered

- **Switch statement**: Marginally cleaner than `if/else if` but still requires modifying the switch body for each new command. Same fundamental problem.
- **Function pointer array without struct**: Would work for dispatch but loses the metadata (name matching, usage strings). The struct groups related data with behavior.

## Consequences

- `main()` is finished, the loop will never change regardless of how many commands are added.
- Adding a command requires only: writing a new `.c` file, adding one line to the array, declaring the handler in `commands.h`.
- Zero modification to existing code when extending (Open/Closed Principle).
- The sentinel value `{NULL, NULL, NULL}` terminates the array, enabling safe iteration without a separate count variable.
