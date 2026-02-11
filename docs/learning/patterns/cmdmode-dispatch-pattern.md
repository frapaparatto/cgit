# The OPT_CMDMODE Dispatch Pattern

## The Problem

Real git's `cat-file` has a clever design problem: the same command supports two
completely different invocation styles:

```
cgit cat-file blob abc123        # Form A: <type> <object>  (positional args)
cgit cat-file -p abc123          # Form B: -flag <object>   (option + arg)
```

The challenge is: **how do you cleanly parse and dispatch both forms with a
single entry point?**

Git solves this with a pattern called **CMDMODE** — a single `int` variable that
acts as a **mode selector**. The key insight is using **the character itself**
(`'p'`, `'t'`, `'s'`, `'e'`) as the mode value, with `0` meaning "no flag was
given" (which implies Form A).

## Step-by-Step Breakdown

### 1. The Mode Variable

```c
int opt = 0;
```

This one variable carries all the information about which mode was requested:

| Value of `opt` | Meaning |
|---|---|
| `0` | No flag given → Form A (type-based: `cat-file blob <hash>`) |
| `'p'` (= 112) | `-p` flag → pretty-print content |
| `'t'` (= 116) | `-t` flag → print type name |
| `'s'` (= 115) | `-s` flag → print size |
| `'e'` (= 101) | `-e` flag → check existence |

The trick: `char` values are just small integers in C. `'p'` is 112, `'t'` is
116, etc. By storing the flag character directly, you get a human-readable switch
statement for free, and `0` is a natural "no flag" sentinel since no printable
ASCII character is `0`.

### 2. How Git's OPT_CMDMODE Works (and what cgit builds instead)

In real git, `OPT_CMDMODE` is a macro in the `parse_options` framework. It does
two things:

1. When `-p` is passed, it stores `'p'` into `opt`
2. It enforces **mutual exclusion** — if you pass both `-p` and `-t`, it errors
   out automatically

cgit doesn't have `parse_options`, but can replicate the exact same behavior
with a simple manual parser. Here's the equivalent logic:

```c
int opt = 0;
const char *obj_name = NULL;
const char *exp_type = NULL;

// argv[0] is "cat-file" (since main.c does argv + 1)
// argv[1] is either a flag (-p, -t, -s, -e) or a type name (blob, commit...)
// argv[2] is the object hash

// Step 1: Check if argv[1] is a flag
if (argc >= 2 && argv[1][0] == '-' && argv[1][1] != '\0' && argv[1][2] == '\0') {
    // It's a single-character flag like "-p"
    opt = argv[1][1];  // store 'p', 't', 's', or 'e'
}
```

That's the heart of it. The check `argv[1][0] == '-' && argv[1][2] == '\0'`
ensures we only match single-character flags. After this, `opt` is either `0`
(no flag → Form A) or a character like `'p'`.

### 3. The Two-Phase Dispatch

This is the most important design insight. The parsing happens in **two phases**:

**Phase 1 — Argument validation** (in `handle_cat_file`):
Determine which form was used and extract the right values.

```c
if (opt) {
    // Form B: flag-based. We need exactly 1 more positional arg (the object)
    // argv[1] was the flag, argv[2] should be the object hash
    if (argc != 3) { /* error: need exactly "cat-file -p <object>" */ }
    obj_name = argv[2];
} else {
    // Form A: type-based. We need exactly 2 positional args
    if (argc != 3) { /* error: need exactly "cat-file <type> <object>" */ }
    exp_type = argv[1];  // "blob", "commit", etc.
    obj_name = argv[2];
}
```

After Phase 1, you have three clean variables regardless of which form was used:

- `opt` — the mode (0 or 'p'/'t'/'s'/'e')
- `exp_type` — non-NULL only for Form A
- `obj_name` — always set (the hash)

**Phase 2 — Execution** (in a separate `cat_one_file` function):
A switch on `opt` that does the actual work.

```c
static int cat_one_file(int opt, const char *exp_type, const char *obj_name,
                        git_object_t *obj) {
    switch (opt) {
    case 't':
        printf("%s\n", obj->type);
        return 0;

    case 's':
        printf("%zu\n", obj->size);
        return 0;

    case 'e':
        // Object was already read successfully if we got here → exists
        return 0;

    case 'p':
        // Pretty-print: just dump content (same as Form A for now)
        fwrite(obj->data, 1, obj->size, stdout);
        return 0;

    case 0:
        // Form A: verify type matches, then print raw content
        if (strcmp(obj->type, exp_type) != 0) {
            fprintf(stderr, "fatal: expected %s, got %s\n", exp_type, obj->type);
            return 1;
        }
        fwrite(obj->data, 1, obj->size, stdout);
        return 0;
    }
    return 1;  // unreachable
}
```

### 4. Why This Design Is Powerful

**Separation of concerns.** Parsing (Phase 1) knows nothing about git objects.
Execution (Phase 2) knows nothing about command-line syntax. They communicate
through three simple variables.

**The switch is exhaustive and flat.** Every mode is a single `case`. No nested
if-else chains. Adding a new flag (say, `-x` for hex dump) means:

1. Add one line to the flag detection
2. Add one `case 'x':` block

**The `0` case is elegant.** Form A (no flag) naturally falls into `case 0`
because `int opt = 0` is the default. You don't need a separate boolean like
`is_form_a`.

**Mutual exclusion is trivial to enforce.** If you want to reject
`cat-file -p -t <hash>`:

```c
// During flag parsing:
if (opt != 0) {
    fprintf(stderr, "error: -%c and -%c are mutually exclusive\n",
            opt, argv[i][1]);
    return 1;
}
opt = argv[i][1];
```

### 5. How It Maps to the Current cgit Code

The current `handle_cat_file` only handles Form A. Here's a conceptual diff
showing what changes:

```
 BEFORE (Form A only):              AFTER (both forms):
 ─────────────────────              ───────────────────
 argv[1] = type                     argv[1] = flag OR type
 argv[2] = hash                     argv[2] = hash (in both cases)
                                    ↓
 read_object(hash, &obj)            opt = detect_flag(argv[1])
 strcmp(obj.type, type)              read_object(hash, &obj)
 fwrite(...)                        switch(opt) { ... }
```

The beautiful part: the `argc == 3` check already works for both forms since
both `cat-file -p <hash>` and `cat-file blob <hash>` have exactly 3 tokens.

### 6. The `-e` Flag Is Special

Notice that `-e` (check existence) doesn't need to read the full object content.
In real git, it only checks if the object file exists. For the cgit
implementation, you could:

- Try `build_object_path()` and check if the file exists with
  `access(path, F_OK)`
- Or just try `read_object()` and treat success/failure as the answer

The `-e` case also has a different return convention: it returns `0` for "yes,
exists" and `1` for "no, doesn't exist" — it prints **nothing**. This is
designed for scripting (`if cgit cat-file -e $hash; then ...`).

### 7. The Big Picture — Why `char` Values as Mode Selectors

This pattern appears throughout git's codebase, not just in `cat-file`. It's a C
idiom for "option-as-enum-without-defining-an-enum":

```c
// Instead of:
typedef enum { MODE_PRETTY, MODE_TYPE, MODE_SIZE, MODE_EXISTS } mode_t;

// Git uses:
int opt = 'p';  // self-documenting: 'p' obviously means -p
```

Advantages:

- No enum definition needed
- The switch cases read like the actual CLI flags
- `0` is a natural "none" value
- You can print the mode directly in error messages: `"unknown flag: -%c", opt`

This works whenever your modes correspond to single-character flags. If you ever
had a long option (`--batch`), you'd use a different integer value (like `256+`
to avoid colliding with ASCII).

## Reference

See `refactoring/git-cat-file-reference.c` for the annotated pseudocode showing
how real git structures `cmd_cat_file()` and `cat_one_file()` using this pattern.
