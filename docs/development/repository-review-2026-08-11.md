# Repository review: libmustache

Date: 2026-08-11

Reviewed revision: `develop` at `f8afd02`

Scope: authored C++ library and CLI code, public headers, tests and fixtures, CMake and Autotools builds, Nix packaging, CI configuration, and user/developer documentation. The pinned Mustache spec and bundled third-party Autotools macros were reviewed as integrations, not audited as upstream source.

## Executive summary

The repository is small and its tokenizer/data/renderer separation is easy to follow, but it is not currently safe to treat a green build as evidence of production readiness. The most urgent problems are:

- JSON and YAML arrays are written outside the vector's logical bounds.
- Owning `Data` and `Node` objects are implicitly copyable, producing double-free/use-after-free failures.
- `Node::unserialize` trusts malformed input and reads beyond its buffer.
- The conformance test removes all whitespace before comparing results; exact comparison exposes 33 failures among 137 executed spec cases.
- The default Nix package builds successfully but its installed CLI cannot load `libmustache.so.5`.

The first three issues are memory-safety defects. The test defect also masks a significant, systematic Mustache conformance gap around standalone tags and partial indentation. These should be fixed before extending the API or adding more features.

Overall assessment: **high risk for untrusted input or general library consumption; medium risk for controlled, narrow usage that avoids copying objects, serialized trees, YAML booleans/nulls, and whitespace-sensitive templates.**

## What is working well

- The core implementation is compact and divided into recognizable responsibilities: `Tokenizer`, `Node`, `Data`, and `Renderer`.
- Both Autotools and CMake variants build successfully with GCC 15.2 through the pinned Nix environment.
- Dependencies and the external Mustache test corpus are reproducibly pinned for Nix builds.
- Basic interpolation, escaping, dotted lookup, sections, partials, and the supported lambda cases pass exact-output tests.
- The project has Linux, Nix, and Windows CI intent, along with formatting and lint hooks.

## Prioritized findings

### 1. High: parsed arrays violate `std::vector`'s size contract

[`Data::init`](../../src/data.cpp#L57) calls `array.reserve(size)`, which allocates capacity but leaves `array.size()` at zero. Both the JSON parser at [`data.cpp:201`](../../src/data.cpp#L201) and YAML parser at [`data.cpp:267`](../../src/data.cpp#L267) then assign through `array[cindex]`. Every non-empty parsed array therefore performs an out-of-bounds vector access in C++ terms.

Observed with `Data::createFromJSON("[1, 2]")`:

```text
length=2 size=0
```

The writes often appear to work because they land in reserved storage, but object lifetime was never established there. The destructor iterates `array.size()`, so it also leaks all children created this way. Rendering later indexes the same logically empty vector using the separate `length` member at [`renderer.cpp:202`](../../src/renderer.cpp#L202).

Recommendation:

- Make the vector the sole source of truth and remove the parallel `length` state.
- Use `push_back`/`emplace_back`, or `resize` before indexed assignment.
- Prefer `std::unique_ptr<Data>` elements so partial construction unwinds safely.
- Add parser tests for empty, single-element, nested, and malformed arrays under sanitizers and `_GLIBCXX_ASSERTIONS`.

### 2. High: owning public types are implicitly copyable

[`Data`](../../src/data.hpp#L27) and [`Node`](../../src/node.hpp#L20) own raw pointers and recursively delete them, but neither type defines nor deletes copy construction and copy assignment. The compiler-generated copies duplicate pointer values, after which both objects delete the same allocations. `Node::Partials` also stores `Node` by value, making safe value semantics especially important.

An AddressSanitizer probe that performs only this operation crashes during destruction:

```cpp
mustache::Node first(mustache::Node::TypeOutput, "owned text");
mustache::Node second = first;
```

The same ownership ambiguity applies to `Data::lambda`, map/list/array children, node children, delimiter strings, and `Node::child`. `Data::init` can also be called repeatedly without releasing the previous active representation.

Recommendation:

- Replace owning raw pointers with values or `std::unique_ptr`.
- Either implement deliberate deep-copy and move semantics or delete copying and support moves.
- Document ownership at API boundaries, especially `Lambda` ownership.
- Make `Data`'s representation private and enforce its active type through constructors/factories rather than public mutable fields.

### 3. High: serialized-node parsing performs unchecked reads

[`Node::unserialize`](../../src/node.cpp#L233) verifies only the first two magic bytes. It then reads all header fields, the declared data payload, and recursively declared children without verifying that enough bytes remain. An offset greater than the input size also underflows `serial.size() - pos`. Declared `children_size` is parsed but never used for validation.

A two-byte input containing only `MU` immediately aborts under `_GLIBCXX_ASSERTIONS` on an out-of-range vector access. Other malformed values can cause excessive allocation, deep recursion, leaks during partial construction, or reads beyond the buffer. The 24-bit data length and 16-bit child count are also silently truncated by serialization without range checks.

Recommendation:

- Parse through a bounds-checked cursor/span abstraction with a `remaining()` check before every read.
- Validate type/flag values, declared lengths, child counts, nesting depth, and exact end position.
- Add a format version and reject unsupported or non-canonical encodings.
- Build child nodes under RAII so an exception cleans up already-parsed state.
- Fuzz both tokenization and deserialization with ASan/UBSan enabled.

### 4. High: the conformance suite is false-green

[`MustacheSpecTest::passed`](../../tests/test_spec.hpp#L34) removes every whitespace character from both actual and expected output before comparison. That hides standalone-line behavior, blank-line errors, partial indentation errors, and even meaningful spaces inside rendered text.

Changing only this comparison to `output == expected` produced:

```text
104 passed, 0 skipped, 33 failed of 137 tests
```

Failures are concentrated in standalone comments, sections, inverted sections, delimiter changes, and partial indentation. Two extension suites are discarded at [`test_spec.cpp:162`](../../tests/test_spec.cpp#L162), one array test returns without being recorded at [`test_spec.cpp:205`](../../tests/test_spec.cpp#L205), and `nSkipped` is never incremented. The CMake test definition also omits `test_misc` entirely in [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt#L7), while Autotools runs it.

Recommendation:

- Compare spec output byte-for-byte and preserve diagnostic escaping for tabs/newlines.
- Mark unsupported cases explicitly as skipped and fail if the executed/skipped totals unexpectedly change.
- Implement Mustache standalone-tag stripping and indentation propagation before accepting the newly exposed failures.
- Run the same test inventory from CMake and Autotools.
- Add sanitizer, assertions, and warning-clean jobs; the current suite does not detect the ownership and array defects.

### 5. High: closing sections are not matched to opening sections

The tokenizer creates a stop node and pops the stack at [`tokenizer.cpp:208`](../../src/tokenizer.cpp#L208), but never compares the closing tag's name with the open section. `{{#open}}...{{/different}}` is accepted. An orphan closing tag is checked against `size() <= 0`, even though the root occupies stack slot one; a template such as `before{{/orphan}}after` pops the root and later aborts with `Reached bottom of stack`.

Recommendation:

- Require at least one open section above the root before accepting a close.
- Compare closing and opening names and report both source locations on mismatch.
- Do not mutate or attach nodes until validation succeeds.
- Add focused tests for orphan, mismatched, repeated, nested, and delimiter-changed closing tags.

### 6. High: the default Nix package installs an unusable executable

The derivation splits `out`, `dev`, and `lib` outputs at [`nix/derivation.nix:44`](../../nix/derivation.nix#L44). Its `postInstall` RPATH uses `$out/lib` at [`nix/derivation.nix:65`](../../nix/derivation.nix#L65), while the Autotools shared library is placed in the separate `$lib/lib` output.

The build and tests pass, but the installed default executable fails:

```text
result/bin/mustachec: error while loading shared libraries:
libmustache.so.5: cannot open shared object file: No such file or directory
```

In addition, `nix run .#` searches for `bin/libmustache` because no `meta.mainProgram` or flake app identifies `mustachec`.

Recommendation:

- Include the actual library output in the executable RPATH, preferably through normal Nix fixup behavior rather than a hard-coded `patchelf` call.
- Add `meta.mainProgram = "mustachec"` or an explicit `apps.default`.
- Add an install check that executes `$out/bin/mustachec -v` and renders a minimal fixture after fixup.
- Test output splitting for both build-system variants; the CMake CLI currently works only because it links `mustache_static`.

### 7. High: the CLI's documented interface and failure handling are broken

The option parser accepts `hrvd:o:t:n:l:` at [`bin/mustache.cpp:67`](../../bin/mustache.cpp#L67), but usage advertises `-c`, `-e`, and `-p` at [`bin/mustache.cpp:241`](../../bin/mustache.cpp#L241). Those options are not accepted, and the related `mode`/`printReadable` state is unused. `mustachec -e ...` exits with `Unknown option '-e'`.

The top-level convenience targets compound this by invoking `./bin/mustache` rather than the built `mustachec` binary in [`Makefile.am:11`](../../Makefile.am#L11). There is no implemented compile/read-bytecode path despite the advertised modes and `Node` serialization API.

Only data parsing is inside an exception handler. Template/partial reads, tokenization, and rendering can terminate the process. Omitting `-d`, supplying an empty/unknown-suffix data file, or rendering with no constructed `Data` reaches an uncaught `Empty data` exception. Output-file open/write errors are not checked.

Recommendation:

- Decide whether this is a render-only CLI or restore compile/execute/print modes; make usage, options, tests, and Make targets agree.
- Put the full operation behind top-level `std::exception` handling and return stable nonzero status codes.
- Validate all option values, input formats (`.yaml` as well as `.yml` if desired), input/output streams, and iteration counts.
- Use automatic storage instead of the leaked heap-allocated output string.
- Add end-to-end CLI tests for success and every error path.

### 8. Medium: YAML and JSON disagree on false and null

The JSON adapter maps false and null to empty strings at [`data.cpp:170`](../../src/data.cpp#L170), which the renderer treats as falsey. The YAML adapter maps every scalar to its text at [`data.cpp:244`](../../src/data.cpp#L244), making `false` and `null` non-empty, truthy strings.

For the same template, observed behavior was:

```text
YAML { flag: false } -> yes
JSON { "flag": false } -> no
```

Recommendation:

- Interpret YAML scalar tags/types consistently with the JSON adapter and Mustache truthiness rules.
- Represent null/boolean distinctly instead of encoding them as magic strings where practical.
- Share adapter conformance tests across JSON and YAML for null, booleans, numbers, strings, objects, and nested arrays.

### 9. Medium: the CMake install contract is incomplete and differs from Autotools

The CMake targets in [`src/CMakeLists.txt`](../../src/CMakeLists.txt#L26) do not publish build/install include directories, so exported targets have no `INTERFACE_INCLUDE_DIRECTORIES`. The export is installed as a raw target file under `include/cmake`, without a discoverable package config/version file or namespace. The shared library has no `VERSION`/`SOVERSION`, while Autotools intentionally installs ABI version 5 at [`src/Makefile.am:41`](../../src/Makefile.am#L41).

The generated installed CMake header also leaves major and patch undefined because `#cmakedefine` treats their zero values as false in [`cmake/mustache_config.h.in`](../../cmake/mustache_config.h.in#L2).

Recommendation:

- Define modern target usage requirements with `target_include_directories`, `target_link_libraries`, and build/install interfaces.
- Install a namespaced `mustacheConfig.cmake` plus version file in the conventional library CMake directory.
- Align ABI/version behavior and feature macros across CMake and Autotools.
- Add consumer tests that install each variant, then build and run a separate downstream project using pkg-config/CMake discovery.
- Verify Windows DLL exports explicitly; the public headers contain no export macro and CMake does not enable automatic symbol export.

### 10. Medium: CI and repository hooks are stale and are not enforced together

Running the configured hooks across the repository produced:

```text
actionlint: failed (ubuntu-20.04 is unknown; actions/checkout@v3 is too old)
alejandra: passed
shellcheck: failed (.envrc has no shell directive/shebang)
```

The workflow still includes `ubuntu-20.04` and Checkout v3 at [`.github/workflows/ci.yml:17`](../../.github/workflows/ci.yml#L17). GitHub's current hosted-image list no longer includes Ubuntu 20.04, and Checkout has moved to Node 24 releases ([runner images](https://github.com/actions/runner-images), [Checkout](https://github.com/actions/checkout)). The workflow also installs a `nixos-24.05` channel while the flake pins 26.05, and pull-request CI only targets `master` even though active development is on `develop`.

The Nix `pre-commit-check` at [`flake.nix:64`](../../flake.nix#L64) is used only as a development-shell hook. It is not added to `checks`, so `nix flake check` and the generated Nix CI matrix do not enforce it. AppVeyor excludes `develop` and tests only Visual Studio 2017.

Recommendation:

- Move Linux jobs to supported runner images and current action releases; pin actions by commit for stronger supply-chain control.
- Add the pre-commit derivation to flake checks and run it explicitly in CI.
- Define the intended branch policy consistently for GitHub Actions and AppVeyor.
- Refresh Windows compiler coverage and add at least one Clang/GCC sanitizer job.
- Remove the unrelated `nix_path` channel setting or align it with the flake to reduce confusion.

### 11. Medium: public API contracts allow avoidable crashes and overreads

Several public APIs are unsafe or incomplete even with otherwise valid inputs:

- The length-taking delimiter setters ignore `len` and call null-terminated assignment at [`tokenizer.cpp:11`](../../src/tokenizer.cpp#L11). A non-null-terminated buffer can be overread.
- Empty delimiters are accepted by setters and later cause `std::string::at(0)` to throw during tokenization.
- Re-tokenizing into an existing root does not clear its old children and overwrites `root->data` without releasing it at [`tokenizer.cpp:83`](../../src/tokenizer.cpp#L83).
- `Renderer::render` checks node and data, but dereferences `_output` without checking it at [`renderer.cpp:51`](../../src/renderer.cpp#L51).
- Installed `lambda.hpp` fails standalone compilation because it does not include `<string>`; `stack.hpp` fails because it does not include the exception declaration it uses.
- Exception paths in lambda rendering can leave swapped output/state pointers unrestored because restoration is not guarded by RAII.

Recommendation:

- Prefer references, values, `std::string_view`/span-like inputs, and explicit non-null contracts over nullable raw pointers.
- Validate delimiter invariants when they are set.
- Make public headers self-contained and add one-header-at-a-time compile tests.
- Make tokenizer/renderer operations either reusable by construction or explicitly single-use.
- Use small scope guards/automatic objects for temporary renderer state.

### 12. Low: user and generated documentation is stale

The README clone command uses the `git://` protocol at [`README.md:17`](../../README.md#L17), which GitHub permanently disabled in 2022 ([GitHub notice](https://github.blog/changelog/2022-03-15-removed-unencrypted-git-protocol-and-certain-ssh-keys/)). The README does not document CMake, library API usage, CLI behavior, dependency versions, or how to run tests. The Doxygen project version is `0.0.1`, while both build systems declare `0.5.0`, and its input includes a nonexistent `../include` directory.

Recommendation:

- Replace the clone URL with HTTPS and document submodule initialization.
- Add minimal library and CLI examples that are exercised as tests.
- Document both supported build paths and their install/consumer workflows.
- Generate version metadata from one source and update Doxygen inputs.

## Verification performed

| Check | Result |
| --- | --- |
| `nix flake check -L` | Flake evaluates successfully; warns about custom `githubActions` output and omits incompatible `aarch64-linux`. |
| `nix build -L --rebuild .#libmustache .#libmustache-cmake` | Both variants build and their configured tests pass; Autotools runs 3 test programs, CMake runs 2. |
| Exact-output Mustache spec comparison | Failed: 104 passed, 33 failed, 137 recorded. |
| `pre-commit run --all-files` in the dev shell | Failed: `actionlint` and `shellcheck`; `alejandra` passed. |
| Default installed `mustachec -v` | Failed to load `libmustache.so.5`. |
| `nix run .# -- -v` | Failed because Nix looked for nonexistent `bin/libmustache`. |
| CMake-installed `mustachec -v` | Passed; the CMake CLI links the static library. |
| Advertised `mustachec -e ...` | Failed as an unknown option. |
| CLI without a data file | Aborted on uncaught `mustache::Exception("Empty data")`. |
| Mismatched/orphan section probes | Mismatched names accepted; orphan close with following text aborted. |
| JSON array structure probe | Reported `length=2 size=0`. |
| Two-byte serialized-node probe with assertions | Aborted on out-of-range vector indexing. |
| `Node` copy probe with AddressSanitizer | Crashed during destruction. |
| One-header-at-a-time compile check | `lambda.hpp` and `stack.hpp` failed; other installed headers passed. |
| JSON/YAML false-value CLI comparison | JSON rendered the false branch; YAML rendered the true branch. |
| Warning-heavy syntax build | Completed with numerous conversion, reorder, shadow, unused-parameter, and incomplete-switch warnings. |

## Recommended remediation order

### Phase 1: establish memory safety and honest tests

1. Fix vector construction and replace recursive ownership with RAII.
2. Delete unsafe copy operations immediately if deep-copy semantics cannot be implemented in the same change.
3. Harden or temporarily disable public deserialization until it is bounds checked.
4. Switch the spec harness to exact comparison and preserve the 33 failures as visible expected failures only while standalone behavior is implemented.
5. Add ASan/UBSan, `_GLIBCXX_ASSERTIONS`, and malformed-input regression tests.

### Phase 2: restore correct product behavior

1. Validate matching section names and orphan closes.
2. Align YAML and JSON data semantics.
3. Decide and implement the CLI contract, including predictable errors.
4. Repair the default Nix executable and add post-install execution checks.

### Phase 3: converge build, packaging, and CI

1. Give CMake and Autotools identical tests, ABI metadata, features, and consumer guarantees.
2. Add installed-package consumer tests for pkg-config, CMake, and Nix.
3. Refresh CI runners/actions and make repository-wide hooks mandatory checks.
4. Test supported compilers/platforms explicitly and remove configurations that are no longer supported.

### Phase 4: simplify the API and documentation

1. Encapsulate `Data`/`Node` internals and replace pointer-heavy APIs.
2. Make headers independently compilable and clarify thread-safety/reuse/ownership contracts.
3. Update installation, API, CLI, testing, and generated documentation from a single version source.

## Suggested release gate

Before the next release, require all of the following:

- zero sanitizer/assertion failures for parser, renderer, copy/move, and malformed-input tests;
- exact Mustache spec results with explicit, counted skips only for intentionally unsupported optional extensions;
- both installed build variants consumable from a separate project;
- both installed CLIs able to execute a render smoke test;
- clean repository-wide lint/format hooks; and
- current Linux and Windows CI jobs passing on every supported target branch.
