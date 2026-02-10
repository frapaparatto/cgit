# Bitwise Operations for Configuration Flags

## The Idea

You can use bitwise operations to set configuration options. You use an unsigned
integer where each bit represents a certain aspect of the config. Then you check
each option with bitwise AND: if the bit is set, do something; if not, do
something else.

## How It Works — Three Operations

### 1. Define: each flag gets its own bit position

```c
#define RUN_SETUP      (1 << 0)   // bit 0  ->  0b0001  =  1
#define NEED_WORK_TREE (1 << 1)   // bit 1  ->  0b0010  =  2
#define SOME_OTHER     (1 << 2)   // bit 2  ->  0b0100  =  4
#define YET_ANOTHER    (1 << 3)   // bit 3  ->  0b1000  =  8
```

The `(1 << N)` notation shifts the number 1 left by N positions in binary.
Each flag lands on its own bit so they never collide.

### 2. Combine: OR flags together to build a configuration

```c
.option = RUN_SETUP | NEED_WORK_TREE
//  0b01 | 0b10  =  0b11  (both bits set)
```

OR means "turn on all the bits that are on in either operand". So you end up
with a single integer that carries multiple on/off switches.

### 3. Test: AND to check if a specific bit is set

```c
if (option & RUN_SETUP)        // 0b11 & 0b01 = 0b01  -> truthy (set)
if (option & NEED_WORK_TREE)   // 0b11 & 0b10 = 0b10  -> truthy (set)
if (option & SOME_OTHER)       // 0b11 & 0b100 = 0b000 -> falsy  (not set)
```

AND means "keep only the bits that are on in both operands". If the result is
non-zero, the flag is set. If zero, it's not.

## A Concrete Example

Say you're configuring a command that needs a repo and a work tree, but not the
other flags:

```c
unsigned int option = RUN_SETUP | NEED_WORK_TREE;
// option = 0b0011

if (option & RUN_SETUP)        // 0b0011 & 0b0001 = 0b0001 -> yes, do setup
    setup_git_directory();

if (option & NEED_WORK_TREE)   // 0b0011 & 0b0010 = 0b0010 -> yes, check tree
    check_work_tree();

if (option & SOME_OTHER)       // 0b0011 & 0b0100 = 0b0000 -> no, skip
    do_something_else();

if (option & YET_ANOTHER)      // 0b0011 & 0b1000 = 0b0000 -> no, skip
    do_yet_another_thing();
```

## Why This Pattern

- A single `unsigned int` holds up to 32 independent boolean flags.
- Testing a flag is one CPU instruction (AND + compare to zero).
- No arrays, no structs, no heap allocation — just one integer.
- Combining flags with OR is readable: `RUN_SETUP | NEED_WORK_TREE` reads almost
  like English.

This is everywhere in systems programming: Unix file permissions (`rwx`),
`open()` mode flags (`O_RDONLY | O_CREAT`), socket options, `fcntl` flags.
Git uses it to declare per-command setup requirements in the dispatch table.
