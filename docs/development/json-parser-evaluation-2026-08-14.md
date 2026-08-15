# JSON parser evaluation

Date: 2026-08-14

## Decision

Use nlohmann/json 3.10.5 or newer through its SAX interface. Keep it a private,
header-only build dependency and construct the owned `Data` tree directly as
events arrive. Do not expose nlohmann/json types in public headers or installed
package metadata.

This is not a claim that nlohmann/json is the fastest parser considered. It is
the best fit for libmustache's current workload and packaging boundary:

- it preserves the original token passed to the floating-point callback;
- a direct builder enforces libmustache's aggregate budgets without retaining
  a parser DOM;
- Ubuntu 22.04 packages a sufficiently recent version with CMake and
  pkg-config metadata;
- vcpkg, Homebrew, and the pinned nixpkgs set provide it;
- it requires no parser library when an installed static libmustache is linked;
  and
- its C++11 baseline is compatible with libmustache's C++17 baseline.

Revisit simdjson or yyjson only if an end-to-end supported workload shows
libmustache's JSON parsing to be material. The current PHP binding converts
zvals directly into the library's data representation and does not normally
invoke this JSON parser, so raw parser throughput is primarily relevant to the
CLI and C++ callers. The synthetic results below demonstrate a possible speedup
if that workload changes.

## Candidates

| Candidate | Result | Main reason |
| --- | --- | --- |
| json-c 0.18 | Replace | Slowest measured candidate, two-pass adapter, compiled dependency leaks to static consumers |
| nlohmann/json 3.12.0 | Select | Header-only SAX API, raw float token, mature cross-platform packaging |
| Boost.JSON 1.89.0 | Do not select now | Excellent incremental handler, but no Boost.JSON package on Ubuntu 22.04 and normally a compiled link dependency |
| simdjson 4.6.4 | Keep as performance fallback | Fastest C++ candidate, but padding/view lifetimes and a compiled dependency add complexity to a full-copy workload |
| RapidJSON snapshot | Do not select | Header-only and fast, but the latest stable release is 1.1.0 and current distributions carry divergent snapshots |
| yyjson 0.12.0 | Keep as performance fallback | Fastest measured candidate and supports raw numbers, but replaces one C link dependency with another |
| jsoncons 1.7.0 | Do not select | Capable but broader, slower, and more expensive to compile than needed |
| Glaze 7.7.1 | Incompatible | Current Glaze requires C++23 and newer compilers than libmustache supports |

Boost.JSON's `basic_parser` is technically the strongest direct-builder API in
the group: it reports decoded string/key pieces and original numeric spellings
to the handler. Its Ubuntu 22.04 packaging gap is decisive for the current CI
and downstream support policy. Embedding Boost.JSON's implementation solely to
avoid that gap would create a project-specific update path that is not
justified by the present workload.

simdjson was explicitly measured rather than dismissed. Its DOM parser was
about ten times faster than nlohmann/json on the large synthetic document, and
its On-Demand API can expose raw tokens. Libmustache cannot retain those views,
however: it must traverse the complete input, apply its own budgets, preserve
selected spellings, and produce owned `Data`. That reduces the benefit while
retaining padding, parser-lifetime, CPU-dispatch, and static-link packaging
costs.

## Synthetic benchmark

The spike used GCC 15.2 with `-O3 -DNDEBUG -std=c++17` on an AMD Ryzen 9
9950X3D development machine. Each candidate parsed the same generated JSON
document into its DOM and then walked every object key and value. The 194-,
3,459-, and 68,909-byte cases ran 200,000, 10,000, and 300 iterations per
sample, respectively, after ten warm-up parses. simdjson used its DOM interface
and yyjson used its ordinary immutable document. Values are medians of three
runs.

This is a parser-selection microbenchmark, not an end-to-end libmustache or PHP
benchmark. It intentionally excludes construction of `Data`, allocator counts,
and PHP-to-C++ conversion. Dynamic and static linkage also make executable size
only a rough comparison. Every operation constructed a fresh parser/document
and, for simdjson, a fresh padded input; parser and buffer reuse could improve
simdjson's result further.

| Candidate | 194 bytes | 3,459 bytes | 68,909 bytes | Compile time | Compiler peak RSS | Binary |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| json-c | 101 MiB/s | 77 MiB/s | 80 MiB/s | 0.34 s | 96 MiB | 28 KiB |
| nlohmann/json | 159 MiB/s | 134 MiB/s | 141 MiB/s | 2.03 s | 289 MiB | 127 KiB |
| Boost.JSON | 346 MiB/s | 378 MiB/s | 301 MiB/s | 0.69 s | 215 MiB | 47 KiB |
| simdjson | 928 MiB/s | 1,365 MiB/s | 1,438 MiB/s | 2.12 s | 279 MiB | 110 KiB |
| RapidJSON | 578 MiB/s | 608 MiB/s | 586 MiB/s | 0.93 s | 146 MiB | 65 KiB |
| yyjson | 1,836 MiB/s | 1,734 MiB/s | 1,799 MiB/s | 0.31 s | 109 MiB | 279 KiB |
| jsoncons | 155 MiB/s | 160 MiB/s | 169 MiB/s | 2.98 s | 350 MiB | 163 KiB |

The smallest case is the most representative of command-line inputs and of a
future PHP path if it ever parses JSON instead of converting zvals directly.
There the raw difference between nlohmann/json and simdjson was about 0.95
microseconds per parse. That is worth remembering, but is not enough by itself
to accept a more complex public build and lifetime boundary.

## Contract review

The new adapter keeps or strengthens the public behavior as follows:

| Input property | Decision |
| --- | --- |
| Finite floating-point spelling, including `1.50`, `1e30`, and `-0.0` | Preserve exactly |
| Process locale with a non-period decimal separator | Preserve JSON's period spelling and numeric value |
| Signed 64-bit integer range | Preserve; reject values outside it before conversion |
| Floating-point overflow such as `1e999` | Deliberately reject instead of retaining an infinite internal `double` |
| Duplicate object keys | Preserve json-c's last-value-wins behavior |
| Trailing input | Reject in strict mode |
| Invalid UTF-8 | Deliberately reject |
| UTF-8 byte-order mark | Continue to reject |
| Raw NUL in explicitly sized input | Continue to reject before parsing |
| Escaped NUL in string values | Preserve by length |
| Escaped NUL in object keys | Deliberately support and budget by decoded length |
| Parser failure or budget exhaustion | Publish no partial `Data` value |

The direct SAX builder charges a value before constructing it, charges array
entries before their child nodes, charges object entries and decoded key bytes
when keys arrive, and retains the 256-value implementation depth ceiling. A
single parser token may be temporarily allocated by nlohmann/json, but the
64-MiB default input limit bounds that allocation and no dependency DOM is
retained.

## Packaging boundary

nlohmann/json is required only while compiling the private `json_parser.cpp`
adapter:

- CMake links its imported target only in the build interface;
- the installed shared and static CMake targets do not reference it;
- `mustache.pc` does not list it in `Requires.private`;
- Autotools receives include flags through `nlohmann_json.pc` but no link
  library; and
- Nix keeps it in `buildInputs`, while only libyaml remains propagated.

On GNU and Clang builds, the complete adapter translation unit has hidden
visibility. This covers nlohmann/json 3.10.5, which predates the dependency's
namespace customization support. Newer releases additionally compile in a
libmustache-private ABI namespace. The Ubuntu 22.04 CI jobs audit both the
shared library's dynamic symbols and the static archive's symbol visibility,
so neither ordinary nlohmann/json types nor the SAX builder can become a
public or interposable implementation boundary. On MSVC, the adapter is first
compiled into a private static archive; this keeps CMake's legacy automatic
DLL export scan from publishing the header-only parser's template machinery.

The project still uses `WINDOWS_EXPORT_ALL_SYMBOLS` for the rest of its legacy
Windows ABI. A later ABI-boundary slice should replace that broad mechanism
with explicit public API annotations, using CMake's `GenerateExportHeader` or
an equivalent checked-in export macro. That is a pre-existing library-wide
issue, not a reason to expose the new parser implementation.

The minimum version is 3.10.5 because that is the Ubuntu 22.04 package and it
contains the SAX callbacks used by the adapter. CI must continue to exercise
that distribution rather than validating only the newer pinned Nix version.
nlohmann/json versions through 3.11.3 rewrite the SAX float spelling to the
current C locale's decimal separator before conversion. The adapter validates
the callback token's numeric shape and reconstructs its locale-independent JSON
period before applying string budgets or retaining the spelling.

## Verification

The replacement passed the GCC and Clang CMake suites, the Autotools suite and
`make distcheck`, all Nix flake checks, and the native Visual Studio 2022 shared
and static builds. The CMake tests also passed against Ubuntu 22.04's minimum
supported nlohmann/json 3.10.5 package. Installed CMake and pkg-config consumer
tests confirmed that the header-only parser does not leak into libmustache's
installed link interface, while ELF and PE symbol audits found no exported
nlohmann/json or private builder symbols. The minimum-version CI audit also
checks that any such definitions in the static archive have hidden visibility.

The selected adapter completed a one-hour ASan/UBSan libFuzzer acceptance run
with the committed dictionary and a copied corpus, `-max_len=4096`, and
`-max_total_time=3600`. It executed 52,093,639 inputs in 3,601 seconds at an
average of 14,466 executions per second, reached 1,190 coverage counters and
6,113 features, and peaked at 399 MiB RSS without a crash, sanitizer finding,
or invariant failure. After strengthening the harness to independently count
the returned tree's nodes, container entries, strings, object keys, and
preserved floating-point spellings, a final 10,000-run sanitizer smoke test
also passed. The Nix fuzz check builds and exercises that strengthened oracle.

## Security update handling

The point-in-time review did not identify an upstream nlohmann/json GitHub
security advisory or an NVD record assigning a vulnerability to the parser.
That is not proof that the dependency is vulnerability-free. CVE-2024-34363,
for example, describes Envoy failing to contain an exception while using
nlohmann/json serialization; the affected product is Envoy, not nlohmann/json.
Libmustache does not use JSON serialization and contains parser exceptions at
its adapter boundary.

Because the parser is header-only, its code is embedded in libmustache and a
dependency security update requires rebuilding libmustache. Keep the pinned
Nix and vcpkg versions current, retain Ubuntu 22.04 only as the minimum-version
compatibility job, and rerun the sanitizer-backed parser fuzzer when the
dependency changes. Treat the absence of a published CVE as one input, not as
the security argument for this selection; strict parsing, explicit budgets,
owned output, fuzzing, and exception containment remain the primary controls.

## Sources

- [nlohmann/json SAX interface](https://github.com/nlohmann/json)
- [nlohmann/json releases](https://github.com/nlohmann/json/releases)
- [nlohmann/json strict parse API](https://nlohmann.github.io/json/api/basic_json/parse/)
- [Ubuntu 22.04 nlohmann-json3-dev package](https://packages.ubuntu.com/jammy/nlohmann-json3-dev)
- [NVD CVE-2024-34363 (Envoy)](https://nvd.nist.gov/vuln/detail/CVE-2024-34363)
- [Boost.JSON basic_parser](https://www.boost.org/doc/libs/latest/libs/json/doc/html/ref/basic_parser.html)
- [Ubuntu Boost.JSON package search](https://packages.ubuntu.com/search?keywords=libboost-json-dev)
- [simdjson basics and ownership rules](https://github.com/simdjson/simdjson/blob/master/doc/basics.md)
- [yyjson project and API](https://github.com/ibireme/yyjson)
- [RapidJSON project](https://github.com/Tencent/rapidjson)
- [jsoncons project](https://github.com/danielaparker/jsoncons)
- [Glaze compiler requirements](https://github.com/stephenberry/glaze)
- [CMake `WINDOWS_EXPORT_ALL_SYMBOLS`](https://cmake.org/cmake/help/latest/prop_tgt/WINDOWS_EXPORT_ALL_SYMBOLS.html)
- [CMake `GenerateExportHeader`](https://cmake.org/cmake/help/latest/module/GenerateExportHeader.html)

## Follow-up

1. Keep the JSON behavior corpus and sanitizer-backed `fuzz_data_parser` target.
2. Repeat the extended fuzz acceptance run against the selected adapter as a
   release and dependency-update check.
3. When php-mustache returns to scope, benchmark end-to-end PHP conversion with
   representative request data. Reconsider simdjson or yyjson only if that
   workload actually invokes JSON parsing and attributes material cost to it.
4. Benchmark AST serialization versus reparsing separately; it is unrelated to
   the JSON parser choice.
