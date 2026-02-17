# How `ls-tree` Command Works in Git

## Tree Objects — The Concept

The idea is similar to the file system in Unix. A directory can be seen as a tree with multiple entries. In git, a tree object has multiple entries, each containing:

- **Mode**: like permissions in Unix (e.g. `040000`, `100644`, `100755`, `120000`)
- **Type**: not directly stored in the binary format, but derived from the mode
- **Name**: the file or directory name (stored alphabetically)
- **Hash (SHA-1)**: points to a blob or tree object
  - If the entry is a file, the hash points to a blob object
  - If the entry is a directory, the hash points to another tree object

### Valid Mode Values

For files:
- `100644` — regular file
- `100755` — executable file
- `120000` — symbolic link

For directories:
- `40000` — directory (printed as `040000` in ls-tree output)

### Example Directory Structure

For a repo like this:

```
your_repo/
  - file1
  - dir1/
    - file_in_dir_1
    - file_in_dir_2
  - dir2/
    - file_in_dir_3
```

The entries in the tree object would be:

```
40000 dir1 <tree_sha_1>
40000 dir2 <tree_sha_2>
100644 file1 <blob_sha_1>
```

- `dir1` is a directory pointing to tree object `<tree_sha_1>`
- `dir2` is a directory pointing to tree object `<tree_sha_2>`
- `file1` is a regular file pointing to blob object `<blob_sha_1>`

`dir1` and `dir2` are tree objects themselves, and their entries contain the files/directories inside them. This creates a recursive structure.

## How `ls-tree` Works Under the Hood

### Similarities with `cat-file`

The structure of the command is similar to `cat-file`. Each tree object is stored like every other git object, using the format `.cgit/objects/aa/bbbbbb...`. The object is compressed with zlib, and after decompression it has a header followed by content.

The key difference is in what the content contains. In a blob object, the format is:

```
blob <size>\0<raw_content>
```

In a tree object, the format is:

```
tree <size>\0<mode> <name>\0<20_byte_sha><mode> <name>\0<20_byte_sha>...
```

There are no newlines between entries in the actual binary — entries are packed one after another. Each entry is `<mode> <name>\0<20_byte_sha>` where:
- `<mode>` is the ASCII mode string
- `<name>` is the ASCII file/directory name
- `\0` is a null byte separator
- `<20_byte_sha>` is the raw 20-byte SHA-1 hash (NOT hexadecimal — just raw binary bytes)

### The `ls-tree` Output

```bash
$ git ls-tree <tree_sha>
040000 tree <tree_sha_1>    dir1
040000 tree <tree_sha_2>    dir2
100644 blob <blob_sha_1>    file1
```

The output is alphabetically sorted — this is how git stores entries internally.

With `--name-only`:

```bash
$ git ls-tree --name-only <tree_sha>
dir1
dir2
file1
```

## Important Considerations

- **Sorting**: entries are already stored sorted alphabetically in the tree object. No need to sort when reading, only when writing.
- **Type derivation**: the type ("blob", "tree") is not stored in the binary format. It must be derived from the mode using a mapping (e.g. `40000` → "tree", `100644` → "blob").
- **Hash conversion**: the 20-byte raw SHA-1 must be converted to 40-character hex representation for display. This uses the same byte-to-hex conversion logic as `compute_sha1`.
- **Type validation**: if the given hash points to a blob object instead of a tree, early return with an error after reading the object header.

## Code Architecture

### Flow of the Command

1. User types `cgit ls-tree [--name-only] <tree_sha>`
2. Handler parses arguments (detect `--name-only` flag)
3. Call `read_object` (same core function used by cat-file)
   - Validate hash
   - Build object path
   - Read file from disk
   - Decompress data
   - Parse header into type, size, content
4. Verify object type is "tree", error if not
5. Call `parse_tree` to parse the content into an array of entries
6. Display results (full entries or names only, depending on flag)
7. Cleanup: `free_tree_entries`, `free_object`

### Why This Design

The initial process (steps 1-3) reuses `read_object` from `core/object.c` — the same function cat-file uses. This is the layered architecture at work: both commands share the same core object reading logic.

The tree-specific parsing (`parse_tree`) belongs in `core/object.c` rather than in the handler because multiple future commands (checkout, diff, merge) will need to parse tree entries. It's a core concern, not a command concern.

The handler's only job is orchestration: validate args, call core functions in the right order, display results, clean up.

### Data Structures

```c
// A single entry in a tree object
typedef struct {
    unsigned int mode;         // numeric mode (octal: 40000, 100644, etc.)
    char *type;                // derived from mode: "tree", "blob"
    char *name;                // file or directory name
    char hash[CGIT_HASH_HEX_LEN + 1];  // 40-char hex hash + null terminator
} tree_entry_t;
```

### Function Signatures

```c
// Parse tree content into array of entries
// Allocates entries internally, caller frees with free_tree_entries
cgit_error_t parse_tree(const unsigned char *data, size_t len,
                        tree_entry_t **entries_out, size_t *count_out);

// Matching free function
void free_tree_entries(tree_entry_t *entries, size_t count);
```

### Ownership Model

- `parse_tree` allocates the entries array internally (only it knows the count)
- On success: ownership transfers to the caller
- On failure: `parse_tree` frees its own partial allocations, caller receives nothing
- The handler calls `free_tree_entries` in its cleanup section
- `hash` is a fixed-size array inside the struct — no separate allocation needed
- `type` and `name` are pointers that `parse_tree` allocates per entry
