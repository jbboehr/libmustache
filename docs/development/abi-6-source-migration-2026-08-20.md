# Migrating C++ consumers from ABI 5 to ABI 6

**Date:** 2026-08-20

**Target release:** libmustache 0.6.0 / shared-library ABI 6

**Audience:** C++ applications and language bindings currently built against
libmustache 0.5 / ABI 5

ABI 6 is an intentional binary and source break. It replaces public raw
ownership with RAII, adds resource limits at every untrusted-input boundary,
and introduces an opaque compiled-template API. A binary built against ABI 5
must be recompiled; it must not load ABI 6 as though the public class layouts
were unchanged. The distinct shared-library version prevents that accidental
substitution.

The preferred migration is to stop constructing or retaining public AST
nodes. Compile source into `CompiledTemplate`, construct an owned `Data` value,
and render to an owned string:

```cpp
#include <mustache/mustache.hpp>

mustache::Data data = mustache::Data::object({
    {"name", mustache::Data::string("Ada")},
    {"enabled", mustache::Data::boolean(true)},
});

mustache::CompiledTemplate compiled =
    mustache::compile("Hello {{name}}!");
std::string output = mustache::render(compiled, data);
```

This document describes the compatibility surface on the 0.6 development
branch. ABI 6 is not stable until 0.6.0 is released.

## Build and package changes

- C++17 is required. CMake consumers receive the requirement from the
  imported target; pkg-config consumers receive the compiler spelling selected
  by the build.
- The package version is 0.6.0 and the shared-library ABI is 6 in both CMake
  and Libtool.
- The installed shared CMake target remains `mustache::mustache`.
- The installed static component is requested with
  `find_package(mustache 0.6 CONFIG REQUIRED COMPONENTS static)` and exports
  `mustache::mustache_static`.
- nlohmann/json is a private build dependency. A shared-library consumer does
  not need nlohmann/json or libyaml development files. A static consumer still
  needs libyaml.
- `mustache_config.h` defines `MUSTACHE_CXX_STANDARD` as `17` and
  `MUSTACHE_HAVE_CXX17`. `MUSTACHE_HAVE_CXX11` remains temporarily as a
  source-compatibility alias.
- Public symbols now use an explicit visibility boundary. When bypassing the
  installed CMake or pkg-config metadata for a Windows static build, define
  `MUSTACHE_STATIC_DEFINE` consistently for every consumer translation unit.

Typical CMake use is unchanged apart from the version and C++17 baseline:

```cmake
find_package(mustache 0.6 CONFIG REQUIRED)
target_link_libraries(example PRIVATE mustache::mustache)
```

## Preferred API and compatibility status

The labels below describe the intended 0.6 source policy, not binary
compatibility with ABI 5.

| Surface | Status for 0.6 | Direction |
|---|---|---|
| `CompiledTemplate`, `PartialMap`, and free or member `compile()`/`render()` | Preferred | Use for application and extension code. |
| Owned `Data` factories, accessors, builders, and value-returning parsers | Preferred | Use for all new data conversion. |
| `Tokenizer::Limits`, `Data::ParseLimits`, `Node::SerializationLimits`, and `RenderLimits` | Preferred | Supply workload-specific limits at trust boundaries. |
| `LambdaRenderContext` callback overload | Preferred | Use for section lambdas and binding callbacks. |
| `Mustache`, `Tokenizer`, public `Node`, and stateful `Renderer` APIs | Retained compatibility surface | Migrate ordinary rendering to compiled handles; keep only code that genuinely manipulates an AST here. |
| `Data(Type, int)` and `Data::init()` | Retained compatibility adapter | Replace with named factories and builders. |
| `Data::createFromJSON()` and `createFromYAML()` | Retained ownership adapter | Replace with `fromJSON()` and `fromYAML()`. |
| Pointer-returning `Node::serialize()` and `Node::unserialize()` | Retained ownership adapter | Replace with `serializeValue()` and `unserializeOwned()`. Persist source for new PHP caches; legacy AST reads remain compatible through the window below. |
| `Lambda::invoke(std::string *, Renderer *)` and `Renderer::renderForLambda()` | Downstream-gated transitional API | Keep until php-mustache uses scoped contexts. Never retain the renderer pointer. |
| `Node::to_template_string()` and `children_to_template_string()` | Transitional API | Needed by compatibility code; not used by the bounded compiled renderer. |
| Installed `Stack` template | Transitional API | No longer used by the renderer. Prefer a standard container. |
| `MUSTACHE_HAVE_CXX11` | Transitional macro | Test `MUSTACHE_CXX_STANDARD` or `MUSTACHE_HAVE_CXX17` instead. |
| Version functions, exceptions, and exported utility functions | Supported low-level API | Retained, subject to the signature changes below. |

No compatibility surface currently carries a C++ `[[deprecated]]` attribute.
That is deliberate: bindings commonly build with warnings as errors, and the
primary downstream has not yet migrated. The project should first compile and
test php-mustache against ABI 6, then decide which transitional APIs can be
removed before 0.6.0 and which must remain through the 0.6 release series.
Once ABI 6 is released, an exported method cannot be removed from a compatible
0.6 update merely because the downstream migration has finished.

The audit covered every installed public header:

| Header | ABI 5 to ABI 6 result |
|---|---|
| `mustache.hpp` | Keeps the `Mustache` facade and C version functions; adds the preferred free and member compiled-template API and length-aware overloads. |
| `compiled_template.hpp` | New opaque `CompiledTemplate` and `PartialMap` API. |
| `data.hpp` | Replaces the public pointer representation; adds owned typed values, builders, accessors, length-aware parsers, and parse limits. |
| `lambda.hpp` | Keeps both ABI 5 virtual entry points and adds the scoped section-callback API. |
| `node.hpp` | Replaces raw node ownership, removes `NodeStack`, makes nodes move-only, and adds checked value-returning serialization APIs. |
| `renderer.hpp` | Makes borrowed inputs const, adds rendering limits and callback-state validation, and makes the stateful renderer non-copyable and non-movable. |
| `tokenizer.hpp` | Keeps ABI 5 tokenizer operations and adds `std::string_view` and limited overloads. |
| `stack.hpp` | Retained and bounds-checked, but no longer used by the renderer. |
| `utils.hpp` | Retained with exported symbols and `std::string_view` slice parameters; exact types changed. |
| `exception.hpp` | `Exception` and `TokenizerException` remain source-compatible. |
| `mustache_export.hpp` | New shared/static visibility contract. |
| `mustache_config.h` | Generated feature and version contract now records the C++17 baseline. |

## Migrating `Data`

ABI 5 exposed its discriminator, length, owning pointers, containers of owning
pointers, and lambda pointer. ABI 6 makes the representation private and owns
every recursive value. `Data` is safely copyable: copies deep-copy ordinary
values and intentionally share a lambda through an internal `shared_ptr`.

Replace representation access as follows:

| ABI 5 | ABI 6 |
|---|---|
| `data.type` | `data.type()` |
| `*data.val` | `data.stringValue()` |
| `data.data[key]` | `data.find(key)`, `data.objectItems()`, or `data.set(key, value)` |
| `data.children` | `data.listItems()` or `data.push_back(value)` |
| `data.array` | `data.arrayItems()` or `data.push_back(value)` |
| `data.length` | `data.arrayItems().size()` |
| `data.lambda` | `data.lambdaValue()`, `Data::lambda()`, or `Data::sharedLambda()` |
| `searchStack()` / `searchStackNR()` | No public replacement; name lookup is renderer-owned. |

For example, replace pointer-by-pointer construction:

```cpp
// ABI 5 shape; the caller had to coordinate every allocation and deletion.
mustache::Data * data = new mustache::Data(mustache::Data::TypeMap, 0);
mustache::Data * name = new mustache::Data(mustache::Data::TypeString, 3);
*name->val = "Ada";
data->data["name"] = name;
```

with an owned value:

```cpp
mustache::Data data = mustache::Data::object();
data.set("name", mustache::Data::string("Ada"));
```

`Map`, `List`, and `Array` now contain `Data` values, not `Data *`. Use
`std::move()` when transferring a large subtree into `set()` or `push_back()`.
Container emptiness comes from the actual container size; there is no separate
mutable length to keep synchronized.

Prefer the value-returning parsers:

```cpp
mustache::Data value = mustache::Data::fromJSON(jsonBytes, limits);
mustache::Data yamlValue = mustache::Data::fromYAML(yamlBytes, limits);
```

The legacy `createFromJSON()` and `createFromYAML()` functions still return a
raw owning pointer which the caller must delete. They are adapters over the
same checked parsers and should not be introduced into new code.

`Data::find()` and the container accessors return borrowed pointers or
references. Do not retain them across structural changes to the owning tree,
and do not structurally modify a `Data` tree while it is being rendered.

ABI 6 additionally preserves null, boolean, signed integer, floating-point,
string, list, array, object, and lambda types. The existing numeric enum values
for ABI 5 types remain unchanged; the scalar enum values are additions, not a
license to persist the in-memory representation.

## Migrating templates, nodes, and partials

Most consumers should replace this flow:

```cpp
mustache::Node root;
mustache::Mustache engine;
engine.tokenize(&source, &root);
engine.render(&root, &data, &partials, &output);
```

with:

```cpp
mustache::Mustache engine;
mustache::CompiledTemplate compiled = engine.compile(source);
std::string output = engine.render(compiled, data);
```

`CompiledTemplate` is an opaque, immutable, inexpensive-to-copy handle. It
owns its parsed representation independently of the source string and the
`Mustache` instance that compiled it. A default-constructed handle is empty;
rendering an empty template handle or empty compiled partial throws a
`mustache::Exception`.

Compile partials separately and transfer their handles into `PartialMap`:

```cpp
mustache::PartialMap partials;
partials.emplace("header", mustache::compile("<h1>{{title}}</h1>"));
std::string output = mustache::render(page, data, partials, limits);
```

Code that must continue manipulating the compatibility AST needs mechanical
ownership changes:

| ABI 5 node member | ABI 6 node member |
|---|---|
| `std::string * data` | `std::optional<std::string> data` |
| `std::vector<std::string> * dataParts` | `std::vector<std::string> dataParts` |
| `std::vector<Node *> children` | `std::vector<std::unique_ptr<Node>> children` |
| borrowed `Node * child` | owned `std::unique_ptr<Node> child` |
| `std::map<std::string, Node> partials` | `std::map<std::string, std::unique_ptr<Node>> partials` |
| delimiter string pointers | `std::optional<std::string>` values |

Construct child nodes with `std::make_unique<Node>()` and move ownership into
the destination. `Node` is move-only; copying is intentionally unavailable.
The tokenizer and decoder construct into temporary owned trees and publish a
complete result only after success.

`NodeStack` was removed. It was an implementation-specific fixed-depth parser
stack and has no public replacement. If unrelated consumer code used it as a
generic container, migrate that code to `std::vector`, `std::stack`, or another
appropriate standard container.

The `Node::Type` values inherited from ABI 5 retain their numeric values.
`FlagLambdaOnly` and `FlagPartialIndent` are internal metadata and should not
be manufactured by consumers. Serialization and deserialization validate
their placement; the renderer defensively validates partial-indentation
metadata but does not validate every property of a manually constructed tree.
Consumers that build nodes directly remain responsible for canonical shapes.

## Serialized AST migration

Prefer RAII and explicitly sized bytes:

```cpp
std::vector<std::uint8_t> bytes = root.serializeValue(serializationLimits);

std::string_view encoded(
    reinterpret_cast<const char *>(bytes.data()), bytes.size());
std::unique_ptr<mustache::Node> decoded =
    mustache::Node::unserializeOwned(encoded, serializationLimits);
```

The pointer-returning overloads remain available for source compatibility and
apply the same structural validation and resource policy. The complete-input
`unserializeOwned()` API rejects trailing bytes rather than publishing a
partial tree.

Canonical ABI 5 fixtures remain readable by ABI 6. Do not infer the reverse:
newly compiled ABI 6 templates may use metadata an ABI 5 decoder does not
understand. Do not replace durable ABI 5 cache entries in place if old
processes must continue reading them.

The [PHP cache benchmark](ast-cache-benchmark-2026-08-22.md) compared validated
decoding with cached-source reparsing, including warm and fresh-process cases,
PHP serialization, and APCu fetch/store overhead. It selected source for new
cross-request caches and rejected investment in another persistent AST format.
Therefore:

- retain checked reads of existing ABI 5 data throughout libmustache 0.6.x and
  php-mustache 0.x;
- deprecate AST writes when the PHP compiled-handle path and source-cache
  guidance ship;
- remove legacy reads only in a separately announced incompatible release;
- use versioned cache keys when ABI 5 and ABI 6 processes can overlap;
- treat serialized input as untrusted and supply request-appropriate limits;
  and
- cache source rather than writing new durable AST entries.

## Migrating lambdas

Variable lambdas continue to override `invoke()`.

Existing section lambdas that override
`invoke(std::string *, Renderer *)` continue to dispatch through the legacy
virtual method. The renderer pointer is valid only for the active callback and
must never be retained.

New and migrated section lambdas should override the scoped overload:

```cpp
class SectionLambda : public mustache::Lambda {
  public:
    std::string invoke() override { return {}; }

    std::string invoke(
        std::string_view,
        mustache::LambdaRenderContext context) override
    {
      mustache::Node name(
          mustache::Node::TypeVariable, "name", mustache::Node::FlagEscape);
      return context.render(name);
    }
};
```

Every copy of a `LambdaRenderContext` becomes inactive when that exact callback
returns or throws. Later use throws without dereferencing the old renderer.
Nested callbacks have independent frames. Rendering through a context is
synchronous and not intended for concurrent use.

A binding should translate all `mustache::Exception` and callback failures
before returning through its language runtime's C ABI. No C++ exception may
cross the Zend or another C boundary.

## Limits and failure behavior

ABI 6 exposes limits for template tokenization, JSON/YAML conversion, AST
serialization, template-source reconstruction, and rendering. Every field is
a hard maximum; zero never means unlimited. Defaults are compatibility limits,
not necessarily appropriate limits for an internet-facing request.

Use explicit lengths (`std::string_view`) for strings originating in another
runtime. This preserves embedded NULs where that input permits them and avoids
an unbounded sentinel scan. JSON and YAML reject raw NUL bytes; JSON escape
sequences such as `\u0000` remain length-aware values.

Operations that return a value publish it transactionally. The legacy
renderer writes into a caller-owned string, so a failed render may leave a
prefix in that string; discard or clear it before reuse. The renderer restores
its internal transient state after a limit failure or callback exception.

Catch `mustache::TokenizerException` when line and character information is
useful, and otherwise catch `mustache::Exception`. Applications should not
make control-flow decisions from the exact English text of an error unless a
specific message is documented as stable.

## Other public-header changes

- `Mustache::render()` and the stateful `Renderer` now borrow `const Node` and
  `const Data`. Calls passing mutable pointers continue to compile, but exact
  member-function pointer declarations must be updated.
- `Renderer` and therefore `Mustache` are non-copyable and non-movable. Their
  transient borrowed state must not be duplicated. `CompiledTemplate` is the
  copyable reusable object.
- `utils.hpp` string-slice parameters and the `whiteSpaces` and `specialChars`
  constants now use `std::string_view`. Ordinary calls with `std::string`
  continue to work; code depending on their exact types or function-pointer
  signatures must be updated.
- `renderer.hpp` no longer provides incidental declarations from
  `tokenizer.hpp`, `utils.hpp`, or `<iostream>`. Include every defining header
  directly.
- `mustache_version()` and `mustache_version_int()` remain C-linkage version
  queries. The integer form encodes 0.6.0 as `600`.
- The obsolete hash-container configuration macros are gone. Use
  `Data::Map`; do not depend on its underlying container in public interfaces.

## Migration checklist

1. Require libmustache 0.6 and C++17 in the consumer build.
2. Recompile every C++ translation unit; do not mix ABI 5 and ABI 6 objects.
3. Replace direct `Data` fields and child pointers with owned factories,
   accessors, `set()`, and `push_back()`.
4. Prefer `CompiledTemplate` and `PartialMap` over retained `Node` graphs.
5. If AST access remains, convert every node edge to explicit `unique_ptr`
   ownership and remove copies.
6. Prefer value-returning parsers and serializers.
7. Move section callbacks to `LambdaRenderContext` and prevent C++ exceptions
   from crossing a binding boundary.
8. Pass explicit lengths and workload-specific limits for untrusted input.
9. Version or invalidate persistent AST caches during mixed-version rollout.
10. Run the consumer's normal, sanitizer, callback-retention, malformed-input,
    and cache round-trip tests before deployment.

For php-mustache specifically, complete the owned zval-to-`Data` conversion,
lambda-context migration, compiled-handle ownership, and source-cache guidance
before deprecating AST writes. Keep checked reads for the compatibility window
above.
