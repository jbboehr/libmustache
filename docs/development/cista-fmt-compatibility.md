# Cista and optional fmt support

The amalgamated Cista header contains an optional specialization for
`fmt::range_format_kind`. Older Cista releases enabled that code whenever a
system `fmt/ranges.h` happened to be discoverable, even though libmustache does
not use Cista's fmt integration. Newer fmt releases changed that API, which can
make an otherwise unrelated libmustache configure probe fail with
`range_format_kind is not a class template`.

The vendored header therefore enables the specialization only when `CISTA_FMT`
is explicitly defined. `src/archive/cista_include.hpp` undefines that macro
before including Cista, so libmustache remains independent of any system fmt
installation. This preserves Cista's optional integration for consumers that
opt into it while avoiding accidental detection of unrelated system headers.

This is tracked upstream in [Cista issue #264](https://github.com/felixguendling/cista/issues/264),
which documents the same amalgamated-header problem.
