# xxHash snapshot

This directory contains the private xxHash header used when the experimental
archived-template implementation is enabled without a system xxHash package.
Nothing in this directory is installed or included by libmustache's public
headers.

- Upstream release and revision: `v0.8.3`
- Release archive:
  `https://github.com/Cyan4973/xxHash/archive/v0.8.3.tar.gz`
- Nix recursive source hash:
  `sha256-h6kohM+NxvQ89R9NEXZcYBG2wPOuB4mcyPfofKrx9wQ=`
- Vendored `xxhash.h` SHA-256:
  `17973c0dc49d9854ca26caa191f0e12f7a424b68858d9a78de3860d959d85e4b`
- License: BSD 2-Clause; see `LICENSE`. The header also embeds the license.

The bundled configuration defines `XXH_IMPLEMENTATION`,
`XXH_STATIC_LINKING_ONLY`, and a libmustache-specific `XXH_NAMESPACE` before
including this header. This keeps one implementation behind the private
out-of-line Cista XXH3 adapter, avoids symbol collisions, and does not require a
separately linked xxHash library. `XXH_INLINE_ALL` is deliberately not used
because it makes Cista 0.16's runtime type hash unstable in optimized GCC
builds. The out-of-line boundary also avoids a GCC 15 miscompilation of Cista's
address-of-parameter hash helper while preserving identical archive bytes.

## Updating

Treat an xxHash update as an archive-format dependency change. Review the
upstream diff and license, copy `xxhash.h` and `LICENSE` from a clean checkout
of the selected release, update every pin and checksum above plus
`expected_xxhash_sha256` in `scripts/check-version-consistency.sh`, update the
libmustache archive format generation when compatibility changes, update the
golden fixture deliberately if the selected bytes change, and run the complete
bundled/system Cista and xxHash build matrix plus the archive
determinism, compatibility, corruption, and sanitizer tests. Complete archive
fuzzing before treating the updated format as stable.

```sh
cp /path/to/xxHash/xxhash.h /path/to/libmustache/vendor/xxhash/xxhash.h
cp /path/to/xxHash/LICENSE /path/to/libmustache/vendor/xxhash/LICENSE
sha256sum /path/to/libmustache/vendor/xxhash/xxhash.h
```

Do not replace the pin with a moving branch or add configure-time downloads.
