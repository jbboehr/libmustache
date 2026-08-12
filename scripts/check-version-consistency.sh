#!/bin/sh

set -eu

script_dir=$(CDPATH='' cd "$(dirname "$0")" && pwd)
repo_root=$(CDPATH='' cd "$script_dir/.." && pwd)

cmake_version=$(sed -n \
  's/^project(mustache VERSION \([^ ]*\) LANGUAGES CXX)$/\1/p' \
  "$repo_root/CMakeLists.txt")
autotools_version=$(sed -n \
  's/^AC_INIT(\[mustache\], \[\([^]]*\)\].*$/\1/p' \
  "$repo_root/configure.ac")
nix_file="$repo_root/nix/derivation.nix"
nix_version=
if [ -f "$nix_file" ]; then
  nix_version=$(sed -n \
    's/^  version = "\([^"]*\)";$/\1/p' \
    "$nix_file")
fi
vcpkg_version=$(sed -n \
  's/^  "version-string": "\([^"]*\)",$/\1/p' \
  "$repo_root/vcpkg.json")
consumer_version=$(sed -n \
  's/^#define MUSTACHE_EXPECTED_VERSION "\([^"]*\)"$/\1/p' \
  "$repo_root/tests/cmake-consumer/main.cpp")

check_version() {
  label=$1
  actual=$2
  source_file=${3:-}
  if [ -n "$source_file" ] && [ ! -f "$source_file" ] &&
    [ "${MUSTACHE_VERSION_CHECK_ALLOW_MISSING:-0}" = 1 ]; then
    return
  fi
  if [ -z "$actual" ] || [ "$actual" != "$cmake_version" ]; then
    printf '%s version is %s; expected %s\n' \
      "$label" "${actual:-missing}" "$cmake_version" >&2
    exit 1
  fi
}

check_version Autotools "$autotools_version"
check_version Nix "$nix_version" "$nix_file"
check_version vcpkg "$vcpkg_version"
check_version consumer-test "$consumer_version"

cmake_abi=$(sed -n \
  's/^set(MUSTACHE_ABI_VERSION \([0-9][0-9]*\))$/\1/p' \
  "$repo_root/CMakeLists.txt")
libtool_version=$(sed -n \
  's/^[[:space:]]*-version-info \([0-9][0-9]*:[0-9][0-9]*:[0-9][0-9]*\)$/\1/p' \
  "$repo_root/src/Makefile.am")

case $libtool_version in
  *:*:*) ;;
  *)
  printf 'Unable to read the Libtool ABI version\n' >&2
  exit 1
  ;;
esac

libtool_current=${libtool_version%%:*}
libtool_rest=${libtool_version#*:}
libtool_age=${libtool_rest#*:}
libtool_soversion=$((libtool_current - libtool_age))
if [ -z "$cmake_abi" ] || [ "$libtool_soversion" != "$cmake_abi" ]; then
  printf 'CMake ABI is %s; Libtool computes ABI %s from %s\n' \
    "${cmake_abi:-missing}" "$libtool_soversion" "$libtool_version" >&2
  exit 1
fi

printf 'Version %s and ABI %s are consistent\n' "$cmake_version" "$cmake_abi"
