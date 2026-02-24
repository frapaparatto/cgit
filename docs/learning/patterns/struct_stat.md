# `struct stat` and `stat()`

## The problem it solves

You often need to ask the OS questions about a file **without opening it**: Does it exist? How big is it? Is it a regular file or a directory? When was it last modified? The `stat()` function answers all of these in one syscall by filling a `struct stat` with the file's metadata.

## `stat()` function

```c
#include <sys/stat.h>

int stat(const char *path, struct stat *buf);
```

**Returns:**
- `0` on success (file exists, metadata written into `buf`)
- `-1` on failure (file doesn't exist, permission denied, etc.) and sets `errno`

So `stat(path, &st) == 0` is idiomatically "does this file exist and can I access it?"

## `struct stat`

It's a struct defined by the OS that holds file metadata. The key fields:

| Field | Type | Meaning |
|---|---|---|
| `st_size` | `off_t` | File size in bytes |
| `st_mode` | `mode_t` | File type + permissions (regular file, directory, symlink, etc.) |
| `st_mtime` | `time_t` | Last modification time |
| `st_ino` | `ino_t` | Inode number |
| `st_nlink` | `nlink_t` | Number of hard links |

## How it's used in cgit

**Idempotency check** (`src/core/object.c`):
```c
struct stat st;
if (stat(path, &st) == 0) goto cleanup;
```
We only care whether `stat` returns 0 (file exists) — we don't read any fields from `st`. We just need to declare it because `stat()` requires a pointer to write into.

**Getting file size** (`src/core/utils.c`):
```c
struct stat st;
if (stat(path, &st) != 0) { /* error */ }
size_t file_size = (size_t)st.st_size;
```
Here we actually use `st.st_size` to know how many bytes to `malloc` before reading the file.

## Why not just `fopen` to check existence?

`stat()` is lighter — it only reads the file's inode metadata without opening a file descriptor. `fopen` allocates a `FILE*`, acquires a file descriptor, and needs `fclose` cleanup. For a simple "does it exist?" check, `stat` is the right tool.
