# Archived-template API ergonomics review

**Date:** 2026-08-29

**Scope:** The public archived-template C++ surface that php-mustache and other
native consumers are expected to use. This is an API-contract review, not a
correctness or security review of the Cista implementation.

**Compatibility status:** libmustache 0.6.0 and shared-library ABI 6 are not yet
released, so source and ABI adjustments to this experimental API can still be
made before consumers depend on it.

## Consumer model

**Primary caller:** A C++/PHP-extension integrator familiar with native resource
ownership but not libmustache's private AST or Cista implementation.

**Job:** Compile templates and partials, persist opaque archive bytes, load one
validated template per request or worker, and render it repeatedly without
reparsing.

**Context:** C++17, a Zend/C ABI boundary across which exceptions must not
escape, workload-specific resource limits, persistent caches such as APCu, and
possibly build-time precompilation followed by deployment to multiple targets.

**Likely approach:** Start with the README, use completion around `compile()`,
`render()`, and `ArchivedTemplate*`, then inspect installed public headers when
the modern and archive paths do not compose.

**Secondary caller:** A C++ application that wants to precompile templates into
deployment artifacts and render them through the ordinary libmustache value
model.

## Consumer examples

### Current happy path

The preferred path now stays on the opaque compiled-template surface:

```cpp
mustache::CompiledTemplate compiled =
    mustache::compile("Hello {{name}}");
std::vector<std::uint8_t> bytes =
    mustache::serializeArchivedTemplate(compiled);
mustache::ArchivedTemplate archived =
    mustache::loadArchivedTemplate(bytes);
std::string output = mustache::render(archived, data);
```

### Advanced composed use

Compiled partials compose with the same archive writer:

```cpp
mustache::CompiledTemplate compiled = mustache::compile("{{> card}}");
mustache::PartialMap partials;
partials.emplace("card", mustache::compile("Hello {{name}}"));

mustache::ArchivedTemplateLimits limits;
limits.maxInputBytes = 1024 * 1024;
std::vector<std::uint8_t> bytes =
    mustache::serializeArchivedTemplate(compiled, partials, limits);
```

### Resolved near-miss

At review time, a caller following the preferred ABI 6 API naturally wrote:

```cpp
auto compiled = mustache::compile("Hello {{name}}");
auto bytes = mustache::serializeArchivedTemplate(compiled);
```

This did not compile. A focused compiler probe failed because
`serializeArchivedTemplate()` requires `const Node&`, not
`const CompiledTemplate&`. Issue 1 added that composition point.

Another legal but fallible state is:

```cpp
mustache::ArchivedTemplate archived;
// A failed or skipped assignment leaves archived empty.
std::string output = mustache::render(archived, data);
```

This compiles and throws `mustache::Exception` only when rendered. The nullable
handle pattern matches `CompiledTemplate`, so it is not independently treated
as a defect below, but it contributes to the lifecycle and error-model tradeoff.

## Findings

### Issue 1 — High: The archive writer requires the legacy `Node` API

- **Contract:** `src/archived_template.hpp:88-90`
- **Conflicting direction:**
  `docs/development/abi-6-source-migration-2026-08-20.md:79-86` identifies
  `CompiledTemplate` and `PartialMap` as preferred while public `Node` is a
  retained compatibility surface.
- **Consumer scenario:** An application or extension compiles templates through
  the modern opaque handle, then tries to persist the result.
- **Evidence:** At review time, the compiler rejected
  `serializeArchivedTemplate(compiled)` with an invalid conversion from
  `CompiledTemplate` to `const Node&`. The README therefore teaches manual
  `Node` tokenization instead of the preferred API.
- **Why it hurts:** The new persistent format entrenches the exact low-level AST
  surface the modernization is trying to retire. Callers must learn public node
  ownership, move-only partial maps, and tokenizer details that the opaque
  compiled handle otherwise hides.
- **Recommended shape:** Add the modern composition point:

  ```cpp
  MUSTACHE_API std::vector<std::uint8_t> serializeArchivedTemplate(
      const CompiledTemplate& compiled,
      const PartialMap& partials = PartialMap(),
      const ArchivedTemplateLimits& limits = ArchivedTemplateLimits());
  ```

  Make this the README path. Retain the `Node` overload temporarily as an
  explicitly advanced compatibility entry point, or make it internal if no
  downstream requires it.
- **Compatibility:** This is additive if the existing overload remains. Because
  ABI 6 is unreleased, the low-level overload can still be renamed or hidden
  without breaking a released contract.
- **Resolution:** Implemented with a `CompiledTemplate` plus `PartialMap`
  overload. The README and installed-consumer path use the opaque compiled
  representation; the `Node` overload remains the advanced compatibility path.
- **Confidence:** High. The declaration, migration policy, README, and compiler
  result agree.

### Issue 2 — Medium: Native archive compatibility is not available as a cache-key contract

- **Contract:** `docs/development/cista-archive-format-v2.md`.
- **Consumer scenario:** Archives are produced during a build and deployed to
  multiple architectures, C++ ABIs, or rolling libmustache versions, or are
  retained in a shared persistent cache while processes are upgraded.
- **Evidence:** The outer preamble identifies only libmustache format generation
  `1`; the complete golden bytes are explicitly specific to x86-64,
  little-endian, and the Itanium C++ ABI. Cista's protected reader can reject an
  incompatible native graph, but the public API exposes no compatibility
  identifier suitable for cache names.
- **Why it hurts:** Consumers cannot proactively isolate incompatible entries
  without duplicating private knowledge about the archive schema, Cista type
  representation, architecture, and ABI. The error is safe rejection rather
  than corruption, but it becomes a runtime cache miss or deployment failure.
- **Recommended shape:** Expose one opaque, library-owned identifier:

  ```cpp
  MUSTACHE_API std::string_view
  archivedTemplateCompatibilityTag() noexcept;
  ```

  The tag should change whenever existing archive bytes cease to be readable and
  should cover the native layout/type domain as well as the outer format
  generation. PHP and build integrations should include it directly in cache
  keys. Embed the same compatibility discriminator in the preamble and advance
  the experimental format generation deliberately.
- **Compatibility:** Adding a query is source- and ABI-additive. Changing the
  preamble requires a deliberate format-generation decision, which is still
  feasible while the format is experimental.
- **Resolution:** Implemented by format generation 2. The public opaque tag and
  the preamble carry the same native-layout fingerprint, and generation-1
  experimental archives are rejected explicitly.
- **Confidence:** High for the contract gap; cross-architecture behavior was
  inferred from the documented native representation and platform-specific
  fixture rather than executed locally.

### Issue 3 — Medium: `ArchivedTemplateView` is an owning value, not a view

- **Contract:** `src/archived_template.hpp:35-55`.
- **Consumer scenario:** A binding author receives archive bytes from a Zend
  string or APCu entry and decides whether the source storage must outlive the
  loaded handle.
- **Evidence:** Both load overloads defensively copy the bytes. The handle owns
  immutable shared state and remains valid independently of the caller's input.
- **Why it hurts:** In C++, “view” conventionally communicates a borrowed,
  non-owning lifetime. A hurried caller may unnecessarily retain the source
  buffer, misunderstand copy costs, or assume the handle becomes invalid when
  the input is released. The documentation corrects the name, but only after the
  caller reads it.
- **Recommended shape:** Rename the public value to `ArchivedTemplate`, parallel
  to `CompiledTemplate`:

  ```cpp
  mustache::ArchivedTemplate archived =
      mustache::loadArchivedTemplate(bytes);
  ```

  `LoadedArchivedTemplate` is another accurate option, but the shorter name
  gives the two opaque renderable representations the clearest symmetry.
- **Compatibility:** Renaming the class changes C++ symbols and source. Do it
  before ABI 6 is released rather than retaining a misleading name or a permanent
  alias.
- **Resolution:** Renamed to `ArchivedTemplate` without retaining an alias. The
  load operation, owning shared state, copy/move behavior, and render semantics
  are unchanged.
- **Confidence:** High. The public declaration and implementation describe
  unambiguous owning semantics.

### Issue 4 — Medium: Archive limits are positional and bake defaults into callers

- **Contract:** `src/archived_template.hpp:25-33` and
  `src/archived_template.hpp:35-41,88-90`.
- **Consumer scenario:** A packager or extension tightens archive limits, or a
  dynamically linked libmustache update needs to revise safe defaults without
  recompiling every consumer.
- **Evidence:** A compiler probe confirmed `ArchivedTemplateLimits` is an
  aggregate and accepts six positional `std::size_t` arguments. Its default
  member initializers are compiled into each caller, unlike the exported
  constructors used by `Tokenizer::Limits`, `Data::ParseLimits`,
  `Node::SerializationLimits`, and `RenderLimits`. In
  `src/archive/cista.cpp:1158-1174`, `maxInputBytes` limits serialized output;
  in `src/archive/cista.cpp:1425-1441`, it limits loader input.
- **Why it hurts:** Six same-typed positional values can be reordered without a
  compiler error. `maxInputBytes` is also misleading on the writer path, and a
  shared-library update cannot revise the default policy for already-compiled
  call sites.
- **Recommended shape:** Make the type non-aggregate with a library-owned
  constructor and use representation-neutral total names:

  ```cpp
  struct ArchivedTemplateLimits {
      std::size_t maxArchiveBytes;
      std::size_t maxNestingDepth;
      std::size_t maxNodes;
      std::size_t maxTotalStringBytes;
      std::size_t maxDataPartsPerNode;
      std::size_t maxTotalDataParts;

      MUSTACHE_API ArchivedTemplateLimits();
  };
  ```

  Keeping one shared archive limit type is useful because it preserves the
  writer-to-reader round-trip policy; separate writer and loader types are not
  necessary unless their policies diverge later.
- **Compatibility:** The constructor and field renames are source changes, and
  changing a public struct's layout is ABI-sensitive regardless. Make the change
  before ABI 6 is frozen. Existing named field assignments require mechanical
  updates; fragile positional initialization should intentionally stop compiling.
- **Confidence:** High. The aggregate probe compiled and the two observable uses
  of `maxInputBytes` are explicit.

### Issue 5 — Low: Archive-loading failures have no machine-readable category

- **Contract:** `src/exception.hpp:11-35` and the load overloads in
  `src/archived_template.hpp:35-41`.
- **Consumer scenario:** A PHP/APCu integration must decide whether to evict and
  recompile a stale or corrupt cache entry, report a deployment incompatibility,
  or surface a configured resource limit that is too small.
- **Evidence:** Invalid magic, unsupported format generation, integrity failure,
  structural invalidity, and resource-limit rejection all surface through
  `mustache::Exception`. The caller can distinguish them only by parsing
  `what()` text or treating every load failure identically.
- **Why it hurts:** Treating every failure as a cache miss is workable for the
  experiment, but it weakens diagnostics and can cause repeated recompilation
  when the actual problem is policy or deployment configuration.
- **Recommended shape:** Add an archive-specific subtype that remains catchable
  as `mustache::Exception`:

  ```cpp
  enum class ArchivedTemplateError {
      invalidArchive,
      unsupportedFormat,
      limitExceeded
  };

  class ArchivedTemplateException : public Exception {
    public:
      ArchivedTemplateError reason() const noexcept;
  };
  ```

  Keep ordinary render-time data, lambda, and output-limit failures on the
  existing render error path.
- **Compatibility:** An additive derived exception preserves existing
  `catch (const mustache::Exception&)` behavior. The reason values become part of
  the source contract and should remain open to compatible additions.
- **Confidence:** High for the current undifferentiated error model; medium that
  consumers require more than a single “archive miss” recovery policy.

## Lens summary

This table reflects the current contract after the recorded resolutions.

| Lens | Assessment | Reason |
|---|---|---|
| Obvious path | Strong | The documented path compiles, serializes, loads, and renders through opaque owning handles. |
| Misuse resistance | Mixed | Validation and immutable ownership are strong, but raw archive bytes, nullable handles, and six positional limit values allow plausible near-misses. |
| Naming and symmetry | Strong | `ArchivedTemplate` parallels `CompiledTemplate`, and `load`/`serialize`/`render` use one vocabulary. |
| Signatures | Mixed | Modern and advanced writer overloads compose predictably, but aggregate archive limits still permit positional mistakes. |
| Type and IDE experience | Strong | Opaque compiled templates and typed partial maps flow directly into the archive writer without exposing Cista. |
| Error model | Mixed | Exceptions are consistent with libmustache, but archive load reasons are not machine-readable. |
| Lifecycle | Strong to mixed | Ownership and cheap copies are explicit, but a default-constructed empty handle remains fallible only when used. |
| Defaults | Mixed | Defaults are conservative and bounded, but are embedded inline in consumer code and one byte-limit name changes meaning by operation. |
| Composability | Strong | `CompiledTemplate` and `PartialMap` feed the writer directly while `Node` remains an advanced path. |
| Progressive disclosure | Strong | The common path stays opaque; callers encounter the low-level AST only when deliberately selecting it. |
| Discoverability | Strong to mixed | Names expose ownership and cache compatibility, while limit and failure details still require documentation. |
| Evolution | Mixed | The format and cache domain are versioned, but aggregate limits still bake defaults into callers before ABI freeze. |

Asynchronous operation and cancellation are not applicable: compilation,
archive loading, and rendering are intentionally synchronous CPU-bound library
operations under caller-controlled resource limits.

## What already works well

- Loading copies the caller's bytes before validation and retains immutable
  private storage, preventing caller aliases from mutating a validated graph.
- The handle is an ordinary RAII value with cheap copies and no manual cleanup.
- Cista and xxHash implementation types do not leak into installed declarations.
- The public reader exposes only the fixed protected archive policy; callers
  cannot accidentally select an unsafe benchmark mode.
- Archive validation limits and render-work limits are separate, coherent
  concepts.
- Free `render()` and `Mustache::render()` provide parallel default and bounded
  forms.
- The feature macro makes optional availability visible at compile time.
- Archive bytes are validated once, then rendered through the same semantic
  engine as owned nodes without reconstructing an AST.

## Recommended contract evolution

Before php-mustache or another consumer treats ABI 6 as stable:

1. Rename `ArchivedTemplateView` to `ArchivedTemplate` so ownership is visible
   from the type name. Completed before ABI 6 release.
2. Add the `CompiledTemplate` plus `PartialMap` archive-writer overload and make
   it the primary example and integration path. Completed.
3. Give `ArchivedTemplateLimits` an exported constructor, rename
   `maxInputBytes` to `maxArchiveBytes`, and clarify aggregate total fields.
4. Expose a library-owned archive compatibility tag for cache keys and embed
   the same native compatibility discriminator in the preamble. Completed in
   format generation 2.
5. Add an archive-specific exception reason before persistent-cache recovery
   behavior is published, if the PHP integration demonstrates a need to
   distinguish cache misses from configuration failures.
6. Retain the `Node` writer only as a clearly labeled compatibility/advanced
   surface until downstream migration confirms whether it is still necessary.

The first three changes form one coherent API slice: they make the ordinary
modern compiled-template path composable, clarify the owning value model, and
regularize resource policy before ABI 6 freezes.

## Review evidence and limitations

- Public declarations, README examples, installed-consumer usage, exception
  types, format documentation, and ABI-6 migration policy were inspected.
- A disposable compiler probe confirmed that, at review time, the preferred
  `CompiledTemplate` call did not compile.
- A second disposable compiler probe confirmed that `ArchivedTemplateLimits`
  is an aggregate and accepts positional initialization, and that the archived
  template handle is publicly default-constructible.
- The repository's existing consumer and archive tests provide executable
  evidence for the current happy path, owning-copy behavior, empty-handle
  rejection, resource limits, and free/member render symmetry.
- Cross-architecture archive reuse was not executed locally; the compatibility
  finding relies on the documented native representation and explicitly
  platform-specific golden fixture.
- IDE completion behavior was inferred from the public declarations rather than
  observed in a specific IDE.
- All disposable probes were removed. This review did not modify tracked source
  files.
