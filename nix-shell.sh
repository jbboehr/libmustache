#!/bin/sh
exec nix-shell -p autoconf -p gcc -p m4 -p libtool -p automake -p pcre -p libyaml -p pkgconfig
