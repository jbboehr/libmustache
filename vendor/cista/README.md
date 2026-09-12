# Cista snapshot

This directory contains the private, generated single-header distribution of
[Cista](https://github.com/felixguendling/cista). It is used only when the
experimental archived-template implementation is enabled. Nothing in this
directory is installed or included by libmustache's public headers.

- Upstream release: `v0.16`
- Upstream commit: `015b94e66c4c0d7c1ef29742383a024c84fbbba1`
- Release archive:
  `https://github.com/felixguendling/cista/archive/refs/tags/v0.16.tar.gz`
- Nix recursive source hash:
  `sha256-Q7IDQckFa/iMZ/f3Bim/yWyKCGqsNxJJ5C9PTToFZYI=`
- Generated `cista.h` SHA-256:
  `88aa1b41df4b974f4937f7c8ea842cd085922c43b3bce8f3a1bf328518d6adae`
- License: MIT; see `LICENSE`. The generated header also embeds the license.

The snapshot is generated with Cista's upstream `uniter` tool and is not edited
by hand. Libmustache pre-includes its small `src/archive/xxh3/xxh3.h`
compatibility header, which suppresses the older embedded copy and routes
Cista's hash calls through one private out-of-line adapter. That adapter uses
the selected modern XXH3 API from the separately versioned bundled or system
xxHash dependency. Cista's parameter-pack hashing exposes a strict-aliasing
defect in unpatched xxHash 0.8.3 during optimized GCC builds, including GCC 16
LTO ([issue #29](https://github.com/jbboehr/libmustache/issues/29)). The bundled
xxHash header carries the upstream fix; see [its provenance](../xxhash/README.md).
For system xxHash builds, the adapter uses GCC's `noipa` attribute where
available to preserve its optimization boundary under LTO and in configuration
probes. This compatibility workaround is unnecessary for the patched bundled
header.

## Updating

Treat a Cista update as an archive-format change, even when the public
libmustache schema number has not changed. Review the upstream diff and
licenses, regenerate the header, update every pin and checksum above plus
`expected_cista_sha256` in `scripts/check-version-consistency.sh`, bump the
libmustache archive format generation when compatibility changes, update the
golden fixture deliberately, and run both
the vendored and system-Cista build matrices plus the archive determinism,
compatibility, corruption, and sanitizer tests. Complete archive fuzzing before
treating the updated format as stable.

From a clean checkout of the selected upstream tag:

```sh
c++ -std=c++17 -O2 tools/uniter/uniter.cc -o /tmp/cista-uniter
cp /path/to/libmustache/src/archive/xxh3/xxh3.h include/xxh3.h
/tmp/cista-uniter \
  LICENSE \
  include \
  include/cista/serialization.h \
  include/cista/reflection/comparable.h \
  include/cista/reflection/printable.h \
  include/cista/reflection/member_index.h \
  > /path/to/libmustache/vendor/cista/cista.h
perl -pi -e 's/\r$//' /path/to/libmustache/vendor/cista/cista.h
cp LICENSE /path/to/libmustache/vendor/cista/LICENSE
sha256sum /path/to/libmustache/vendor/cista/cista.h
```

Do not replace the pin with a moving branch or add configure-time downloads.
