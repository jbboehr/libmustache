# Cista archive format generation 3

Generation 3 preserves original section callback text. It keeps the framing,
integrity, native compatibility checks, and little-endian policy described in
the [generation-2 specification](cista-archive-format-v2.md), with the schema
changes below. Vendored Cista is unchanged.

The format remains experimental. Readers reject generations 1 and 2 with
`ArchivedTemplateError::UnsupportedFormat` at the preamble boundary. Recompile
cached template source to produce generation-3 archives; the reader does not
attempt to recover source text missing from older archives.

## Version and cache identity

| Field | Generation 3 value |
|---|---|
| Preamble magic | `MUSTARC\0` |
| Format generation at byte 8 | `3`, unsigned 64-bit little-endian |
| Semantic graph schema | `2` |
| Compatibility tag prefix | `libmustache-cista-v3-` |

The fingerprint includes the new node layout and its source-field offset.
The runtime type-version witness uses the same updated schema as the library.
Callers must continue using the complete opaque result of
`archivedTemplateCompatibilityTag()` in cache keys.

## Original section text

Each node adds an `ArchiveSlice originalSectionText` after its closing
delimiter slice. Presence bit `8` means that original text is available.
The slice uses the existing string table, so no additional offset vector or
pointer-bearing structure is introduced.

The writer copies each distinct retained source buffer into the string table
once. Nested sections refer to overlapping ranges within that copy. Ordinary
node data, delimiters, and partial names retain their existing storage. Buffer
addresses are used only to recognize sharing during writing; they are never
serialized and do not determine output order.

The complete retained input buffer is stored, including when only a detached
section from that input is archived. `maxTotalStringBytes` counts its bytes once,
along with ordinary node strings and partial names. `maxArchiveBytes` continues
to bound the complete framed result. Both limits apply to writing and loading.

| Source state when writing | Archived callback behavior |
|---|---|
| Parsed section with retained source | Receives the exact original body, including whitespace, comments, delimiter directives, and NUL bytes |
| Present, empty original body | Receives an empty body, even if its AST was edited |
| Constructed node, legacy-decoded node, or `discardSource()` applied | Reconstructs text from the archived nodes |

Public AST edits do not update retained source. Calling `discardSource()`
before writing explicitly selects reconstruction from the edited tree. The
archive preserves whichever behavior the owned tree had at serialization.

The loaded handle owns the archived source. Rendering borrows the validated
range and copies callback input through the existing lambda-byte accounting,
which charges both callback input and returned template text.

## Validation and native layout

The existing string-range checks apply to original text: the offset must be
within the table, and the length must fit in the remaining bytes. A present
empty range may end exactly at the end of the table. Absent source requires
both offset and length to be zero. Only section nodes may carry the presence
bit. Overlapping valid ranges are allowed; validation does not require source
text to match an edited AST.

The native node record grows from 40 to 48 bytes on the pinned fixture ABI:

| Offset | Field |
|---:|---|
| 0, 8, 16 | Data, opening delimiter, closing delimiter slices |
| 24 | Original section text slice |
| 32, 36 | First child and next sibling indices |
| 40, 42 | Node type and flags |
| 44 | Presence bits |
| 45–47 | Zero reserved bytes |

`tests/fixtures/cista-archive-v3-x86_64-le-itanium.hex` pins the complete
generation-3 bytes. The generation-2 fixture remains as a rejection test.
Other platforms retain their own native compatibility domain; transferring
compiled archives across architectures is not supported.
