# xxHash snapshot

This directory contains the private xxHash header used when the experimental
archived-template implementation is enabled without a system xxHash package.
Nothing in this directory is installed or included by libmustache's public
headers.

- Upstream release and revision: `v0.8.3`
- Release archive:
  `https://github.com/Cyan4973/xxHash/archive/v0.8.3.tar.gz`
- Nix recursive source hash of the unpatched upstream release:
  `sha256-h6kohM+NxvQ89R9NEXZcYBG2wPOuB4mcyPfofKrx9wQ=`
- Vendored `xxhash.h` SHA-256:
  `153be464f90daf4239f3e5aa0b347b0b316774555ae1f18826162e5cef1dc3e1`
- License: BSD 2-Clause; see `LICENSE`. The header also embeds the license.

The bundled configuration defines `XXH_IMPLEMENTATION`,
`XXH_STATIC_LINKING_ONLY`, and a libmustache-specific `XXH_NAMESPACE` before
including this header. This keeps one implementation behind the private
out-of-line Cista XXH3 adapter, avoids symbol collisions, and does not require a
separately linked xxHash library.

## Backported fix

The header includes the two-line strict-aliasing fix from upstream
[PR #1013](https://github.com/Cyan4973/xxHash/pull/1013), merged in commit
`ee34939dee941c8241d6a97097c49b62b46b9dae`. It adds `__may_alias__` to the
unaligned 32-bit and 64-bit read typedefs used by `XXH_FORCE_MEMORY_ACCESS=1`.
These reads must be allowed to alias the input's original type. Without that
attribute, optimized GCC builds can produce inconsistent Cista type hashes;
GCC 16 LTO exposes this in
[libmustache issue #29](https://github.com/jbboehr/libmustache/issues/29).

The patch preserves the hash algorithm and archive bytes. Bundled builds use
normal optimization without `noipa`. System xxHash may still be unpatched, so
the private adapter retains GCC's `noipa` attribute where available for those
builds, including LTO and configuration probes.

On GCC/AArch64, the bundled adapter defaults to `XXH_FORCE_MEMORY_ACCESS=0`
before including the header. This selects xxHash's `memcpy` scalar reads and
avoids [GCC bug #124146](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124146),
an internal compiler error involving aligned `may_alias` types during LTO.
The hash algorithm, vector implementation, and LTO remain enabled as before.
The upstream backport is retained for other configurations.

## Updating

Treat an xxHash update as an archive-format dependency change. Review the
upstream diff and license, copy `xxhash.h` and `LICENSE` from a clean checkout
of the selected release, reapply the backport above if it is not yet included,
update every pin and checksum above plus
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
