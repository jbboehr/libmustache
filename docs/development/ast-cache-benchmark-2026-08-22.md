# Cached AST decoding versus source reparsing

Date: 2026-08-22; measurements refreshed after review on 2026-08-23 and with
a Cista feasibility experiment on 2026-08-24

## Decision

Cache template source across PHP requests today, and do not use the legacy AST
as the basis of a new format. A checked Cista direct-view prototype now makes a
new persistent compiled format a live candidate, but it is not yet a production
decision.

Validated AST decoding is intrinsically faster than parsing, and it wins for
flat templates. It does not provide a consistent end-to-end win for the
primary PHP consumer: with nested partial graphs, php-mustache's required
ownership clones make cached AST hits 9% to 17% slower at the medium and
large sizes across warm and fresh-process cases. The legacy AST is also 2.16
to 2.38 times the source size. Serializing and storing that AST in APCu is 16
to 19 times slower than serializing and storing source at those sizes.

On 2026-08-24, an opt-in Cista experiment rendered a checked immutable archive
directly, without rebuilding `Node` trees or cloning partials. It was 73% to
81% faster than source compilation plus rendering at median and p95 for the
medium and large workloads. This clears the native portion of the predeclared
20% threshold and removes the old partial-ownership penalty. It does not yet
clear the PHP gate: PHP serialization and APCu fetch/store have not been
measured, the payload is 4.35 to 4.80 times source, and the prototype does not
support lambdas or inline-partial ownership. It also used
`WITH_STATIC_VERSION` without Cista's `DEEP_CHECK` or `WITH_INTEGRITY`, so the
published timings are not the final production-security configuration.

Request-local compiled reuse remains useful when one request renders the same
view repeatedly, but that is not the typical one-view/one-render PHP workload
and is not the primary cross-request optimization.

The compatibility window is now explicit: libmustache keeps checked legacy AST
reads throughout the 0.6 release series, and php-mustache keeps them throughout
its 0.x release series. AST writes should be deprecated when source-cache
guidance or a replacement format ships. Removing the reader requires a separate
incompatible release decision and migration notice. New cache guidance should
prefer source until the Cista design is exercised through php-mustache's actual
one-fetch/one-render APCu boundary. A production format still requires a
canonical compatibility model, full Mustache semantics, fuzzing, and an
explicit security review.

## Predeclared threshold

Before the full runs, a persistent AST remained a candidate only if the PHP
cache-hit path reduced both median and p95 latency by at least 20% for medium
and large flat templates and nested partial graphs. This deliberately requires
an end-to-end win large enough to pay for a second format, migration rules, and
cache invalidation. The C++ construction result is diagnostic and cannot pass
the PHP gate by itself.

The AST failed this threshold. It won warm flat-template medians by 28% to 30%,
but the warm large-flat APCu p95 improved by only 15%. Fresh-process flat cases
cleared 20%, but medium and large partial graphs regressed median and p95
latency in both warm and fresh-process runs.

The later Cista experiment cleared the native latency threshold for every
medium and large shape, including nested partial graphs. It therefore warrants
an end-to-end PHP/APCu experiment; it does not replace that experiment.

## Workload and oracles

The repository benchmark programs are:

- [`benchmarks/ast-cache-vs-source.cpp`](../../benchmarks/ast-cache-vs-source.cpp),
  which compares optimized C++ compilation and validated decoding, records
  `operator new` allocation counts and requested bytes, measures an ideal
  direct-owned render path, and measures resident `CompiledTemplate` reuse; and
- [`benchmarks/ast-cache-vs-source.php`](../../benchmarks/ast-cache-vs-source.php),
  which exercises php-mustache's public source, `MustacheAST`, PHP
  serialization, APCu, request-local reuse, and fresh-process first-hit paths.

When `MUSTACHE_ENABLE_CISTA_BENCHMARK=ON`, the C++ program also builds a
flattened Cista archive and validates and renders directly from its immutable
offset-based view. [`benchmarks/cista-archive.cpp`](../../benchmarks/cista-archive.cpp)
is deliberately benchmark-only and is not linked into libmustache.

Each program generates deterministic small, medium, and large cases at about
1 KiB, 32 KiB, and 256 KiB. Every size has two shapes:

- a flat template containing comments, escaped variables, dotted lookups,
  truthy and inverted sections, and realistic markup; and
- a nested `root -> layout -> card -> badge` partial graph with the same
  constructs and aggregate source size.

Every PHP case renders the same owned data. Before timing, the benchmark
requires source and decoded AST output to match, requires AST bytes to
round-trip exactly, checks PHP serialization round trips, and checks that APCu
returns the exact cached payload. A mismatch aborts the run.

Warm cases use repeated operations in one CLI process. Cold cases execute each
first hit in a fresh PHP CLI process with no timed warmups. Process startup and
fixture loading occur before the child starts its timer; APCu is primed in that
fresh process immediately before its first timed fetch. This isolates the
first-operation allocator, decoder, compiler, and renderer cost, but it is only
a cold-worker proxy: it does not reproduce FPM process management, a shared
FPM APCu segment, cross-host cache latency, or OPcache policy.

Zend cycle collection is disabled during timed samples and enabled between
cases. A separate operation records Zend-tracked peak memory. The C++
allocation counter supplies complementary native allocation totals. Neither
measurement is a native peak-RSS result.

## Environment and method

- Legacy C++ and PHP runs: libmustache
  `56643b061aadb72a6d8b9675709884109ffa5f7f`
- Cista feasibility runs: libmustache base
  `8065003173138a60c108e4878cd58a9f03a5d0d0` plus the Cista experiment in this
  change
- php-mustache `3b4aaf2d4a121666244f36142a38123ed1625a52`
- PHP 8.3.33 NTS and php-mustache 0.9.3
- APCu 5.1.28 with `apc.enable_cli=1`
- GCC 15.2.0; C++ benchmark at `-O3 -DNDEBUG`; PHP extension at `-O2`
- Cista 0.16 from nixpkgs for the 2026-08-24 native follow-up
- AMD Ryzen 9 9950X3D; Linux 7.1.5; benchmark process pinned to CPU 0

The PHP program took 101 individual-operation samples after ten warmups in each
of three parent processes. Each parent also launched 31 fresh PHP processes per
workload and cold case. The tables report the median of the three run medians
and the median of their p95 values. The C++ benchmark used 101 samples after ten
warmups and was also repeated three times. Work is bounded by the library and
extension defaults. CPU boost and frequency were not fixed, so repeated-run
aggregation and the 20% threshold are also safeguards against local timing
noise.

The C++ benchmark can be reproduced from the repository root with:

```sh
cmake -S . -B build/benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DMUSTACHE_BUILD_CLI=OFF \
  -DMUSTACHE_ENABLE_TESTS=OFF \
  -DMUSTACHE_ENABLE_ASSERTIONS=OFF
cmake --build build/benchmark --target mustache_ast_cache_vs_source -j4
taskset -c 0 build/benchmark/benchmarks/mustache_ast_cache_vs_source
```

Run the Cista variant from the Nix development shell with:

```sh
nix develop --command cmake -S . -B build/benchmark-cista \
  -DCMAKE_BUILD_TYPE=Release \
  -DMUSTACHE_BUILD_CLI=OFF \
  -DMUSTACHE_ENABLE_TESTS=ON \
  -DMUSTACHE_ENABLE_ASSERTIONS=OFF \
  -DMUSTACHE_ENABLE_CISTA_BENCHMARK=ON
nix develop --command cmake --build build/benchmark-cista -j4
nix develop --command ctest --test-dir build/benchmark-cista --output-on-failure
nix develop --command taskset -c 0 \
  build/benchmark-cista/benchmarks/mustache_ast_cache_vs_source
```

For PHP, build php-mustache `develop` against this libmustache checkout, load
the resulting `mustache.so` plus APCu, and run:

```sh
BENCH_MUSTACHE_EXTENSION=/path/to/mustache.so \
BENCH_APCU_EXTENSION=/path/to/apcu.so \
BENCH_JSON=build/ast-cache-vs-source.json \
  taskset -c 0 php -n \
  -d extension=/path/to/apcu.so \
  -d apc.enable_cli=1 \
  -d extension=/path/to/mustache.so \
  /path/to/libmustache/benchmarks/ast-cache-vs-source.php
```

`BENCH_SAMPLES`, `BENCH_WARMUPS`, and `BENCH_COLD_SAMPLES` override the
defaults. Set `BENCH_COLD_SAMPLES=0` to omit child processes.
`BENCH_MUSTACHE_EXTENSION` is required for cold samples, and
`BENCH_APCU_EXTENSION` is additionally required when APCu is loaded. If APCu
is not loaded, the program still runs in-memory, PHP-serialization, and
non-APCu cold cases.

The raw machine-readable results are retained beside the programs:

- [PHP run 1](../../benchmarks/results/ast-cache-vs-source-2026-08-23-php-run1.json),
  [run 2](../../benchmarks/results/ast-cache-vs-source-2026-08-23-php-run2.json),
  and [run 3](../../benchmarks/results/ast-cache-vs-source-2026-08-23-php-run3.json);
- [C++ run 1](../../benchmarks/results/ast-cache-vs-source-2026-08-23-cpp-run1.csv),
  [run 2](../../benchmarks/results/ast-cache-vs-source-2026-08-23-cpp-run2.csv),
  and [run 3](../../benchmarks/results/ast-cache-vs-source-2026-08-23-cpp-run3.csv); and
- [Cista run 1](../../benchmarks/results/ast-cache-vs-source-2026-08-24-cista-run1.csv),
  [run 2](../../benchmarks/results/ast-cache-vs-source-2026-08-24-cista-run2.csv),
  and [run 3](../../benchmarks/results/ast-cache-vs-source-2026-08-24-cista-run3.csv).

## Payload size

| Workload | Source bytes | AST bytes | AST/source | Cista bytes | Cista/source | PHP source payload | PHP AST payload |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Small flat | 1,211 | 2,649 | 2.19x | 5,482 | 4.53x | 1,306 | 2,743 |
| Small graph | 1,181 | 2,812 | 2.38x | 6,041 | 5.12x | 1,476 | 3,107 |
| Medium flat | 33,047 | 71,277 | 2.16x | 144,082 | 4.36x | 33,143 | 71,372 |
| Medium graph | 33,031 | 76,508 | 2.32x | 158,529 | 4.80x | 33,328 | 76,804 |
| Large flat | 262,342 | 565,562 | 2.16x | 1,142,332 | 4.35x | 262,439 | 565,658 |
| Large graph | 262,156 | 606,668 | 2.31x | 1,255,509 | 4.79x | 262,454 | 606,965 |

## Intrinsic construction cost

For a like-for-like public `Node` construction comparison, the PHP wrapper's
`Mustache::parse()` path confirms that decoding the current checked format is
34% to 41% faster than tokenizing source. Actual source cache hits below use
the production `CompiledTemplate` render path. Times are median/p95
microseconds.

| Workload | Parse source | Decode AST | Median change |
| --- | ---: | ---: | ---: |
| Small flat | 7.77 / 12.52 | 4.58 / 4.69 | -41.1% |
| Small graph | 8.52 / 8.77 | 5.33 / 5.40 | -37.4% |
| Medium flat | 260.54 / 312.16 | 163.22 / 194.20 | -37.4% |
| Medium graph | 214.25 / 283.30 | 138.51 / 192.47 | -35.4% |
| Large flat | 2,215.84 / 2,407.80 | 1,351.15 / 1,570.24 | -39.0% |
| Large graph | 1,705.76 / 1,943.36 | 1,130.00 / 1,257.85 | -33.8% |

The optimized C++ benchmark produced the same direction. On the medium graph,
compilation requested 4,720 allocations and 844,514 cumulative bytes versus
4,484 allocations and 820,710 bytes for decoding. On the large graph, the
counts were 37,153 and 6,688,510 bytes versus 35,504 and 6,504,420 bytes. The
speedup therefore comes with only a modest native-allocation reduction.

The end-to-end C++ paths now construct a fresh renderer for both source and AST
operations. This removes the warm stateful-renderer bias from the original
measurement. Decoding directly into one owned `Node` tree plus
`Node::Partials` remains 27% to 39% faster than compile plus render.

| Workload | Compile + render | Decode + render | Resident compiled render |
| --- | ---: | ---: | ---: |
| Small flat | 8.90 / 9.68 | 6.03 / 6.74 | 1.44 / 1.44 |
| Small graph | 10.33 / 12.55 | 7.50 / 7.66 | 2.18 / 2.19 |
| Medium flat | 265.80 / 303.51 | 163.01 / 199.31 | 37.35 / 48.21 |
| Medium graph | 301.90 / 360.00 | 203.80 / 239.54 | 59.08 / 80.03 |
| Large flat | 1,944.15 / 2,081.47 | 1,341.28 / 1,458.03 | 308.68 / 361.25 |
| Large graph | 2,246.76 / 2,401.61 | 1,637.14 / 1,792.74 | 492.57 / 576.69 |

The current PHP AST path cannot realize the direct-owned result because
independently owned `MustacheAST` partials must be cloned into the renderer.

### Cista direct-view feasibility result

The Cista prototype uses flat POD node records, integer child and sibling
indices, and one shared byte arena. A checked read verifies Cista's static type
version and bounds, then applies libmustache-specific framing, graph-shape,
node-type, flag, string-range, nesting, node-count, and dotted-name limits. The
renderer walks that immutable archive in place. Times below are median/p95
microseconds aggregated across the three 2026-08-24 runs.

| Workload | Compile + render source | Validate + render Cista | Median change | p95 change |
| --- | ---: | ---: | ---: | ---: |
| Medium flat | 260.61 / 280.25 | 52.40 / 54.48 | -79.9% | -80.6% |
| Medium graph | 295.32 / 318.98 | 73.90 / 79.56 | -75.0% | -75.1% |
| Large flat | 1,911.67 / 1,989.10 | 421.26 / 450.21 | -78.0% | -77.4% |
| Large graph | 2,190.36 / 2,323.20 | 584.63 / 614.81 | -73.3% | -73.5% |

The direct read requested 10 to 13 allocations for the medium and large flat
cases, versus 4,395 and 34,651 for source compilation plus rendering. Partial
graphs requested 218 and 1,631 allocations because rendering still allocates
for partial indentation and output, versus 4,945 and 38,791 for source.

The writer is more expensive. Compilation plus Cista serialization added 39%
to 57% over source compilation, before any PHP or APCu store cost:

| Workload | Compile source | Compile + serialize Cista | Median change |
| --- | ---: | ---: | ---: |
| Medium flat | 223.83 | 310.11 | +38.5% |
| Medium graph | 234.98 | 331.53 | +41.1% |
| Large flat | 1,593.81 | 2,494.84 | +56.5% |
| Large graph | 1,697.85 | 2,659.25 | +56.6% |

This is a feasibility result, not a proposed wire format. The prototype is
native-layout dependent, has no published compatibility contract or checksum,
rejects lambdas and inline-partial ownership, and has not crossed a Zend string
or APCu boundary. Its direct-view performance is strong enough to justify that
next experiment; its size and writer cost are reasons not to adopt it yet. The
nixpkgs Cista 0.16 package also has inconsistent CMake version metadata and an
unpropagated optional `fmt` definition, both worked around by the opt-in build.

The recorded measurements used `WITH_STATIC_VERSION`, Cista's ordinary bounds
checks, and a libmustache-specific semantic validator. They did not enable
`DEEP_CHECK` or `WITH_INTEGRITY`. Cista's
[serialization reference](https://github.com/felixguendling/cista/wiki/Serialization-Reference)
requires deep checking for untrusted input. The experiment now uses
`WITH_STATIC_VERSION | WITH_INTEGRITY | DEEP_CHECK`, but the retained CSVs
predate that change; rerun and re-evaluate the benchmark before treating its
latency as a production result. The integrity checksum is useful corruption
detection, not authentication; caches exposed to hostile writers still need a
separate trust boundary.

### Dependency and API direction

If the feasibility work continues, use this integration policy:

- Keep archived templates optional and default off initially, under
  `MUSTACHE_ENABLE_ARCHIVED_TEMPLATES`.
- Vendor a reviewed and pinned Cista snapshot as a private implementation
  dependency, together with its
  [MIT license](https://github.com/felixguendling/cista/blob/master/LICENSE),
  provenance, and update instructions. Cista officially supports a
  [single-header integration](https://github.com/felixguendling/cista/wiki/Installation-and-Usage),
  so this does not require a submodule or configure-time network access.
- Optionally provide a default-off `MUSTACHE_USE_SYSTEM_CISTA` override for
  packagers. Test both paths and fail configuration clearly when the requested
  system version or features are unavailable.
- Do not expose `cista::*` in installed headers. Expose a libmustache-owned
  `ArchivedTemplateView` alongside the ordinary owned `Node`/`CompiledTemplate`
  path, and route both through a shared renderer algorithm using internal view
  adapters. Rendering a checked archive must not require rebuilding a `Node`
  tree first.
- Give the archive an explicit libmustache schema/version header and include
  the pinned Cista snapshot, pointer width, endianness, and other relevant
  compiler/platform assumptions in compatibility checks and PHP cache keys.
  Treat any Cista update as a deliberate format event backed by golden fixtures
  and rejection tests for incompatible bytes.
- Require full lambda and inline-partial semantics, alignment and backing-store
  lifetime tests, corruption fixtures, archive-validation/render fuzzing, and
  the secured native rerun before exposing the experiment to php-mustache.

## PHP cache-hit result

These are complete cache-hit-and-render times in median/p95 microseconds.
`PHP payload` performs `unserialize()` on an in-memory serialized cache value.
`APCu` fetches that value first.

| Workload | PHP source | PHP AST | AST median change | APCu source | APCu AST | AST median change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Small flat | 10.68 / 16.28 | 7.50 / 7.64 | -29.8% | 10.71 / 10.92 | 7.68 / 12.93 | -28.3% |
| Small graph | 12.56 / 14.41 | 12.56 / 12.84 | 0.0% | 12.72 / 14.46 | 12.69 / 21.61 | -0.2% |
| Medium flat | 251.87 / 294.23 | 179.56 / 216.00 | -28.7% | 253.41 / 297.76 | 179.89 / 222.94 | -29.0% |
| Medium graph | 297.14 / 346.76 | 328.70 / 404.84 | +10.6% | 302.36 / 347.81 | 328.18 / 395.18 | +8.5% |
| Large flat | 2,070.72 / 2,242.56 | 1,474.85 / 1,644.58 | -28.8% | 2,059.06 / 2,274.46 | 1,482.35 / 1,944.15 | -28.0% |
| Large graph | 2,371.52 / 2,597.81 | 2,687.86 / 2,925.55 | +13.3% | 2,396.99 / 2,585.10 | 2,749.68 / 2,967.93 | +14.7% |

The Zend-tracked peak delta also favored source. For the large flat APCu hit it
was 545,576 bytes for source and 1,155,928 bytes for AST. For the large graph it
was 543,456 bytes and 1,236,120 bytes, respectively. These figures exclude the
native `Node` heap, so they are directional rather than total process memory.

## Fresh-process cache-hit result

Each sample below is the first timed cache hit in a newly started PHP CLI
process. Times are median/p95 microseconds. As described above, this is a
cold-worker proxy rather than a direct FPM measurement.

| Workload | PHP source | PHP AST | AST median change | APCu source | APCu AST | AST median change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Small flat | 30.02 / 39.91 | 22.29 / 26.56 | -25.7% | 30.84 / 33.97 | 23.10 / 24.54 | -25.1% |
| Small graph | 37.11 / 50.36 | 38.09 / 50.80 | +2.6% | 37.55 / 45.95 | 38.93 / 49.64 | +3.7% |
| Medium flat | 364.54 / 408.92 | 246.85 / 293.66 | -32.3% | 362.33 / 452.77 | 255.00 / 308.50 | -29.6% |
| Medium graph | 426.53 / 530.00 | 478.52 / 513.72 | +12.2% | 422.34 / 518.50 | 480.18 / 575.04 | +13.7% |
| Large flat | 2,904.71 / 3,110.82 | 1,905.07 / 2,076.67 | -34.4% | 2,925.95 / 3,154.77 | 1,967.11 / 2,126.05 | -32.8% |
| Large graph | 3,337.21 / 3,638.11 | 3,797.79 / 4,052.36 | +13.8% | 3,338.82 / 3,564.51 | 3,918.64 / 4,188.10 | +17.4% |

## Serialization and APCu store cost

The APCu store cases time `serialize($graph)` and `apcu_store()` together.
They no longer copy a string serialized before the timer. The separate
serialization rows make the boundary auditable. Times are median/p95
microseconds.

| Workload | Serialize source | Serialize AST | Store source | Store AST | Store ratio |
| --- | ---: | ---: | ---: | ---: | ---: |
| Small flat | 0.12 / 0.15 | 5.04 / 6.19 | 0.56 / 0.58 | 5.89 / 5.95 | 10.5x |
| Small graph | 0.19 / 0.21 | 6.54 / 6.60 | 0.68 / 0.69 | 7.45 / 7.52 | 11.0x |
| Medium flat | 0.41 / 0.48 | 122.15 / 158.20 | 8.76 / 8.85 | 140.06 / 171.73 | 16.0x |
| Medium graph | 0.50 / 0.57 | 138.86 / 171.01 | 8.95 / 9.02 | 158.73 / 193.06 | 17.7x |
| Large flat | 2.20 / 2.81 | 1,071.80 / 1,165.48 | 68.21 / 78.10 | 1,206.22 / 1,326.66 | 17.7x |
| Large graph | 2.26 / 2.38 | 1,165.79 / 1,303.17 | 68.38 / 78.90 | 1,302.49 / 1,426.40 | 19.0x |

## Request-local reuse

`MustacheTemplate` stores source only, so both source rows below compile during
rendering. `Compile source` also constructs the PHP wrapper objects inside the
timed operation; `reuse source object` keeps those wrappers resident. `Reuse
AST` keeps parsed AST objects resident, although partial graphs still pay the
extension's required clone. Times are median/p95 microseconds.

| Workload | Compile source | Reuse source object | Reuse AST |
| --- | ---: | ---: | ---: |
| Small flat | 10.46 / 10.72 | 10.30 / 11.61 | 2.58 / 2.60 |
| Small graph | 12.38 / 20.59 | 11.91 / 12.08 | 6.32 / 6.37 |
| Medium flat | 253.25 / 323.19 | 251.52 / 298.68 | 52.33 / 71.36 |
| Medium graph | 295.15 / 345.14 | 293.84 / 358.69 | 174.89 / 212.97 |
| Large flat | 2,689.21 / 2,984.38 | 2,680.60 / 2,887.87 | 430.54 / 497.60 |
| Large graph | 2,380.29 / 2,590.36 | 2,368.05 / 2,557.09 | 1,397.38 / 1,529.25 |

## Interpretation and follow-up

The legacy format is not a poor decoder: it provides a real construction win.
The current cross-request product decision remains source because its win is
shape-dependent, the warm large-flat tail misses the threshold, nested partials
reverse the result in both warm and fresh-process measurements, serialization
plus store strongly favors source, and every legacy AST consumes more than
twice the cache space.

The Cista result materially changes the next question. Direct validated archive
views eliminate the graph reconstruction and ownership-clone costs, and their
native read path comfortably clears the threshold. The remaining decision must
be made at the real PHP boundary, where the larger payload, Zend string handling,
alignment, PHP serialization, APCu copying, and one-fetch/one-render lifecycle
can erase part of that gain.

Recommended follow-up:

1. Document cached source as the default persistent PHP cache value.
2. Refactor libmustache so owned nodes and an archived-template view share one
   rendering algorithm without exposing Cista types in public headers.
3. Keep archived-template support optional and default off. Vendor a reviewed,
   pinned Cista snapshot and license by default; consider a separately tested,
   default-off system-package override for packagers.
4. Add full lambda and inline-partial semantics, explicit
   compiler/architecture/schema compatibility, `WITH_INTEGRITY` and
   `DEEP_CHECK`, golden corruption and compatibility fixtures, alignment and
   lifetime tests, and fuzzing of validation plus rendering. Rerun the native
   benchmark in that configuration.
5. Only after the secured native path passes, prototype the libmustache-owned
   archived view behind an experimental php-mustache API and benchmark one APCu
   fetch plus one render against cached source. Include warm and fresh-process
   cases, payload-copy or aligned-copy cost, peak memory, and writer cost.
6. Treat request-local compiled reuse as a secondary optimization only for
   applications that render the same view repeatedly in one request.
7. Keep accepting checked legacy AST payloads through libmustache 0.6.x and
   php-mustache 0.x. Deprecate writes when source-cache guidance or a replacement
   format ships; require a separately announced incompatible release to remove
   reads.
8. Adopt a canonical persistent format only if the complete PHP/APCu path still
   clears the predeclared threshold for every medium and large shape.
