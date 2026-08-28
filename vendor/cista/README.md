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
  `e409ba42914b9988d662896cf5e7b855d9a46aab4d0726d85265a3aea2b915d2`
- License: MIT; see `LICENSE`. The generated header also embeds the license.

The snapshot is generated with Cista's upstream `uniter` tool and is not edited
by hand. Before generation, libmustache's small compatibility include is staged
as `include/xxh3.h`, so the single header uses the selected modern XXH3 API from
the separately versioned bundled or system xxHash dependency.

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
cp /path/to/libmustache/benchmarks/cista-xxh3/xxh3.h include/xxh3.h
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
