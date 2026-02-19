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
