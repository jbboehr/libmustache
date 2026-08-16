# Modernization and memory-safety strategy

**Date:** 2026-08-11

**Last revised:** 2026-08-15

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

The current serializer rejects values that the legacy field widths cannot
represent instead of silently truncating them. `Node::serializeValue()` now
returns the encoded bytes by value, while `Node::unserializeOwned()` returns a
`std::unique_ptr<Node>` only after a complete strict decode. The original
owning-pointer methods remain compatibility adapters over those canonical
paths.

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

The recorded extended decoder run used Clang 21.1.8 with ASan/UBSan,
`-max_total_time=3600`, `-max_len=4096`, `-timeout=5`, and the committed
dictionary and corpus. It completed 169,692,478 executions in 3,601 seconds,
with 536 MiB peak RSS and no crash, timeout, sanitizer finding, or failure
artifact.

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

**Status: implemented on the 0.6 development branch.**

The compatibility `Node` tree now uses RAII: node text and dotted-name
components are values, children and the legacy container child are uniquely
owned, partial maps contain uniquely owned nodes, and the unused fixed-size
`NodeStack` has been removed. `Node` remains move-only for source
compatibility. New consumers can instead use the opaque `CompiledTemplate`
handle, which immutably shares the owned AST without exposing its layout.

AST nodes should become rule-of-zero values or immutable owned state:

- strings stored by value;
- children stored as values or explicit immutable handles;
- no owning raw pointers;
- no borrowed `child` field;
- safe copying or explicit immutable sharing; and
- enforced parser depth and size budgets.

The opaque, immutable compiled-template handle is available alongside the
compatibility surface:

```cpp
CompiledTemplate compile(std::string_view source);

std::string render(
    const CompiledTemplate& compiled,
    Data& data,
    const PartialMap& partials
);
```

`PartialMap` owns compiled-template handles rather than borrowing `Node*`
values, and the renderer reads AST nodes through const pointers. A fresh
renderer instance holds the transient data and output state for each compiled
render. The compatibility `Node` API remains available during the downstream
migration window. This lets libmustache change parser internals without
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

The recorded extended tokenizer run used Clang 21.1.8 with ASan/UBSan,
`-max_total_time=3600`, `-max_len=4096`, `-timeout=5`, and the committed
dictionary and corpus. It completed 14,972,000 executions in 3,601 seconds,
with 553 MiB peak RSS and no crash, timeout, sanitizer finding, or failure
artifact.

**Definition of done:** the AST contains no owning or silently borrowed raw
pointers; partial lifetime is explicit; existing parser and rendering behavior
passes its characterization suite; all parser limits have tests; and the parser
fuzzer completes its CI smoke test and documented extended sanitizer run.

## Phase 5: Replace `Data` with an owned value model

**Status: owned representation, shared JSON/YAML resource policy, and the
nlohmann/json SAX replacement are implemented and verified on the 0.6
development branch.**

`Data` now uses private variant-backed storage. Strings and recursive
containers are values, no child is owned through a raw pointer, container
sizes come from their containers, typed scalar factories preserve JSON type
information, and copies deep-copy the value tree. Lambda factories accept an
RAII handle; copied data trees deliberately share the callback. Rendering is
const with respect to the data tree, while callbacks remain callable and may
retain their own state. The former public representation fields are not part
of ABI 6. As the intentional scalar-model correction anticipated by the
characterization suite, valid top-level JSON `null` now produces a null value
instead of being mistaken for a parse failure.

The representation distinguishes between:

```text
null | boolean | integer | floating point | string | array | object | lambda
```

The implementation uses an opaque storage object containing `std::variant`,
so recursive containers are instantiated only after `Data` is complete. This
layout must remain covered on libstdc++, libc++, and MSVC.

The representation has these properties:

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

`Data::ParseLimits` now applies the same public policy to length-aware JSON
and YAML parsing. The defaults are 64 MiB of input, 32 value
nodes along a root-to-leaf path, 100,000 expanded value nodes, 64 MiB of
aggregate strings and object keys, and 100,000 aggregate container entries.
Every field is a hard maximum and zero never means unlimited. The root counts
as the first value node. Preserved JSON floating-point spellings count as
string bytes. A separate implementation safety ceiling rejects paths beyond
256 value nodes even when a caller configures a higher maximum.

YAML aliases are expanded into independent owned values. Each expansion
shares the node, string-byte, and container-entry budgets, while active-path
tracking rejects recursive aliases before conversion can recurse indefinitely.
JSON is built directly into owned values by a bounded nlohmann/json SAX
adapter, so it retains no dependency DOM. YAML structural preflight enforces
the policy before libyaml constructs a complete document; conversion
independently rechecks the produced value tree and YAML alias expansion. Both
adapters explicitly reject raw embedded NULs. Escaped NULs are supported and
budgeted by decoded length in JSON string values and object keys. JSON rejects
invalid UTF-8, byte-order marks, unsupported numeric values, and trailing
input.
YAML rejects a second document marker, including an empty additional document,
and distinguishes malformed content after an explicit document end. This is a
deliberate tightening from ABI 5, which ignored content following the first
YAML document. The 32-node default preserves json-c's former default nesting
envelope and introduces a corresponding limit for YAML.
`fuzz_data_parser` runs both length-aware adapters with constrained budgets;
its committed corpus includes valid typed JSON, trailing JSON, an escaped-NUL
JSON key, duplicate JSON keys, floating-point overflow, ordinary YAML aliases,
recursive aliases, and multiple documents.
Its sanitizer-backed smoke run is part of
`nix build .#checks.x86_64-linux.libmustache-fuzz`. The recorded extended
acceptance run used `-max_total_time=3600`, `-max_len=4096`, the committed
data-parser dictionary, and a copied corpus in the build tree. It completed
52,093,639 executions in 3,601 seconds under ASan/UBSan without a crash,
sanitizer finding, or invariant failure. Post-parse budget validation and
deep-copy failures remain visible to libFuzzer rather than being treated as
expected parser rejection.

### JSON adapter selection and replacement

The implementation spike and decision record are in
[json-parser-evaluation-2026-08-14.md](json-parser-evaluation-2026-08-14.md).
nlohmann/json 3.10.5 or newer was selected and the former json-c adapter was
removed. The replacement uses nlohmann/json's SAX callbacks to construct
`Data` directly and preserves finite floating-point token spelling without a
temporary parser DOM.

The parser is a private header-only build dependency. CMake's build interface,
Autotools compile flags, and Nix `buildInputs` make it available to the private
`json_parser.cpp` adapter, but the installed static target and pkg-config file
do not require it. This is an intentional improvement over json-c, whose link
dependency was exposed to static consumers.

`Data::createFromJSON` remains a compatibility facade. The parser is an
implementation detail, and no partial root is published after syntax,
allocation, numeric-range, encoding, or resource-limit failure.

The spike covered:

- Linux, macOS, and MSVC packaging and installed-consumer behavior;
- shared and static linking, including whether parser dependencies leak into
  the public package interface;
- strict JSON and UTF-8 handling;
- integer, floating-point, negative-zero, and overflow behavior;
- duplicate object keys and exact trailing-input rejection;
- parse-error and exception-path cleanup under sanitizers;
- compile time, compiler peak memory, binary size, and raw parse-and-walk
  throughput; and
- maintenance cost and the process for receiving dependency security updates.

simdjson and yyjson remain documented performance fallbacks. They materially
outperformed nlohmann/json in the synthetic parse-and-walk benchmark, but both
introduce compiled dependencies and would affect only the JSON adapter. The
current PHP zval conversion bypasses that adapter. Do not reopen the choice
unless an end-to-end supported workload actually invokes JSON parsing and
attributes material request-level cost to it.

The direct SAX builder is justified independently of microbenchmark speed:
nlohmann/json's DOM does not retain the original floating-point token, while
its SAX callback supplies that token. The builder also enforces aggregate
budgets during construction and avoids a second full owned tree.

Libmustache imposes its own input-byte,
nesting-depth, node-count, aggregate-string-byte, and allocation budgets. The
adapter rejects trailing input, invalid encodings, unsupported numeric values,
and limit exhaustion with stable library errors. Parser defaults are defense in
depth, not the public resource-limit policy. The JSON fuzz target retains a
behavior corpus covering the former json-c contract. The reviewed differences
are recorded in the parser evaluation: non-finite numeric results and invalid
UTF-8 are now rejected, while escaped-NUL object keys are now preserved
correctly by length.

The same limits must cover YAML conversion. In particular, YAML aliases must
have cycle detection and share the node-expansion budget so cyclic anchors and
alias amplification cannot recurse indefinitely or exhaust memory.

**Definition of done:** JSON, YAML, and direct construction use the owned value
model; no owning raw pointer or separate length remains; the recursive layout
is tested with every supported standard library; the selected JSON parser has
explicit resource limits and fuzz coverage; allocation and parser failure
unwind cleanly under sanitizers; and the scalar golden tests either remain
unchanged or record an intentionally approved behavior change.

## Phase 6: Modernize rendering and lambdas

**Status: renderer resource policy, exception-safe transient state, bounded
compatibility AST reconstruction, and a copyable callback-scoped lambda
capability are implemented on the 0.6 development branch. Migrating downstream
bindings off the retainable legacy renderer-pointer hook remains.**

The renderer should:

- accept `std::string_view` for borrowed input;
- return owned output;
- accept immutable compiled templates;
- avoid persistent borrowed pointers;
- carry explicit recursion and output-size limits; and
- remain exception-safe when callbacks fail.

`RenderLimits` now bounds aggregate bytes appended across all render output
buffers, active node nesting, aggregate node work, and aggregate lambda
template bytes. The defaults are 64 MiB, 256 active levels, 1,000,000 work
units, and 64 MiB respectively; every field is a hard maximum and zero is
never unlimited. The root consumes the first level and work unit. Repeated
sections, partial expansion, callback helper rendering, section source
reconstructed for callbacks, and parsed lambda results share the operation's
counters. Lambda-generated AST nodes are charged once when parsed and again
when traversed, so unvisited nodes still consume the work budget. An
independent 256-level implementation ceiling prevents a caller from turning a
large configured nesting limit into an unbounded C++ call stack.

Output growth, including HTML escaping, is checked before each logical append.
The compatibility renderer now uses a bounds-safe dynamic lookup stack, clears
operation counters and borrowed stack entries on every exit path, restores
temporarily swapped callback output with RAII, and rejects mutation or
top-level re-entry during a render. Renderer objects are non-copyable and
non-movable so borrowed operation state cannot be duplicated.

Standalone partial tags retain their local indentation in validated,
serializable AST metadata. The metadata consumes the ordinary parser,
serializer, and renderer node budgets. Rendering applies the indentation only
to literal template lines, so newlines introduced by variables or lambda
results are not altered. Nested standalone partials compose borrowed
indentation components instead of repeatedly copying cumulative prefixes, and
every emitted indentation byte consumes the aggregate output budget. Inline
partials and lambda-generated templates use isolated source-line state.

Section callbacks now receive a `LambdaRenderContext` on the preferred virtual
path. Copies share invalidatable frame state, render only while their exact
callback is active, and fail cleanly after normal return, exceptions, nested
callback completion, or renderer destruction. The legacy
`invoke(std::string *, Renderer *)` virtual remains as a source-compatibility
adapter and `renderForLambda()` remains available for that adapter. Its raw
pointer must not be retained; downstream bindings must move to the scoped
context before the legacy hook can be deprecated or removed.

The public policy, numeric defaults, zero-value behavior, accounting rules,
and stable exhaustion messages are documented and covered by tests.
php-mustache may select stricter request-appropriate defaults, but it must not
depend on undocumented renderer behavior.

The compatibility-only `Node::children_to_template_string()` and
`Node::to_template_string()` methods now share a checked reconstruction path.
`Node::TemplateStringLimits` defaults to 64 MiB of output, 64 nodes along a
root-to-leaf path, and 100,001 visited nodes including the receiver; zero never
means unlimited. The receiver counts when reconstructing only its children,
and skipped closing nodes still consume work. A separate 256-level ceiling
protects the implementation stack even when callers configure a larger public
limit. Returned strings are transactional, so failures never publish partial
output. Existing overloads apply the defaults.

These helpers and the installed `Stack` API remain available while downstream
code migrates, but are transitional compatibility surfaces. The bounded
renderer uses its own accounting-aware reconstruction and never calls them.
Remove the duplicate compatibility surface only under the Phase 8 source
schedule rather than breaking current consumers silently.

The php-mustache lambda helper requires special handling. PHP can retain the
helper beyond the callback window in which its legacy renderer pointer is
valid. Migrate it to store the library's `LambdaRenderContext`, which:

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

The isolated `test_allocation_failure` executable deterministically replaces
ordinary C++ allocation and sweeps every failure position through tokenization,
JSON and YAML conversion, AST serialization and deserialization, and compiled
rendering with lambdas and partials. Each sweep requires a final successful
operation and verifies that failures neither publish partial destination state
nor make reusable inputs unusable. Running the test under the sanitizer job
also checks that every injected unwind releases its transient allocations.

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
