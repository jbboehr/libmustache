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

calculate_sha256() {
  path=$1
  if command -v sha256sum >/dev/null 2>&1; then
    checksum_output=$(sha256sum "$path")
  elif command -v shasum >/dev/null 2>&1; then
    checksum_output=$(shasum -a 256 "$path")
  else
    printf 'Unable to verify vendored dependencies: no SHA-256 tool is available\n' >&2
    exit 1
  fi
  printf '%s\n' "${checksum_output%% *}"
}

check_vendored_header() {
  label=$1
  header=$2
  expected=$3

  if [ ! -f "$header" ]; then
    printf 'Unable to read vendored %s header from %s\n' "$label" "$header" >&2
    exit 1
  fi

  actual=$(calculate_sha256 "$header")
  if [ "$actual" != "$expected" ]; then
    printf 'Vendored %s SHA-256 is %s; expected %s\n' \
      "$label" "$actual" "$expected" >&2
    exit 1
  fi
}

check_documented_checksum() {
  label=$1
  readme=$2
  expected=$3

  if [ -f "$readme" ]; then
    documented=$(sed -n \
      's/^  [^0-9a-f]*\([0-9a-f]\{64\}\)[^0-9a-f]*$/\1/p' \
      "$readme")
    if [ "$documented" != "$expected" ]; then
      printf 'Documented %s SHA-256 is %s; expected %s\n' \
        "$label" "${documented:-missing}" "$expected" >&2
      exit 1
    fi
  elif [ "${MUSTACHE_VERSION_CHECK_ALLOW_MISSING:-0}" != 1 ]; then
    printf 'Unable to read vendored %s checksum from %s\n' \
      "$label" "$readme" >&2
    exit 1
  fi
}

expected_cista_sha256=e409ba42914b9988d662896cf5e7b855d9a46aab4d0726d85265a3aea2b915d2
expected_xxhash_sha256=17973c0dc49d9854ca26caa191f0e12f7a424b68858d9a78de3860d959d85e4b
check_vendored_header Cista "$repo_root/vendor/cista/cista.h" "$expected_cista_sha256"
check_documented_checksum Cista "$repo_root/vendor/cista/README.md" "$expected_cista_sha256"
check_vendored_header xxHash "$repo_root/vendor/xxhash/xxhash.h" "$expected_xxhash_sha256"
check_documented_checksum xxHash "$repo_root/vendor/xxhash/README.md" "$expected_xxhash_sha256"

printf 'Version %s and ABI %s are consistent\n' "$cmake_version" "$cmake_abi"
