
# Decompression and `git cat-file` — solid mental model

## Why decompression exists (goal)
The goal of decompression is to reconstruct the **original data representation** from a compressed form, so the information contained in the data becomes usable and readable again.

In Git, objects are stored compressed on disk to save space and improve performance. To inspect an object (as `git cat-file` does), Git must:

1. load the compressed bytes from disk,
2. decompress them,
3. interpret the decompressed bytes according to Git’s object format.

So decompression is not an optional step: it is the bridge between **raw storage** and **meaningful data**.

## Why decompression is iterative (core idea)
Decompression is an **iterative process** because the size of the decompressed data is **not known upfront**.

Even if you have all compressed bytes in memory, you cannot:
* predict the exact decompressed size,
* allocate the final buffer safely in advance,
* decompress everything in a single guaranteed step.

So the approach is:
* create a **decompression machine** (stateful),
* feed it compressed input,
* let it produce decompressed output in chunks,
* accumulate those chunks until decompression is complete.

This is not a limitation of Git; it is a fundamental property of streaming compression algorithms like DEFLATE.

## The decompression “machine” (mental model)

Think of decompression as a **state machine** with memory.
* It has **internal state** (what has already been decoded, what remains).
* That state must persist across iterations.
* Each iteration:
  * consumes some compressed input,
  * produces some decompressed output,
  * updates the internal state.

The machine stops **only** when it explicitly reports that the stream has ended.

## Buffers: temporary vs accumulator (critical distinction)

### Temporary output buffer

* Purpose: hold **only the bytes produced in one iteration**.
* Fixed size.
* Reused every iteration.
* Old data does not matter after it’s copied out.

Example:

```c
unsigned char tmp[32768];
```

This buffer is **scratch space** for the decompressor.

### Accumulator buffer

* Purpose: store **all decompressed bytes so far**.
* Grows dynamically.
* Owns the final decompressed data.

Key invariant:

> After each iteration, the accumulator contains exactly all bytes produced so far, stored contiguously from index `0` to `acc_len - 1`.

Example pattern:

```c
unsigned char *acc_buff = malloc(initial_cap);
size_t acc_len = 0;
size_t acc_cap = initial_cap;
```

Each iteration:

```c
memcpy(acc_buff + acc_len, tmp, produced);
acc_len += produced;
```

## Why “no more input” does NOT mean “finished”

### Key idea

**Decoding and emitting output are not the same thing.**

A decompressor can:

* consume all compressed input,
* still have decoded data internally that hasn’t been emitted yet,
* still need to finalize the stream (checksums, end markers).

So:

* “no more input bytes” is a **mechanical condition**,
* “stream finished” is a **semantic condition**.

Only the decompressor itself can tell you when the stream is truly complete.

## zlib inflate: how the iteration works

### Setup (once)

* Initialize the decompression state.
* Point the input window to the compressed data.

Conceptually:

```c
strm.next_in = compressed_buffer;
strm.avail_in = compressed_size;
```

### Loop (core logic)

Each iteration:
1. Provide fresh output space:

   ```c
   strm.next_out = tmp;
   strm.avail_out = sizeof(tmp);
   ```
2. Run one decompression step.
3. Measure how many bytes were produced:

   ```c
   produced = tmp_size - strm.avail_out;
   ```
4. Append those bytes to the accumulator.
5. Stop only if the decompressor reports **end of stream**.

### Return codes (important distinction)

* **`Z_OK`**

  * progress was made,
  * stream is **not finished**,
  * continue looping.

* **`Z_STREAM_END`**

  * stream fully decoded,
  * output is complete,
  * stop.

* Anything else

  * error or misuse,
  * abort immediately.

**Buffer fullness (`avail_out == 0`) is NOT the same as stream completion.**

## What decompression produces in Git

After decompression, Git object bytes have this exact format:

```
<type><space><size><NUL><payload>
```

Example:

```
blob 12\0Hello World\n
```

Meaning:

* `type`: object kind (`blob`, `tree`, `commit`)
* `size`: payload size in bytes (decimal ASCII)
* `NUL`: separator
* `payload`: actual object data

This format is what `git cat-file` interprets.

## How `git cat-file` uses decompression

Conceptually, `git cat-file` does:

1. Take a SHA-1 hash.
2. Locate `.git/objects/aa/bbbbb...`.
3. Read the compressed file.
4. Decompress it (iterative process).
5. Parse the decompressed header.
6. Based on the option:

   * `-t`: print `type`
   * `-s`: print `size`
   * `-p`: print `payload`

Decompression is **always required**, regardless of which option is used.
