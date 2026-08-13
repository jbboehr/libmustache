# Modernization and memory-safety strategy

**Date:** 2026-08-11

**Last revised:** 2026-08-13

**Status:** Proposed implementation strategy

This document describes the planned modernization of libmustache after the
build-system hardening work. It incorporates the findings from the
[repository review](repository-review-2026-08-11.md), the
[build-hardening follow-up](build-hardening-follow-up-2026-08-11.md), a review
of php-mustache as the library's primary downstream consumer, and subsequent
review of the original modernization plan.

## Direction

Modernize libmustache in C++ first, while treating php-mustache as the primary
downstream compatibility test. Urgent libmustache security work must not wait
for a complete PHP build matrix or for the PHP extension migration.

The goals are:

- rule-of-zero, value-owned C++ internals;
- no owning raw pointers;
- no unchecked binary parsing;
- length-aware input everywhere;
- a recognizable high-level API;
- an intentional ABI break instead of preserving unsafe structure layouts;
- eager, owned conversion of PHP data unless profiling later justifies a lazy
  adapter; and
- continuous compatibility testing against php-mustache as the new API
  settles.

A Rust rewrite is not currently recommended. It would replace the present C++
boundary with a C ABI and unsafe callback/lifetime integration at the Zend
boundary while discarding approximate C++ source compatibility. Rust should be
reconsidered only if the long-term product becomes a language-neutral engine
behind a stable C ABI rather than primarily a C++ library.

C++17 is the proposed initial language baseline. It provides the important
ownership and representation tools, including `std::variant` and
`std::string_view`, without unnecessarily raising downstream compiler
requirements. C++20 can be selected later if a packaging and consumer survey
shows that the higher baseline is acceptable.

Raising the baseline should also remove obsolete portability machinery such as
the `AC_CXX_STL_HASH` probe and `MUSTACHE_HASH_*` indirection. The installed
`MUSTACHE_HAVE_CXX11` macro and `mustachec -v` output need an explicit
deprecation or replacement rather than silent removal.

## Compatibility policy

Preserve the concepts and common usage of:

- `Mustache`, `Data`, `Node`, `Renderer`, and `Lambda`;
- compile, tokenize, and render operations;
- intentional rendering behavior; and
- safe reading of legacy serialized ASTs for a documented compatibility
  window.

Do not preserve:

- the ABI or public structure layout;
- public ownership fields;
- raw-pointer containers;
- `Data::length`;
- borrowed AST child pointers;
- unchecked deserialization;
- unsafe implicit copy operations; or
- direct mutation of internal type discriminators.

The result should be released as the new libmustache 0.6 ABI, even if
ordinary callers retain approximate source compatibility. Compatibility means
keeping the high-level library recognizable; it does not mean emulating unsafe
field-level ownership.

The modernization development line now reports package version 0.6.0 and ABI
6 so its shared library cannot be mistaken for the released ABI 5. ABI 6 is
not considered stable or released until this modernization is complete; the
project will make this single ABI transition rather than assigning a new ABI
number to every incompatible commit on the unreleased development line.

The current AST byte format is publicly exposed by php-mustache through the
`MustacheAST` constructor, string conversion, and PHP serialization hooks. It
therefore cannot be treated as an internal or APC-only cache format. The
existing decoder must be made safe now. Whether libmustache should introduce a
replacement format, however, is a separate decision that will be made from a
PHP benchmark of validated AST decoding versus source reparsing. The project
will not commit to maintaining two formats indefinitely.

## Execution principles

- Security fixes in independently testable libmustache code proceed without
  waiting for downstream CI infrastructure.
- A parser or decoder fuzzer is part of that component's acceptance criteria,
  not a final optional verification step.
- Characterization tests must compare exact output and accurately report
  omitted or unsupported cases.
- Observable behavior changes are separated from ownership and representation
  changes wherever possible.
- Resource limits are public policy. Their units, defaults, failure behavior,
  and configuration mechanism must be documented and tested.
- Every phase has an explicit definition of done.

## Phase 1: Establish an honest safety baseline

Before changing representations:

1. Fix Mustache specification skip accounting. Every omitted case must have a
   recorded reason, and the suite must fail if the expected inventory changes
   silently.
2. Compare specification output byte-for-byte. Keep known failures in an
   explicit deviation ledger rather than normalizing whitespace.
3. Capture common uses of the public and installed API in focused
   characterization tests. Expand `tests/cmake-consumer` beyond its current
   version-only check so it exercises the installed API as a downstream would.
4. Add golden tests for JSON, YAML, and direct `Data` scalar truthiness and
   output formatting, including null, false, true, integers, floating-point
   values, strings, and embedded NULs where supported.
5. Import exact legacy AST byte fixtures from php-mustache and add
   serialize/decode round-trip tests.
6. Inventory actual `Data` and `Node` construction and copy use in libmustache
   and php-mustache.

The full php-mustache CI integration is not a prerequisite for the decoder
work in Phase 2.

**Definition of done:** the test report accounts for every specification case;
known exact-output deviations and scalar behavior are pinned; legacy PHP AST
fixtures decode and round-trip; and an installed consumer exercises the common
public API.

## Phase 2: Harden the exposed binary boundary immediately

The binary AST decoder is callable with PHP-provided strings and must be
treated as an untrusted-input boundary. `_GLIBCXX_ASSERTIONS` may convert some
unchecked accesses into process aborts in hardened builds, but it is not a
security boundary and consumers may build without it.

The decoder must:

- parse through a bounds-checked byte cursor;
- perform a bounds check before every read;
- check arithmetic before allocation or cursor movement;
- limit input bytes, string sizes, node counts, child counts, and nesting
  depth;
- reject invalid type and flag values;
- validate declared child sizes and string terminators;
- reject truncated, trailing, and otherwise non-canonical input;
- avoid partially modifying an existing AST;
- construct into temporary RAII-managed state and publish only on success; and
- return precise, stable errors rather than aborting the process.

The current serializer must also reject values that the legacy field widths
cannot represent instead of silently truncating them, and it should return an
owned value instead of a heap-allocated container pointer.

Keep the existing `serialize()` and `unserialize()` entry points for source
compatibility, but make their resource policy explicit through overloads that
accept `Node::SerializationLimits`. The compatibility defaults are 64 MiB for
both serialized input and output, 64 nodes along any root-to-leaf path, 100,000
nodes in total, 256 dotted-name components per node, and 100,000 dotted-name
components in total. Each value is a hard maximum; zero never means unlimited.
The fixed 24-bit data-length and 16-bit child-count fields remain independent
wire-format limits. PHP should pass request-appropriate limits rather than
relying on the broader compatibility defaults.

Create a dedicated binary-decoder fuzz target as part of this phase. Commit a
seed corpus containing valid php-mustache fixtures plus malformed, truncated,
oversized, and deeply nested cases. Run a short sanitizer-backed fuzz smoke test
in CI and record an extended run before considering the hardening complete.
The initial extended acceptance run should be at least one CPU-hour under
AddressSanitizer and UndefinedBehaviorSanitizer with no findings. Every fuzzer
finding becomes a permanent regression test.

The reproducible smoke check is
`nix build .#checks.x86_64-linux.libmustache-fuzz`. For the one-hour acceptance
run, configure a Clang build with `MUSTACHE_ENABLE_TESTS`,
`MUSTACHE_ENABLE_SANITIZERS`, and `MUSTACHE_ENABLE_FUZZING` enabled, build it,
then run `fuzz_node_unserialize` with `-max_total_time=3600`, the committed
dictionary, and the corpus copy in the build tree. Keep any generated crash
artifact and add its minimized input as a regression test before fixing it.

Do not design a replacement serialization format in this phase. Preserve safe
legacy reads while the PHP benchmark in Phase 7 determines whether persistent
compiled ASTs have enough value to justify a new format.

**Definition of done:** the valid legacy corpus still decodes; the malformed
corpus fails cleanly; unit tests cover every limit and validation rule; the
fuzzer passes its CI smoke test and documented extended sanitizer run; and no
decoder failure aborts or leaks.

## Phase 3: Install immediate API safety barriers

Owning `Data` and `Node` objects are currently implicitly shallow-copyable.
Turn this runtime corruption risk into an explicit source-level contract before
the larger representation work:

- delete copy construction and copy assignment unless a correct deep copy is
  deliberately implemented;
- implement correct move construction and move assignment;
- add compile-time assertions for the selected copy and move traits;
- update in-repository partial construction to construct in place or move; and
- compile php-mustache early to expose any downstream reliance on unsafe
  copying.

The preferred short-term policy is move-only ownership. The final value-owned
AST may later regain safe value copying or use an immutable shared handle.

In the same compatibility tranche:

- raise CMake and Autotools to C++17;
- remove obsolete hash-container portability probes and macros;
- handle the installed C++11 feature macro explicitly; and
- add length-aware overloads for template source, delimiters, lambda text, and
  serialized bytes while retaining safe compatibility adapters where useful.

Begin with one representative php-mustache build and PHPT job. Expand the PHP
version and platform matrix later; it must not delay the decoder fix.

**Definition of done:** unsafe copies fail at compile time, moves pass sanitizer
tests, all in-repository call sites use the explicit ownership contract, both
build systems enforce C++17, installed consumers pass, and at least one
php-mustache compatibility job reports downstream breakage clearly.

## Phase 4: Replace the AST ownership model

The first ownership slice converts the compatibility `Node` tree itself to
RAII: node text and dotted-name components are values, children and the legacy
container child are uniquely owned, partial maps contain uniquely owned nodes,
and the unused fixed-size `NodeStack` has been removed. `Node` remains
move-only while the opaque immutable `CompiledTemplate` API below is still to
be introduced.

AST nodes should become rule-of-zero values or immutable owned state:

- strings stored by value;
- children stored as values or explicit immutable handles;
- no owning raw pointers;
- no borrowed `child` field;
- safe copying or explicit immutable sharing; and
- enforced parser depth and size budgets.

Introduce an opaque, immutable compiled-template handle alongside the
compatibility surface:

```cpp
CompiledTemplate compile(std::string_view source);

std::string render(
    const CompiledTemplate& compiled,
    const Data& data,
    const PartialMap& partials
);
```

Partials should refer to compiled-template handles rather than borrowed
`Node*` values. A compatibility `Node` API can temporarily act as a facade over
the safe representation. This lets libmustache change parser internals without
requiring php-mustache to depend on AST layout.

Add a template-parser fuzz target during this phase, with depth, input-size,
node-count, and delimiter invariants represented in its corpus. Existing exact
rendering tests should remain unchanged unless a behavior correction is
separately documented.

The parser fuzzer is `fuzz_tokenizer`. Its CI smoke run is part of
`nix build .#checks.x86_64-linux.libmustache-fuzz`; an extended acceptance run
should invoke it with `-max_total_time=3600`, the committed tokenizer
dictionary, and the copied tokenizer corpus in the build tree. Serialization
round-trip failures after successful parsing are fuzz findings, not expected
parser rejections; the fuzzer uses compatible explicit serialization limits so
those invariants remain observable.

**Definition of done:** the AST contains no owning or silently borrowed raw
pointers; partial lifetime is explicit; existing parser and rendering behavior
passes its characterization suite; all parser limits have tests; and the parser
fuzzer completes its CI smoke test and documented extended sanitizer run.

## Phase 5: Replace `Data` with an owned value model

The new representation should distinguish conceptually between:

```text
null | boolean | integer | floating point | string | array | object | lambda
```

`std::variant` is a likely implementation tool, but the recursive container
layout must be standards-valid on libstdc++, libc++, and MSVC. In particular,
the design must not directly instantiate `std::map<std::string, Data>` while
`Data` is incomplete. Validate the chosen wrapper or indirection model with all
supported standard libraries before making it public.

The representation should have these properties:

- strings are owned by value;
- arrays and objects own their child values;
- no child is owned through a raw pointer;
- container sizes come from the containers themselves;
- lambda ownership uses an explicit RAII handle;
- copy and move behavior is deliberately defined; and
- failed construction cannot leave a partially valid object.

Expose constructors, factories, builders, and visitors rather than mutable
public representation fields. For example:

```cpp
Data::string("Ada");
Data::array({...});
Data::object({...});
```

Safe compatibility overloads may be retained or deprecated, but the new API
must not reproduce raw-pointer ownership. The choice between unique, shared,
or cloneable lambda ownership should be made after characterizing existing
copy behavior; a bare owning `Lambda*` is not acceptable.

The current adapters collapse scalars to strings, and JSON, YAML, direct C++
construction, and PHP do not necessarily use identical text or truthiness
rules. The new model should preserve scalar type information internally while
initially reproducing the characterized rendering behavior. Any correction to
truthiness or scalar formatting must be a separately documented behavior
change with explicit compatibility tests.

### Replace the json-c adapter

The Nix build currently obtains json-c 0.18 from the pinned nixpkgs input, but
the CMake and Autotools checks do not declare a minimum json-c version. This
means compatibility tests must not accidentally treat behavior introduced by
a newer json-c release, such as a particular floating-point spelling, as a
libmustache guarantee.

It is acceptable to raise the temporary minimum json-c version when a concrete
security fix, API requirement, or supported-platform policy justifies it. Such
a change must be applied consistently to CMake, Autotools, Nix, vcpkg, CI, and
installed-consumer metadata. The minimum must not be raised solely to make a
characterization test pass; version-dependent formatting should instead be
tested semantically or recorded as dependency-specific behavior.

Replace json-c while introducing the owned `Data` representation rather than
as an isolated rewrite. This keeps parser behavior changes adjacent to the
typed scalar model and avoids translating from one unsafe intermediate
representation into another. Preserve `Data::createFromJSON` as a compatibility
facade, but make the parser an implementation detail that produces a complete
temporary value and publishes it only after successful validation and
conversion.

Boost.JSON is the leading replacement candidate because it offers RAII-owned
values, strict parsing, explicit nesting limits, error-code APIs, and support
for C++11 and later. Before selecting it, compare it with nlohmann/json and the
existing json-c adapter using a small implementation spike. The comparison
must cover:

- Linux, macOS, and MSVC packaging and installed-consumer behavior;
- shared and static linking, including whether parser dependencies leak into
  the public package interface;
- strict JSON and UTF-8 handling;
- integer, floating-point, negative-zero, and overflow behavior;
- duplicate object keys and exact trailing-input rejection;
- parse errors and allocation-failure cleanup;
- compile time, binary size, parse time, allocations, and peak memory; and
- maintenance cost and the process for receiving dependency security updates.

Do not select simdjson without a demonstrated workload need; its parser-owned
view lifetimes add complexity that is unnecessary for the CLI and do not help
the PHP extension's usual direct conversion path. Prefer a simple temporary
DOM followed by transactional conversion into owned `Data`. A direct SAX
builder may be considered only if profiling shows the temporary DOM to be a
material cost and the builder can preserve the same validation and exception-
safety guarantees.

Whichever parser is selected, libmustache must impose its own input-byte,
nesting-depth, node-count, aggregate-string-byte, and allocation budgets. The
adapter must reject trailing input, invalid encodings, unsupported numeric
values, and limit exhaustion with stable library errors. Parser defaults are
defense in depth, not the public resource-limit policy. Add a JSON fuzz target
and retain a differential corpus across json-c and the replacement until every
intentional compatibility difference has been reviewed and recorded.

**Definition of done:** JSON, YAML, and direct construction use the owned value
model; no owning raw pointer or separate length remains; the recursive layout
is tested with every supported standard library; the selected JSON parser has
explicit resource limits and fuzz coverage; allocation and parser failure
unwind cleanly under sanitizers; and the scalar golden tests either remain
unchanged or record an intentionally approved behavior change.

## Phase 6: Modernize rendering and lambdas

The renderer should:

- accept `std::string_view` for borrowed input;
- return owned output;
- accept immutable compiled templates;
- avoid persistent borrowed pointers;
- carry explicit recursion and output-size limits; and
- remain exception-safe when callbacks fail.

Introduce a public `RenderLimits` or equivalent policy object. Before the
phase is complete, choose numeric library defaults, define whether zero means
disabled or forbidden, document limit accounting, and specify the exception
raised on exhaustion. php-mustache may select stricter request-appropriate
defaults, but it must not depend on undocumented renderer behavior.

The php-mustache lambda helper requires special handling. PHP can retain the
helper beyond the callback window in which its renderer pointer is valid.
Replace that borrowed pointer with a callback-scoped capability that:

- is valid only during the lambda invocation;
- is explicitly invalidated afterward;
- throws a clean exception if retained and reused; and
- never exposes silently dangling renderer state.

No C++ exception may cross a Zend or other C ABI boundary.

**Definition of done:** renderer state contains no untracked borrowed lifetime;
limit defaults and overrides are documented and tested; limit exhaustion and
callback exceptions leave the renderer reusable; retained callback
capabilities fail safely; and sanitizer tests cover every error path.

## Phase 7: Migrate and benchmark php-mustache

php-mustache should continue converting PHP values into an owned C++ snapshot:

```text
PHP zvals
    |
    | PhpDataConverter
    v
owned mustache::Data
    |
    v
pure C++ rendering
```

The converter should:

- build transactionally into temporary values;
- preserve null, boolean, numeric, and string distinctions;
- use Zend-provided string lengths;
- detect recursive arrays and objects;
- enforce depth, element-count, and allocation limits;
- release all state correctly on exceptions; and
- retain PHP closures through a narrow RAII callback type.

This isolates the renderer from Zend lifetimes, reference counting, magic
property access, reentrancy, and mutation during callbacks. The renderer may be
designed around a small internal `ValueView` abstraction so that a lazy
`PhpValueProvider` remains possible, but such a provider should be implemented
only if measurements show that snapshot conversion is a material bottleneck.

The extension migration should:

1. Replace direct `Data` field mutation with the converter and builder API.
2. Store compiled-template handles instead of public AST internals.
3. Replace borrowed partial pointers with explicit immutable ownership.
4. Use explicit lengths for all PHP strings.
5. Update lambda lifetime management.
6. Translate library failures consistently into PHP exceptions.
7. Update the extension's C++ standard and libmustache version requirements.

### Serialization decision benchmark

Before designing a replacement AST format, benchmark these PHP workflows:

1. Fetch cached template source and compile/tokenize it.
2. Fetch a cached serialized AST, validate it, and decode it.
3. Reuse an already compiled template within one request.
4. Repeat the comparisons for realistic small, medium, and large templates and
   for nested partial graphs.

Measure median and tail latency, throughput, allocations, peak memory, and
cached payload size. Include PHP serialization and APCu fetch/store overhead
where relevant. Use optimized builds with equivalent validation and resource
limits, and define what counts as a material improvement before examining the
results.

The result controls the serialization roadmap:

- If validated AST decoding has no material real-world advantage, cache source
  and reparse it. Deprecate writing serialized ASTs and retain the hardened
  legacy decoder only for a documented compatibility window.
- If decoding wins materially, introduce a canonical, versioned format. Write
  only the new format, read the legacy format for a documented migration
  window, and give PHP caches a versioned key.

In neither case should the legacy decoder be maintained indefinitely without a
specific compatibility commitment.

**Definition of done:** the full supported PHPT suite passes against the new
library API; PHP conversion and callbacks pass sanitizer-backed tests where
practical; the benchmark method and results are committed or otherwise
reproducible; and the serialization decision, compatibility window, and cache
migration are documented.

## Phase 8: Continuous verification and release

Continuously exercise the modernized code with:

- AddressSanitizer and UndefinedBehaviorSanitizer;
- strict compiler warnings and targeted static analysis;
- parser and binary-decoder fuzzing;
- serialization compatibility and round-trip tests;
- allocation-failure and exception-path tests;
- deeply nested templates and data;
- excessive output-expansion tests;
- arbitrary byte strings and embedded NULs where supported;
- retained lambda-helper tests;
- recursive PHP array and object tests; and
- repeated PHP serialization and wakeup tests.

Run the C++ suite and the php-mustache PHPT suite under sanitizers where
practical. Sanitizer-clean tests are necessary but do not replace explicit
resource limits, parser invariants, and ownership design.

For the ABI-breaking release:

- update the CMake ABI declaration and Libtool `-version-info` together;
- keep `scripts/check-version-consistency.sh` enforcing their agreement;
- update CMake, Autotools, Nix, vcpkg, and consumer version metadata;
- publish a source-migration guide for removed public fields and ownership
  operations; and
- state the legacy AST compatibility window and selected serialization policy.

**Definition of done:** all build systems, installed-consumer tests, sanitizer
jobs, fuzz smoke tests, and the supported php-mustache matrix pass; version and
ABI consistency checks pass; no unresolved high-severity finding remains; and
the migration and compatibility policies are published with the release.

## Immediate implementation order

1. Fix specification accounting and commit the behavior/deviation ledger.
2. Add scalar golden tests and import the php-mustache AST byte fixtures.
3. Harden the existing decoder and serializer behind unit tests and a fuzzer.
4. Delete unsafe implicit `Data` and `Node` copying, add correct moves, and
   update existing call sites.
5. Raise the baseline to C++17, remove obsolete portability machinery, and add
   length-aware APIs.
6. Establish one representative php-mustache compatibility job.
7. Implement the safe AST and `CompiledTemplate` before changing scalar data
   semantics.
8. Implement the safe owned `Data`, replace json-c after the parser spike, and
   add renderer limits and safe callback lifetimes.
9. Migrate php-mustache, run the serialization benchmark, and follow its
   decision.
10. Remove transitional compatibility surfaces according to their documented
    schedules and release the new ABI.

This sequence addresses the currently exposed hazards first, makes fuzzing a
condition of accepting parser work, separates security fixes from downstream
CI plumbing, and uses measurements rather than assumptions to decide whether a
new persistent AST format is worth its security and maintenance cost.
