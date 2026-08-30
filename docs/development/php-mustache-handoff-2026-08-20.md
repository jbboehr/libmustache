# php-mustache migration handoff

**Date:** 2026-08-20

**libmustache baseline:** `3ce34e078d3091a91cc765edb968c49ef9421367`

**php-mustache baseline reviewed:**
[`604f56973aebfbbda5a7de74518aec36df027dc4`](https://github.com/jbboehr/php-mustache/tree/604f56973aebfbbda5a7de74518aec36df027dc4)
on `master`

**Purpose:** hand the completed libmustache C++ modernization to the
php-mustache migration without reopening settled library decisions or hiding
downstream compatibility choices inside mechanical porting.

This is the operational starting point. The authoritative library design and
API details remain in the
[modernization strategy](modernization-strategy-2026-08-11.md) and
[ABI 6 source-migration guide](abi-6-source-migration-2026-08-20.md).

## State at handoff

libmustache now provides:

- package version 0.6.0 and unreleased shared-library ABI 6;
- a C++17 baseline and explicit shared/static export boundary;
- owned, typed, copyable `Data` values;
- move-only RAII `Node` trees and checked legacy AST decoding;
- immutable, copyable `CompiledTemplate` and `PartialMap` handles;
- scoped `LambdaRenderContext` callback capabilities;
- explicit tokenization, parsing, serialization, reconstruction, and rendering
  limits;
- allocation-failure, sanitizer, fuzz, installed-consumer, and Mustache
  specification coverage; and
- a documented compatibility policy for retained and transitional APIs.

The core C++ modernization is at a downstream-testing boundary. ABI 6 is not
released or frozen: a narrowly justified API correction discovered by the PHP
migration can still be made before 0.6.0, but reproducing unsafe ABI 5
ownership is not an acceptable compatibility fix.

php-mustache `master` was last verified for this handoff at the commit above.
It currently has Linux, Docker, macOS, and Nix coverage, with Nix package
variants for PHP 8.1 through 8.4 and GCC/Clang. Reconfirm the supported PHP
versions at the start of the migration rather than treating this 2024 baseline
as a release commitment.

## Non-negotiable constraints

- Keep the externally visible PHP API recognizable while changing its C++
  implementation.
- Do not restore public raw ownership or mutable representation fields in
  libmustache merely to make the extension compile.
- Continue converting zvals eagerly into an owned C++ snapshot. A lazy holder,
  provider, or view layer is a later optimization only if end-to-end profiling
  shows conversion is material.
- Use Zend-provided lengths for every PHP string. Do not reconstruct a
  `std::string` from only `Z_STRVAL_P()`.
- Detect recursive PHP arrays and objects and enforce conversion budgets before
  unbounded recursion or allocation.
- Give every C++ object stored by a Zend object an explicit construction and
  destruction path. Zend object memory allocated with `ecalloc` does not run a
  C++ constructor for embedded `CompiledTemplate`, `LambdaRenderContext`,
  `unique_ptr`, or other non-trivial members.
- Never let a C++ exception cross a Zend or other C ABI boundary.
- Do not retain a `Renderer *`. Replace the PHP lambda helper with a scoped
  `LambdaRenderContext` capability that becomes safely inactive after the
  callback.
- Treat serialized AST strings as untrusted. Apply explicit decode limits and
  require complete canonical input.
- Follow the recorded [cache benchmark](ast-cache-benchmark-2026-08-22.md):
  persist source today and keep checked legacy reads only for the compatibility
  window below. The archived-template representation subsequently passed the
  hardened native performance, semantic, compatibility, fuzzing, and security
  gates with versioning, deep checking, integrity, and modern XXH3 enabled. Do
  not make archived bytes the default PHP cache value unless the remaining
  one-fetch/one-render PHP/APCu benchmark also passes.
- Keep Cista entirely inside libmustache. The extension should receive a
  libmustache-owned `ArchivedTemplate`, not include Cista headers or expose
  Cista types in PHP-facing implementation interfaces. Ordinary inputs should
  continue through the owned `Node`/`CompiledTemplate` path; cached archive
  bytes may use the checked archive path. Both paths must share rendering
  semantics inside libmustache.
- Construct archived templates through `loadArchivedTemplate()`. Its byte-vector
  and `std::string_view` overloads defensively copy Zend/APCu bytes into private
  aligned storage. The resulting immutable handle validates once and remains
  valid independently of the input buffer, so the extension must not create a
  separate borrowed Cista view over Zend memory.
- Catch `ArchivedTemplateException` at the persistent-cache boundary and branch
  on `reason()` rather than parsing `what()`. `InvalidArchive` and
  `UnsupportedFormat` identify entries that may be evicted and recompiled;
  `LimitExceeded` is a policy/configuration failure and must not become a
  repeated cache-miss loop. Retain a default switch branch for future reason
  values and a generic `mustache::Exception` catch for non-loading operations.
- Include `archivedTemplateCompatibilityTag()` verbatim in every persistent
  archive cache namespace. Do not derive a key from libmustache versions,
  architecture names, or private Cista details; the library-owned opaque tag
  already identifies the format generation and native representation domain.

## Verified downstream touchpoints

The following file inventory was verified against the php-mustache baseline,
not inferred only from the old libmustache API.

| php-mustache file | Current dependency on ABI 5 | Migration direction |
|---|---|---|
| `config.m4` | Probes C++11 and accepts any pkg-config `mustache` version. | Require C++17 and libmustache 0.6; preserve pkg-config flags rather than hardcoding a standard spelling. |
| `config.w32` | Looks for `mustache_static.lib` without the ABI 6 static-consumer contract. | Require the 0.6 headers/library and propagate `MUSTACHE_STATIC_DEFINE`; verify the supported MSVC/PHP SDK combinations. |
| `mustache_data.cpp` | Mutates `Data::type`, `val`, `length`, `data`, `children`, `array`, and `lambda`; allocates every child manually. | Replace with a transactional zval-to-owned-`Data` converter and const reverse conversion. |
| `mustache_ast.cpp` | Owns `Node *`, uses pointer-returning serialization, iterates raw child pointers, and exposes the legacy bytes through construction, string conversion, sleep, and wakeup. | Own the decoded tree through RAII, use `serializeValue()`/`unserializeOwned()`, and preserve checked legacy reads during the benchmark window. |
| `mustache_mustache.cpp` | Tokenizes into public nodes, builds `Node::Partials`, assigns a borrowed AST to `TypeContainer::child`, and often ignores Zend string lengths. | Migrate in stages: keep the safe compatibility renderer where AST input requires it, use length-aware calls immediately, and move ordinary source templates/partials to compiled handles. |
| `mustache_template.cpp` | Stores only PHP template source and reparses it during rendering. | Preserve the PHP class behavior first; decide request-local or object-local compiled-handle caching from lifecycle tests and the benchmark. |
| `mustache_lambda.cpp` | Implements the legacy `invoke(std::string *, Renderer *)` callback and installs the raw renderer in a PHP helper object. | Override the scoped callback overload and pass a correctly constructed `LambdaRenderContext` payload into the helper. |
| `mustache_lambda_helper.cpp` | Tokenizes helper input and calls `Renderer::renderForLambda()` through a retained raw pointer. | Render through the active context, use explicit PHP string lengths, and test helper use after callback invalidation. |
| `flake.nix` and `nix/derivation.nix` | Pin libmustache as a flake input and exercise PHP/compiler/coverage combinations. | Override the input with the local ABI 6 tree during development, then update the lock and keep a representative compatibility check in both projects. |

The public PHP surfaces that specifically constrain the migration are
`Mustache`, `MustacheData`, `MustacheTemplate`, `MustacheAST`, and
`MustacheLambdaHelper`. Existing PHPTs cover direct rendering, data round trips,
template and AST inputs, AST PHP serialization/APCu wakeup, lambdas, recursive
arrays and objects, partials, and generated Mustache specification cases.

## Known decision gates

### PHP scalar behavior

Do not blindly replace the old scalar strings with typed factories and update
expected output afterward. Characterize the existing PHP behavior first.

In the reviewed converter:

- `null` and `false` become empty strings;
- `true` becomes the string `"1"`;
- integers use `std::to_string()`;
- doubles use PHP's `EG(precision)` formatting; and
- PHP strings are currently copied without their explicit length.

The owned libmustache model renders `Data::boolean(true)` as `"true"`, and its
floating-point formatting is not defined by PHP's `EG(precision)`. The
migration must therefore explicitly choose and test one of these outcomes:

1. preserve the established PHP rendering spellings while separately
   retaining the original scalar type in the binding;
2. approve and document a PHP behavior change to libmustache's typed spelling;
   or
3. add a narrow pre-release library presentation mechanism justified by both
   bindings and ordinary C++ use.

Do not permanently collapse PHP values to strings merely because that is the
shortest port. Also decide whether `MustacheData::toValue()` and
`debugDataStructure()` preserve their old string-collapsing behavior or expose
the corrected PHP scalar types; this is a public PHP behavior decision.

### Arrays, objects, and recursion

The old converter rejects mixed numeric and associative PHP arrays. Preserve
that behavior for the first green port unless an explicit compatibility change
is approved. Build a temporary `Data` value and publish it only on complete
success.

Track active `HashTable *` and `zend_object *` identities while converting.
Reject cycles deterministically, including reference-mediated cycles. Apply
limits to nesting depth, expanded node count, aggregate string bytes, and
container entries. Every failure path must release temporary C++ values and
Zend references.

Preserve the characterized precedence between visible object properties,
methods, closures, and invokable objects. PHP callback wrappers must retain the
necessary zval through an RAII reference and release it exactly once.

### `MustacheAST` and partial ownership

`MustacheAST` is not an internal cache detail. Its constructor accepts binary
AST data; `__toString()`, `__sleep()`, `__wakeup()`, and APCu tests expose those
bytes to userland.

The existing partial adapter makes a `TypeContainer` whose `child` borrows the
`MustacheAST` node. In ABI 6, `Node::child` is owning. Never place that borrowed
pointer into a `unique_ptr` or clone it with a shallow copy.

Start with `MustacheAST` owning a `std::unique_ptr<Node>` and checked legacy
bytes. The safe representation for mixing AST-backed and source-backed
partials is a downstream discovery item. Plausible solutions include a narrow
libmustache factory that turns a validated owned node into an immutable
compiled handle, or a temporary compatibility-renderer path with explicit
ownership. Any new library API must preserve immutability and clear ownership;
serialization round-trips must not become an accidental cloning primitive.

Do not remove AST writes, introduce a replacement wire format, or promise an
indefinite legacy window before the benchmark.

### Zend storage for C++ values

Current payloads store raw `Data *`, `Node *`, and `Renderer *`, which are
trivial pointer fields inside `ecalloc`-allocated memory. Replacing a pointer
field directly with a non-trivial C++ value is undefined unless its constructor
is run.

Use one consistent pattern, such as a separately `new`-allocated C++ payload
owned by the Zend object or placement construction paired with an explicit
destructor in `free_obj`. Test failed object construction, partially initialized
objects, PHP cloning policy, request shutdown, and repeated destruction. Keep
the Zend-facing struct layout and C++ lifetime rules visibly separate.

## Recommended migration slices

Keep these as reviewable commits. Do not combine the data, AST, lambda, and
benchmark changes into one patch.

### Slice 0: Reproduce and record the ABI 6 baseline

1. Check out php-mustache at the reviewed baseline or record a newer starting
   SHA.
2. Point its flake input at the local libmustache tree without updating the
   permanent lock yet:

   ```sh
   nix build -L \
     --override-input libmustache path:/path/to/libmustache \
     '.#checks.x86_64-linux.php84-gcc'
   ```

3. Preserve the first compiler/linker diagnostics as the migration inventory.
4. Run the last known-good PHPT suite against ABI 5 so failures caused by the
   environment are separated from ABI 6 failures.
5. Select one representative Linux PHP build as the fast inner loop; expand
   the matrix only after that build is green.

This slice should not add unsafe compatibility shims to libmustache.

### Slice 1: Update the build contract and string lengths

- Require C++17 in Unix and Windows builds.
- Require libmustache 0.6 through pkg-config or an equivalent version check.
- Carry the package's selected compiler flags and Windows static definition.
- Pass `Z_STRVAL_P()` together with `Z_STRLEN_P()` everywhere, including
  template source, delimiters, scalar strings, lambda text, helper templates,
  and serialized AST bytes.
- Get the extension compiling far enough that remaining diagnostics correspond
  to representation changes rather than the toolchain contract.

### Slice 2: Replace zval conversion with owned `Data`

- Make the primary converter return `Data` by value or publish through an
  explicit RAII owner.
- Implement null, boolean, integer, floating-point, string, array, object, and
  lambda conversion transactionally.
- Add active-path recursion detection and conversion limits.
- Make reverse conversion accept `const Data&` and use typed accessors.
- Lock the scalar-policy decision with focused PHPTs before changing broad
  generated expectations.
- Keep rendering on the compatibility node path during this slice so data
  failures are isolated from AST migration.

### Slice 3: Migrate template and partial ownership

- Use `CompiledTemplate` for ordinary PHP string templates and source-backed
  partials.
- Preserve custom delimiter and default-escaping configuration through the
  member `Mustache::compile()` path.
- Move `MustacheAST` to `unserializeOwned()` and `serializeValue()` with
  request-appropriate limits.
- Resolve mixed source/AST partial ownership without borrowed owning pointers.
- Preserve `Mustache::parse()`, `MustacheAST::toArray()`, PHP serialization,
  wakeup, and APCu reads throughout php-mustache 0.x. Deprecate AST writes when
  compiled-handle and source-cache guidance ship.

### Slice 4: Migrate lambda capabilities

- Override `invoke(std::string_view, LambdaRenderContext)` in the PHP callback
  wrappers.
- Give `MustacheLambdaHelper` an explicitly constructed context payload.
- Render helper templates only through that context.
- Verify normal return, callback exceptions, nested callbacks, retained helper
  use after return, object destruction, and request shutdown.
- Translate every C++ exception before returning through Zend.

### Slice 5: Apply request-level resource policy

- Select explicit extension defaults for zval conversion, tokenization, AST
  decoding, rendering, output, lambda-generated templates, and partial depth.
- Keep zero meaning zero; do not reinterpret it as unlimited.
- Decide whether user configuration is necessary only after secure defaults
  work without configuration.
- Add PHPTs for each exhausted resource and verify a failed operation does not
  poison reusable PHP or C++ objects.

### Slice 6: Restore and expand the matrix

- Run the complete PHPT and generated specification suites.
- Run GCC and Clang builds, the supported PHP versions, valgrind, and practical
  sanitizer configurations.
- Run Linux and macOS before expanding Windows.
- Use the available Windows VM for the supported MSVC/PHP SDK combination and
  both static/shared assumptions that the extension actually ships.
- Add one php-mustache-against-libmustache-development job so future library
  API drift is reported before release.

### Slice 7: Benchmark serialization versus reparsing

Run this only after the functional paths are equivalent and green.

Compare:

1. fetching cached source and compiling it;
2. fetching a cached legacy AST, validating it, and decoding it;
3. reusing an already compiled handle within one request; and
4. the same cases with realistic nested partial graphs.

Use small, medium, and large production-shaped templates. Include PHP
serialization and APCu fetch/store costs, equivalent resource limits, optimized
builds, warm and cold cases, and enough samples to report median and tail
latency. Record throughput, allocations, peak memory, and cache payload size.
Define the threshold for a material win before examining the results.

If decoding does not win materially, cache source, deprecate AST writes, and
retain checked legacy reads for a documented migration window. If decoding
wins materially, design one canonical versioned format, write only that format,
read the old format during a bounded window, and version PHP cache keys.

**Result, updated 2026-08-25:** the
[completed legacy-format benchmark](ast-cache-benchmark-2026-08-22.md) selected
source for current cross-request caches. Warm and fresh-process legacy partial
graphs both favored source, and serialization-plus-APCu-store cost strengthened
that decision. A later checked Cista direct-view prototype removed the graph
ownership penalty and was 73% to 81% faster than source compile plus render on
the medium and large native workloads. Its payload was 4.35 to 4.80 times
source, and PHP serialization and APCu have not been measured. The initial
prototype timing used `WITH_STATIC_VERSION` alone. A later native mode matrix
found `DEEP_CHECK` effectively free, while Cista's FNV-1a-based
`WITH_INTEGRITY` made validation plus rendering 2.35 to 2.67 times as
expensive. A checksum follow-up measured zlib CRC-32 about 6.5 times and
XXH3-64 about 32 times as fast as FNV-1a; one XXH3 pass added 4.2% to 5.2% to
the deep-checked read-and-render path.
A final direct comparison supplied Cista with modern xxHash 0.8.3 and used
`WITH_VERSION | DEEP_CHECK | WITH_INTEGRITY`. Medium and large reads were
effectively tied with static versioning plus an external XXH3 pass; the native
writer was 2.6% to 5.5% slower. Cista 0.16 runtime versioning added a fixed 19
allocations and 781 bytes per read, a visible cost only for the smallest cases.
A matched native writer comparison found the selected Cista format 16% to 33%
slower than legacy compile plus serialization, or about 25 microseconds to 5.1
milliseconds per write. That is acceptable for a write-once, render-many cache.

The native libmustache slice is complete. Its optional archived-template
representation has the same rendering semantics as owned nodes, keeps Cista
private, requires versioning, deep checking, integrity, and libmustache
semantic validation, and uses pinned modern XXH3. The archive boundary has
sanitizer-backed fuzz, platform, installed-consumer, and export-boundary
coverage. Build systems now
default to automatic detection of the required private-symbol controls. The
production archive implementation has no zlib dependency; zlib remains only in
the explicit CRC-32 benchmark.

The next slice belongs in php-mustache: add a narrow experimental bridge and a
one-fetch/one-render APCu experiment. That experiment
must measure the actual Zend-string lifetime, alignment or aligned-copy cost,
PHP serialization, APCu copying, payload size, peak memory, and writer cost;
request-local reuse is not a substitute. Keep checked legacy reads through
libmustache 0.6.x and php-mustache 0.x; removal requires a separately announced
incompatible release.

## Test gates

The migration is not complete until the following are green:

- complete existing PHPT inventory with intentional expectation changes
  documented separately;
- embedded-NUL PHP strings through templates, data, delimiters, lambda text,
  and AST bytes where the target API permits them;
- scalar formatting and truthiness for every PHP scalar type;
- recursive and deeply nested arrays/objects, references, and alias-like object
  graphs;
- mixed array-key behavior and visible property/method precedence;
- malformed, truncated, trailing, oversized, and deeply nested AST input;
- AST construction, string conversion, `toArray()`, PHP serialization, wakeup,
  and APCu round trips;
- lambda closure, invokable object, object method, nesting, exception, retained
  helper, and post-callback invalidation paths;
- partial recursion, mixed source/AST partials, standalone indentation, and
  custom delimiters;
- deterministic allocation or exception-path coverage where practical;
- representative sanitizer or valgrind coverage; and
- at least one Linux CI job against the exact libmustache 0.6 development
  revision, followed by the supported release matrix.

## Definition of done

The handoff is complete when php-mustache builds against ABI 6 without direct
libmustache representation access or untracked owning raw pointers; all PHP
strings use explicit lengths; recursive conversion and all library boundaries
have enforced limits; callbacks cannot retain a live renderer capability;
exceptions and partial construction unwind safely; the supported PHPT matrix
passes; and the AST benchmark has produced a documented format, cache, and
compatibility-window decision.

Only then should libmustache attach deprecation attributes, remove a
downstream-gated compatibility adapter before 0.6.0, or declare ABI 6 stable.
