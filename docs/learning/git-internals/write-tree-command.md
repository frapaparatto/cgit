# How `write-tree` Command Works in Git

## The Core Idea

`write-tree` takes the current working directory and creates a tree object that represents its entire structure. It works recursively — bottom-up — producing blob objects for files and tree objects for directories, returning the root tree's SHA-1 hash.

## Tree Object Format

While a blob object is stored as `<type> <size>\0<content>`, a tree object is stored as:

```
tree <size>\0<mode> <name>\0<20_byte_sha><mode> <name>\0<20_byte_sha>...
```

Each entry is `<mode> <name>\0<20_byte_sha>` where:
- `<mode>` is the ASCII mode string (e.g. `40000`, `100644`) — no leading zeros in storage
- `<name>` is the file/directory name
- `\0` is a null byte separator
- `<20_byte_sha>` is the raw 20-byte binary SHA-1 hash (NOT the 40-char hex)

Entries are sorted alphabetically by name.

## The Recursive Flow

The function takes a directory path and returns a hash. Two cases:
- If entry is a file → create a blob object (same as hash-object), return its hash
- If entry is a directory → recurse into it, collect child entries, build tree content, write tree object, return its hash

```
write_tree(".")
├── stat "dir1" → directory
│   └── write_tree("dir1")                    ← recurse
│       ├── stat "file_in_dir_1" → file
│       │   └── hash-object → blob_sha_1
│       ├── stat "file_in_dir_2" → file
│       │   └── hash-object → blob_sha_2
│       └── build tree content, write object → tree_sha_1
├── stat "dir2" → directory
│   └── write_tree("dir2")                    ← recurse
│       ├── stat "file_in_dir_3" → file
│       │   └── hash-object → blob_sha_3
│       └── build tree content, write object → tree_sha_2
├── stat "file1" → file
│   └── hash-object → blob_sha_1
└── build tree content from [dir1, dir2, file1]
    write tree object → root_tree_sha (printed to stdout)
```

Each recursive call produces a hash. The parent uses those hashes as the `<20_byte_sha>` in its entries. Everything flows upward — deepest directories first, root last.

## Key Implementation Details

### The Header Comes Last

The tree object header (`tree <size>\0`) can't be written until the total content size is known. The content size can't be known until all entries are processed. For directory entries, this means all subtrees must be fully resolved first. This is why the process is naturally bottom-up.

### Mode Handling

Modes come from the filesystem via `stat()`. When you stat a file, you can determine if it's regular, executable, symlink, or directory. This gives you the mode directly — no need to convert from type.

Storage format vs display format are separate concerns:
- **Storage**: raw number without padding (`40000`, `100644`)
- **Display** (ls-tree): with leading zeros using `printf("%06o", mode)`

These are two different operations in two different places in the code. Not an inconsistency.

### SHA-1 Format: Hex vs Raw Bytes

`compute_sha1` produces 40-char hex. That's its single, well-defined output. 

The tree binary format requires 20 raw bytes. The conversion from hex to raw bytes is a small utility function (`hex_to_bytes` in utils.c). Two hex chars become one byte.

Flow: `compute_sha1` → 40-char hex → `hex_to_bytes` → 20 raw bytes → write into tree entry.

The reverse (raw bytes to hex) is what `parse_tree` already does when reading tree entries.

Principle: each function produces one well-defined output format. Callers convert if they need something different.

### Directory Filtering

`write-tree` must skip the `.cgit` directory during traversal. Same as real git skips `.git`.

### Sorting

Entries in tree content must be sorted alphabetically by name. This is a write-time concern — when reading (ls-tree), entries are already sorted.

## Building the Tree Content

For each directory entry, the binary content needs: mode, name, and the 20-byte raw hash. Two approaches:
- Store entries in an intermediate struct, sort them, then serialize to binary
- Build the binary content directly into a buffer as entries are collected

Consider: if building directly into a buffer, entries still need to be sorted alphabetically. This means you either need to read all entries first (to sort them) before writing any, or sort the directory listing before processing. An intermediate representation may be simpler to reason about.

## Reuse of Existing Core Functions

- `write_object` — once tree content bytes are assembled, writing them as a git object is the same process as hash-object: prepend header, compute SHA-1, compress, store
- `read_file` — for reading file contents when creating blobs
- `compute_sha1` — for hashing both blob and tree objects
- `hex_to_bytes` — new utility needed for converting hex hash to raw bytes for tree entry storage

The new piece is building the tree content — collecting entries from the filesystem, determining modes, sorting by name, serializing into the binary format. This is git object knowledge and belongs in core alongside `parse_tree` (the reading side).

## Architecture Summary

```
handle_write_tree (commands/write_tree.c)
  │
  └── write_tree_recursive (core/object.c)
        │
        ├── for each file:
        │     read_file → write_object("blob", ...) → get hash
        │
        ├── for each directory:
        │     write_tree_recursive(subdir) → get hash    ← recurse
        │
        ├── sort entries alphabetically
        ├── serialize entries to binary format (using hex_to_bytes for hashes)
        └── write_object("tree", content, ...) → return tree hash
```

The handler's job is minimal: validate args, call the recursive function on ".", print the resulting hash. All the logic lives in core.

## Code Walkthrough

### `handle_write_tree` — the command entry point

```c
int handle_write_tree(int argc, char *argv[]) {
  int result = 1;
  tree_entry_t *entries = NULL;
  size_t count = 0;
  char *curr_dir_path = ".";
  buffer_t out = {0};
  int persist = 1;
  char hash_out[CGIT_HASH_HEX_LEN + 1];

  cgit_error_t err = write_tree_recursive(curr_dir_path, &entries, &count);
  ...
  cgit_error_t err_serialize = serialize_tree(entries, count, &out);
  ...
  cgit_error_t err_writing =
      write_object(out.data, out.size, "tree", hash_out, persist);
  ...
  result = 0;
cleanup:
  buffer_free(&out);
  free_tree_entries(entries, count);
  return result;
}
```

The handler orchestrates three sequential steps:
1. `write_tree_recursive(".")` — walks the filesystem, writes blob/tree objects for every entry, returns a flat array of `tree_entry_t` for the root level
2. `serialize_tree` — sorts that array and encodes it into the binary tree content format
3. `write_object` — wraps the content in the git object header, computes the SHA-1, compresses and writes to disk

All resources (`out` buffer, `entries` array) are declared at the top and freed unconditionally in the single `cleanup` label — the standard cleanup pattern used throughout the codebase.

---

### `write_tree_recursive` — the recursive filesystem walker

```c
cgit_error_t write_tree_recursive(const char *path, tree_entry_t **entries_out,
                                  size_t *count_out) {
  cgit_error_t result = CGIT_OK;
  tree_entry_t *entries = NULL;
  size_t count = 0;
  int persist = 1;
  buffer_t buf = {0};
  tree_entry_t *sub_entries = NULL;
  size_t sub_count = 0;

  DIR *dir = opendir(path);
  ...
  struct dirent *dir_entry;
  while ((dir_entry = readdir(dir)) != NULL) {
    // skip . .. .cgit
    ...
    // stat the entry to determine mode and type
    struct stat st;
    if (stat(sub_path, &st)) goto cleanup;
    switch (st.st_mode & S_IFMT) {
      case S_IFDIR: mode = 40000;  type = "tree"; break;
      case S_IFREG: mode = (st.st_mode & S_IXUSR) ? 100755 : 100644; type = "blob"; break;
      case S_IFLNK: mode = 120000; type = "blob"; break;
      default: result = CGIT_ERROR_INVALID_OBJECT; goto cleanup;
    }

    // grow the entries array and populate the new slot
    tree_entry_t *tmp = realloc(entries, (count + 1) * sizeof(tree_entry_t));
    ...
    entry->type = strdup(type);
    entry->mode = mode;
    entry->name = malloc(dir_entry->d_namlen + 1);
    ...

    if (strcmp(entry->type, "blob") == 0) {
      result = read_file(sub_path, &buf);
      ...
      result = write_object(buf.data, buf.size, type, entry->hash, persist);
      ...
    } else if (strcmp(entry->type, "tree") == 0) {
      result = write_tree_recursive(sub_path, &sub_entries, &sub_count);
      ...
      result = serialize_tree(sub_entries, sub_count, &buf);
      ...
      result = write_object(buf.data, buf.size, "tree", entry->hash, persist);
      ...
      free_tree_entries(sub_entries, sub_count);
      sub_entries = NULL;
      sub_count = 0;
    }

    buffer_free(&buf);
    count++;
  }

  *entries_out = entries;
  *count_out = count;
  return result;

cleanup:
  buffer_free(&buf);
  free_tree_entries(sub_entries, sub_count);
  free_tree_entries(entries, count);
  if (dir) closedir(dir);
  return result;
}
```

A few things worth noting:

**Mode from `stat`**: `st.st_mode` is a bitmask. `S_IFMT` is a mask for the file-type bits. The `& S_IFMT` strips all permission bits and gives you only the type: directory, regular file, or symlink. For regular files you additionally check the execute bit (`S_IXUSR`) to pick between `100644` and `100755`.

**`entry->hash` is filled inline**: `write_object` writes the resulting hex hash directly into `entry->hash`. By the time the loop moves on to the next entry, each `entry` already carries its own hash — ready to be serialized by the caller.

**Buffer reuse across loop iterations**: `buf` is declared once at function scope. After each entry is processed successfully, `buffer_free(&buf)` is called — which zeroes the struct so it's ready to be re-allocated on the next iteration. On error paths, the single `cleanup` label frees `buf` (idempotent since `buffer_free(zeroed)` is a no-op). This eliminates the need to call `buffer_free` before every `goto cleanup`.

**Tree branch resets `sub_entries`**: after recursing into a subdirectory, `sub_entries` is freed and reset to `NULL`/`0` at the end of the branch. If it weren't reset, a later error-path `free_tree_entries(sub_entries, sub_count)` in cleanup would double-free already-freed memory.

**Output params vs return value**: the function returns `cgit_error_t` for error signaling. The actual output (the array of entries) is returned through output parameters `entries_out` and `count_out`. These are only written on success — if an error occurs, `goto cleanup` is taken before reaching those assignments.

---

### `serialize_tree` — encoding entries into binary format

```c
cgit_error_t serialize_tree(tree_entry_t *entries, size_t count, buffer_t *out) {
  cgit_error_t result = CGIT_OK;
  qsort(entries, count, sizeof(tree_entry_t), cmp_entry);

  out->data = malloc(CGIT_READ_BUFFER_SIZE);
  ...
  out->capacity = CGIT_READ_BUFFER_SIZE;
  out->size = 0;

  for (size_t i = 0; i < count; i++) {
    char tmp[CGIT_READ_BUFFER_SIZE] = {0};
    char entry_byte_hash[CGIT_HASH_RAW_LEN];

    hex_to_bytes_hash((const unsigned char *)entries[i].hash, entry_byte_hash);

    size_t len = snprintf(tmp, sizeof(tmp), "%u %s", entries[i].mode, entries[i].name);
    memcpy(tmp + len + 1, entry_byte_hash, CGIT_HASH_RAW_LEN);
    size_t total_len = len + 1 + CGIT_HASH_RAW_LEN;

    // grow out buffer if needed ...

    memcpy(out->data + out->size, tmp, total_len);
    out->size += total_len;
  }

  return result;

cleanup:
  buffer_free(out);
  return result;
}
```

The binary format for each entry is built in a local stack buffer `tmp`:

1. `snprintf` writes `"<mode> <name>"` into `tmp`. `len` is the number of bytes written, **not** including a null terminator — but `snprintf` does write one, so `tmp[len] == '\0'`. That null byte is the entry separator required by the format.
2. `memcpy(tmp + len + 1, ...)` places the 20 raw bytes starting **after** the null byte at `tmp[len]`. The `+1` skips over it intentionally — it stays in place as the separator.
3. `total_len = len + 1 + CGIT_HASH_RAW_LEN` accounts for: the text, the null byte, and the 20 hash bytes.

The resulting layout in `tmp` is exactly: `<mode> <name>\0<20_byte_sha>`, which is what git expects. The whole block is then copied into the growing `out` buffer.

**Sorting happens first**: `qsort` runs before the loop. Git requires entries sorted by name; doing it upfront on the struct array is simpler than sorting a raw binary buffer.

**`hex_to_bytes_hash`**: the stored hashes in `tree_entry_t` are 40-char hex strings (produced by `compute_sha1`). The tree binary format requires 20 raw bytes. `hex_to_bytes_hash` converts them: every two hex chars become one byte via `sscanf("%02x", ...)`.

**Error path frees `out`**: if anything fails mid-loop, `goto cleanup` calls `buffer_free(out)` to release the partially-built buffer and zero the struct, leaving the caller with a clean `{NULL, 0, 0}` buffer rather than a dangling pointer.
