# Experimental Cista archive format generation 2

The archived-template experiment prefixes its private Cista payload with a
minimal libmustache-owned preamble. The preamble identifies the application
format before Cista is entered; libmustache owns the native type-version gate,
while Cista remains responsible for integrity and first-phase structural
validation. No `cista::*` type appears in the installed API.

This format remains experimental and optional. CMake and Autotools enable it
automatically when the required private-symbol controls are available, while
explicit require and disable modes remain available. Production archives use
XXH3 through the bundled or selected xxHash implementation; zlib is only an
optional dependency of the separate checksum benchmark. Generation 2 is pinned
so dependency changes and future API work cannot silently change cache bytes.

## Preamble layout

The preamble is exactly 24 bytes. Its integers are unsigned and encoded
little-endian. The Cista payload starts at offset 24, preserving its native
alignment when the complete input buffer is suitably aligned.

| Offset | Size | Field | Generation 2 value |
| ---: | ---: | --- | --- |
| 0 | 8 | Magic | `MUSTARC\0` |
| 8 | 8 | libmustache format generation | `2` |
| 16 | 8 | Native compatibility fingerprint | Platform-dependent |

`archivedTemplateCompatibilityTag()` exposes this cache domain as the opaque
string `libmustache-cista-v2-` followed by the fingerprint as 16 lowercase
hexadecimal digits. The preamble stores the same integer little-endian. Callers
must use the complete returned string in persistent cache keys and must not
parse or synthesize it.

The fingerprint is a stable FNV-1a digest of the format generation, semantic
schema and graph magic, serialized Cista version/integrity representation,
integrity algorithm, native byte order, the sizes, alignments, and member
offsets that determine the archived graph and Cista vector layouts, and the
exact Cista type-version value stored at payload offset 0. It changes
automatically when any of those native representation properties change.

For `WITH_VERSION`, libmustache calculates Cista's canonical type walk with a
fixed-capacity stack value. This keeps `archivedTemplateCompatibilityTag()`
allocation-free and nonthrowing; Cista's own runtime helper allocates a
`std::map` from functions declared `noexcept`, so an allocation failure would
otherwise terminate the process. Libmustache explicitly writes and validates
the fixed-storage value. It invokes Cista with `SKIP_VERSION` only after
pre-seeding the reserved version slot, preserving the exact `WITH_VERSION`
payload layout and integrity offsets without invoking the allocating runtime
walk. The benchmark's `WITH_STATIC_VERSION` mode similarly writes Cista's
constexpr static type version directly.

Every native CMake or Autotools archive configuration, and every CMake
benchmark configuration that selects runtime versioning, compiles and runs a
private dependency witness that compares the fixed-storage value with Cista's
own runtime type walk for the exact archive schema. That allocating helper is
used only by this build-time executable, never by the library. A system-Cista
algorithm change not mirrored by the fixed-storage adapter therefore fails the
configuration before a library can publish a tag or produce cache bytes.
Cross-compiling either runtime-version path requires an emulator or an explicit
successful witness from a matching native build
(`MUSTACHE_CISTA_VERSION_ALGORITHM_VERIFIED=ON` for CMake, or cached
`mustache_cv_cista_version_algorithm=yes` for Autotools). This applies to both
system and vendored Cista so a changed dependency can never bypass the
algorithm gate merely because the target is cross-compiled. The benchmark-only
`WITH_STATIC_VERSION` mode has no runtime algorithm and does not require this
witness.

Cista's `DEEP_CHECK` pointer tracker also calls its allocating runtime type walk.
The archived graph has only three offset vectors, so libmustache exhaustively
validates every serialized vector header, pointer, alignment, and byte range
before Cista's non-allocating first-phase structural traversal. The protected
graph validator then checks every node, partial, and string reference. This
fixed-schema path provides the supported deep-checking policy without entering
Cista's allocation-unsafe phase-II tracker.

The preamble deliberately does not store Cista's complete type hash verbatim,
or duplicate its checksum or reader policy. The supported cache contract keeps
the `WITH_VERSION | WITH_INTEGRITY | DEEP_CHECK` representation and policy:
the explicit `WITH_VERSION` value rejects an incompatible native type graph
before pointer traversal, `WITH_INTEGRITY` detects payload changes, and the
fixed-schema deep checks perform bounds and structural validation while
reading. Deep checking adds no serialized bytes and is a reader policy rather
than an archive property.

The alternate security modes exposed by the benchmark harness exist to measure
those costs independently. They are not separately supported cache formats and
the caller must select the matching Cista representation when using them. In
particular, the benchmark-only deep-check modes execute Cista's native
phase-II `DEEP_CHECK` tracker so they remain distinct from the neither and
integrity-only measurements; the production reader uses the fixed-schema
alternative described above.

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

1. Enforce `maxArchiveBytes` against the complete preamble plus payload.
2. Require the complete fixed preamble, magic, supported format generation, and
   matching native compatibility fingerprint.
3. Require the payload's Cista type-version word to match the fixed-storage
   value incorporated into the compatibility fingerprint.
4. Require a nonempty, correctly aligned Cista payload.
5. Bounds-check the serialized root-vector headers and spans without following
   their offset pointers.
6. Apply Cista integrity and first-phase structural validation without its
   allocating runtime type walk.
7. Apply libmustache's exhaustive fixed-schema pointer, graph-shape, resource,
   and rendering validation.
8. Require the protected graph magic, semantic schema, and exact payload size.

The writer enforces the same total-byte limit, so its default output is always
accepted by the default reader limits. XXH3 detects accidental payload
corruption; it does not authenticate the preamble or defend against an attacker
who can replace the complete cache entry.

Libmustache supplies XXH3 to Cista through one hidden, out-of-line adapter.
Together with the explicitly written version word, this preserves Cista's
selected `WITH_VERSION | WITH_INTEGRITY` bytes without entering its allocating
runtime type-hash helper. The archive golden fixture ensures that this
implementation boundary does not change the format.

## Compatibility and golden fixture

`tests/fixtures/cista-archive-v2-x86_64-le-itanium.hex` pins the complete bytes
for the canonical archive test graph on 64-bit little-endian x86 using the
Itanium C++ ABI. Every tested bundled/system Cista and xxHash combination must
produce that fixture exactly. Other supported platforms run the preamble,
Cista, graph, and rejection tests but do not compare against this
platform-specific fixture.

Changing graph semantics requires a semantic-schema change. Changing the
preamble or the Cista encoding contract requires a format-generation change.
Generation-1 archives are rejected explicitly at the preamble boundary; there
is no compatibility read path for the unreleased experimental representation.
Updating Cista, xxHash, or the supported security policy requires explicit
review and a deliberate golden-fixture decision; incompatible bytes must never
silently reuse an existing cache key.

The CMake `cista_version_mutation` regression produces archives and cache tags
with unmodified and controlled system-Cista builds for the current native
platform and inherits the parent compiler, toolchain, ABI flags, sysroot, and
configuration flags. A coherent mutation changes Cista's runtime base hash
without changing the archive graph layout; each build must publish a distinct
tag and reject the other build's archive at the compatibility preamble. A
second mutation changes only Cista's recursive vector hashing rule and must be
rejected by the build-time dependency witness. These checks cover dependency
hash drift without relying on the x86-64 golden fixture. The archive allocation
regression separately injects every writer and reader allocation failure in
fresh subprocesses, requiring `std::bad_alloc` propagation instead of process
termination.

## Fuzz coverage

`fuzz_cista_archive` exercises the complete default reader and renderer with
arbitrary framed bytes. It also repairs the XXH3 integrity field after mutation
so coverage can proceed past checksum rejection into fixed-schema deep
checking, libmustache semantic validation, and archive-backed rendering. Validation is
isolated from rendering so only expected archive rejection is caught; any
failure after validation remains a fuzz finding. A separate template path
tokenizes the same input, renders both the owned tree and its protected
archive, and requires matching expected Mustache rejections or identical
output; unrelated post-tokenization exceptions remain fuzz findings.

The Clang sanitizer job and `libmustache-fuzz` Nix check run a bounded corpus
smoke test with a fixed seed and a freshly recreated writable output corpus. Its
immutable input corpus contains the golden archive, a truncated preamble, a
checksum-corruption/repair protocol derived from the golden archive, and a
template covering escaped and unescaped values, sections, dotted lookup,
partials, arrays, and a lambda. Crash artifacts persist across invocations.
Minimized findings must be retained as permanent regressions. A longer
sanitizer-backed acceptance run remains a gate before the format crosses the PHP
cache boundary.
