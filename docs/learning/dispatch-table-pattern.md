# Dispatch Table Pattern

## Why

In a program like git you have many commands (init, cat-file, hash-object, ...),
and each one maps to a function. The naive approach is a big if/else or switch
chain that calls the right function directly. The problem is that this mixes
**lookup** (which command did the user type?) with **execution** (run the command).

With a dispatch table you separate these two steps:

1. **Lookup** — search the table for the command name, return the whole entry.
2. **Execution** — read the entry's flags, do conditional setup, THEN call the
   function.

This separation matters because different commands need different setup before
running. For example `cat-file` needs a valid repository (`RUN_SETUP`) while
`init` creates one from scratch. If you only stored function pointers, every
command would have to handle its own setup internally, duplicating logic
everywhere. Instead, the flags let a central runner (`run_builtin`) handle
common setup — a clean separation of concerns.

## What It Needs

The pattern requires a few building blocks:

- **A struct** that holds together: the command name, a function pointer, and
  option flags.
- **All command functions must have the same signature** (same interface), e.g.
  `int (*fn)(int argc, const char **argv)`. This is what makes the table
  possible — you can call any entry the same way.
- **An array of these structs** — the actual table. Each entry declares its
  name, its function, and what setup it needs via flags.
- **A lookup function** that iterates the array and returns a pointer to the
  matching struct (not just the function — the whole struct, because the caller
  needs the flags too).
- **A runner function** that reads the flags, performs conditional setup, and
  then calls `fn`.

## How It Works

```
user types "cat-file"
        |
        v
  get_builtin("cat-file")         <-- step 1: lookup
        |
        v
  returns &{ "cat-file", cmd_cat_file, RUN_SETUP }
        |
        v
  run_builtin(entry, argc, argv)  <-- step 2: execution
        |
        +-- check entry->option & RUN_SETUP  --> yes, init repo
        +-- check entry->option & NEED_WORK_TREE --> no, skip
        |
        v
  entry->fn(argc, argv)           <-- actual command runs
```

The key point: the runner inspects the flags **before** calling the function.
This is why `get_builtin` returns the whole struct, not just the function
pointer.

## Basic Implementation (from sample_dispatch.c)

```c
#define RUN_SETUP      (1 << 0)
#define NEED_WORK_TREE (1 << 1)

struct cmd_struct {
    const char *cmd;
    int (*fn)(int argc, const char **argv);
    unsigned int option;
};

static struct cmd_struct commands[] = {
    { "init",        cmd_init,        RUN_SETUP | NEED_WORK_TREE },
    { "cat-file",    cmd_cat_file,    RUN_SETUP },
    { "hash-object", cmd_hash_object, RUN_SETUP },
};

/* Step 1: lookup — returns the whole struct */
static struct cmd_struct *get_builtin(const char *s)
{
    for (size_t i = 0; i < ARRAY_SIZE(commands); i++) {
        struct cmd_struct *p = &commands[i];
        if (!strcmp(s, p->cmd))
            return p;
    }
    return NULL;
}

/* Step 2: setup + execution */
static int run_builtin(struct cmd_struct *builtin, int argc, const char **argv)
{
    if (builtin->option & RUN_SETUP)
        printf("  [setup] initializing repository...\n");

    if (builtin->option & NEED_WORK_TREE)
        printf("  [setup] checking work tree exists...\n");

    return builtin->fn(argc, argv);
}
```

## Differences with Real Git

The core idea is the same — real git uses exactly this pattern in `git.c`. The
main differences:

- Real git's `commands[]` table has **hundreds** of entries, not three.
- The setup phase in `run_builtin` does real work: calling `setup_git_directory()`
  to find `.git/`, loading config, verifying the work tree exists, etc.
- There are more flags beyond `RUN_SETUP` and `NEED_WORK_TREE` (e.g.
  `RUN_SETUP_GENTLY` for commands that can work with or without a repo).
- Real git also handles external commands (not in the table) by falling back to
  `execv_dashed_external()`.

But the **why** is identical: separate lookup from execution so that a central
runner can do conditional setup based on per-command flags before calling the
actual function.
