#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: %s ARCHIVE.tar.gz\n' "$0" >&2
  exit 2
fi

jobs=${MUSTACHE_RELEASE_CHECK_JOBS:-2}
case $jobs in
  '' | *[!0-9]* | 0)
    printf 'MUSTACHE_RELEASE_CHECK_JOBS must be a positive integer\n' >&2
    exit 2
    ;;
esac

archive_dir=$(CDPATH='' cd "$(dirname "$1")" && pwd)
archive="$archive_dir/$(basename "$1")"
if [ ! -f "$archive" ]; then
  printf 'release archive does not exist: %s\n' "$archive" >&2
  exit 2
fi

archive_name=$(basename "$archive" .tar.gz)
case $archive_name in
  '' | . | .. | */*)
    printf 'release archive has an invalid name: %s\n' "$archive" >&2
    exit 2
    ;;
esac

scratch=$(mktemp -d "${TMPDIR:-/tmp}/libmustache-release-check.XXXXXX")
cleanup() {
  cmake -E remove_directory "$scratch"
}
trap cleanup EXIT HUP INT TERM

tar -xzf "$archive" -C "$scratch"
source_dir="$scratch/$archive_name"
if [ ! -d "$source_dir" ]; then
  printf 'release archive did not contain its expected root: %s\n' "$archive_name" >&2
  exit 1
fi

"$source_dir/scripts/check-version-consistency.sh"

cmake_full="$scratch/cmake-full"
cmake_prefix="$scratch/cmake-prefix"
cmake -S "$source_dir" -B "$cmake_full" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$cmake_prefix" \
  -DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=ON \
  -DMUSTACHE_ENABLE_JSON=ON \
  -DMUSTACHE_ENABLE_YAML=ON \
  -DMUSTACHE_WARNINGS_AS_ERRORS=ON
cmake --build "$cmake_full" --parallel "$jobs"
cmake -E chdir "$cmake_full" \
  ctest --parallel "$jobs" --output-on-failure
cmake --install "$cmake_full"

for linkage in shared static; do
  consumer_build="$scratch/consumer-$linkage"
  cmake -S "$source_dir/tests/cmake-consumer" -B "$consumer_build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$cmake_prefix" \
    -DMUSTACHE_CONSUMER_LINKAGE="$linkage"
  cmake --build "$consumer_build" --parallel "$jobs"
  "$consumer_build/mustache_consumer"
done

cmake_minimal="$scratch/cmake-minimal"
cmake -S "$source_dir" -B "$cmake_minimal" \
  -DCMAKE_BUILD_TYPE=Release \
  -DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=OFF \
  -DMUSTACHE_ENABLE_JSON=OFF \
  -DMUSTACHE_ENABLE_YAML=OFF \
  -DMUSTACHE_WARNINGS_AS_ERRORS=ON
cmake --build "$cmake_minimal" --parallel "$jobs"
cmake -E chdir "$cmake_minimal" \
  ctest --parallel "$jobs" --output-on-failure

autotools_minimal="$scratch/autotools-minimal"
autotools_prefix="$scratch/autotools-prefix"
mkdir "$autotools_minimal"
(
  cd "$autotools_minimal"
  "$source_dir/configure" \
    --prefix="$autotools_prefix" \
    --disable-archived-templates \
    --disable-shared \
    --enable-static \
    --enable-warnings-as-errors \
    --without-json \
    --without-yaml
  make --jobs="$jobs" check
  make install
)

PKG_CONFIG_PATH="$autotools_prefix/lib/pkgconfig"
export PKG_CONFIG_PATH
package_version=$(pkg-config --modversion mustache)
# pkg-config deliberately emits compiler and linker arguments as shell words.
# shellcheck disable=SC2046
${CXX:-c++} "$source_dir/tests/cmake-consumer/main.cpp" \
  -std=c++11 \
  "-DMUSTACHE_EXPECTED_VERSION=\"$package_version\"" \
  -DMUSTACHE_EXPECT_STATIC_DEFINE \
  $(pkg-config --static --cflags --libs mustache) \
  -o "$scratch/autotools-static-consumer"
"$scratch/autotools-static-consumer"

printf 'Release archive verification passed: %s\n' "$archive"
