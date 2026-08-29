# Experimental Cista archive format generation 1

The archived-template experiment prefixes its private Cista payload with a
minimal libmustache-owned preamble. The preamble identifies the application
format before Cista is entered; Cista remains responsible for native type
compatibility, integrity, and structural validation. No `cista::*` type appears
in the installed API.

This format remains experimental and optional. CMake and Autotools enable it
automatically when zlib and the required private-symbol controls are available,
while explicit require and disable modes remain available. Generation 1 is
pinned so dependency changes and future API work cannot silently change cache
bytes.

## Preamble layout

The preamble is exactly 16 bytes. Its integer is unsigned and encoded
little-endian. The Cista payload starts at offset 16, preserving its native
alignment when the complete input buffer is suitably aligned.

| Offset | Size | Field | Generation 1 value |
| ---: | ---: | --- | --- |
| 0 | 8 | Magic | `MUSTARC\0` |
| 8 | 8 | libmustache format generation | `1` |

The preamble deliberately does not duplicate Cista's type hash, checksum, or
reader policy. The supported cache format uses Cista `WITH_VERSION |
WITH_INTEGRITY | DEEP_CHECK`: `WITH_VERSION` rejects an incompatible native
type graph before pointer traversal, `WITH_INTEGRITY` detects payload changes,
and `DEEP_CHECK` performs bounds and structural validation while reading.
`DEEP_CHECK` adds no serialized bytes and is a reader policy rather than an
archive property.

The alternate security modes exposed by the benchmark harness exist to measure
those costs independently. They are not separately supported cache formats and
the caller must select the matching Cista representation when using them.

## Protected graph metadata

After Cista validation, libmustache validates the `ArchiveGraph` stored inside
the Cista payload. It contains:

- a graph magic value;
- the libmustache semantic schema version;
- the exact serialized Cista payload size; and
- the root, node, partial, and string tables.

Those fields are covered by Cista integrity in the supported cache format. The
semantic schema is independent of the outer format generation: change it when
the meaning or validation of graph fields changes, and change the format
generation when the preamble or underlying Cista encoding becomes incompatible.

## Validation order and limits

Readers apply these gates in order:

1. Enforce `maxInputBytes` against the complete preamble plus payload.
2. Require the complete fixed preamble, magic, and supported format generation.
3. Require a nonempty, correctly aligned Cista payload.
4. Bounds-check the serialized root-vector headers and spans without following
   their offset pointers.
5. Apply Cista type-version and integrity validation.
6. Apply Cista deep pointer and structural validation.
7. Require the protected graph magic, semantic schema, and exact payload size.
8. Apply libmustache graph-shape, resource, and rendering limits.

The writer enforces the same total-byte limit, so its default output is always
accepted by the default reader limits. XXH3 detects accidental payload
corruption; it does not authenticate the preamble or defend against an attacker
who can replace the complete cache entry.

Libmustache supplies XXH3 to Cista through one hidden, out-of-line adapter.
This preserves Cista's selected `WITH_VERSION | WITH_INTEGRITY` bytes while
preventing optimized compilers from separately inlining its address-of-local
type-hash helper in writer and reader paths. The archive golden fixture ensures
that this implementation boundary does not change the format.

## Compatibility and golden fixture

`tests/fixtures/cista-archive-v1-x86_64-le-itanium.hex` pins the complete bytes
for the canonical archive test graph on 64-bit little-endian x86 using the
Itanium C++ ABI. Every tested bundled/system Cista and xxHash combination must
produce that fixture exactly. Other supported platforms run the preamble,
Cista, graph, and rejection tests but do not compare against this
platform-specific fixture.

Changing graph semantics requires a semantic-schema change. Changing the
preamble or the Cista encoding contract requires a format-generation change.
Updating Cista, xxHash, or the supported security policy requires explicit
review and a deliberate golden-fixture decision; incompatible bytes must never
silently reuse an existing cache key.

## Fuzz coverage

`fuzz_cista_archive` exercises the complete default reader and renderer with
arbitrary framed bytes. It also repairs the XXH3 integrity field after mutation
so coverage can proceed past checksum rejection into Cista deep checking,
libmustache semantic validation, and archive-backed rendering. Validation is
isolated from rendering so only expected archive rejection is caught; any
failure after validation remains a fuzz finding. A separate template path
tokenizes the same input, renders both the owned tree and its protected archive,
and requires matching expected Mustache rejections or identical output;
unrelated post-tokenization exceptions remain fuzz findings.

The Clang sanitizer job and `libmustache-fuzz` Nix check run a bounded corpus
smoke test with a fixed seed and a freshly recreated writable output corpus. Its
immutable input corpus contains the golden archive, a truncated preamble, a
checksum-corruption/repair protocol derived from the golden archive, and a
template covering escaped and unescaped values, sections, dotted lookup,
partials, arrays, and a lambda. Crash artifacts persist across invocations.
Minimized findings must be retained as permanent regressions. A longer
sanitizer-backed acceptance run remains a gate before the format crosses the PHP
cache boundary.
