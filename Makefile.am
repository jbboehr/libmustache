SUBDIRS = src bin tests

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mustache.pc

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	README.md \
	LICENSE.md \
	benchmarks/CMakeLists.txt \
	benchmarks/ast-cache-vs-source.cpp \
	benchmarks/ast-cache-vs-source.php \
	benchmarks/cista-archive.cpp \
	benchmarks/cista-archive.hpp \
	benchmarks/cista-xxh3/xxh3.h \
	benchmarks/test-cista-archive.cpp \
	benchmarks/results/ast-cache-vs-source-2026-08-23-cpp-run1.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-23-cpp-run2.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-23-cpp-run3.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-23-php-run1.json \
	benchmarks/results/ast-cache-vs-source-2026-08-23-php-run2.json \
	benchmarks/results/ast-cache-vs-source-2026-08-23-php-run3.json \
	benchmarks/results/ast-cache-vs-source-2026-08-24-cista-run1.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-24-cista-run2.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-24-cista-run3.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-cista-security-run1.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-cista-security-run2.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-cista-security-run3.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-checksums-run1.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-checksums-run2.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-checksums-run3.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-selected-static-external-run1.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-selected-static-external-run2.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-selected-static-external-run3.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-selected-version-xxh3-run1.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-selected-version-xxh3-run2.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-selected-version-xxh3-run3.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-legacy-writer-comparison-run1.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-legacy-writer-comparison-run2.csv \
	benchmarks/results/ast-cache-vs-source-2026-08-25-legacy-writer-comparison-run3.csv \
	docs/development/abi-6-source-migration-2026-08-20.md \
	docs/development/ast-cache-benchmark-2026-08-22.md \
	CMakeLists.txt \
	.clang-tidy \
	.appveyor.yml \
	.envrc \
	.github/workflows/ci.yml \
	cmake/mustache_config.h.in \
	cmake/mustacheConfig.cmake.in \
	default.nix \
	flake.nix \
	flake.lock \
	nix/derivation.nix \
	scripts/check-version-consistency.sh \
	spec/specs \
	tests/check_windows_exports.cmake \
	tests/cmake-consumer/CMakeLists.txt \
	tests/cmake-consumer/main.cpp \
	tests/cmake-find-quiet/CMakeLists.txt \
	tests/cmake-subproject/CMakeLists.txt \
	tests/cmake-subproject/main.cpp \
	vcpkg.json

.PHONY: check-version render-standard render-complex render-deep

check-local: check-version

check-version:
	$(SHELL) $(top_srcdir)/scripts/check-version-consistency.sh

render-standard: all
	./bin/mustachec -t $(top_srcdir)/tests/fixtures/standard.mustache -d $(top_srcdir)/tests/fixtures/standard.yml

render-complex: all
	./bin/mustachec -t $(top_srcdir)/tests/fixtures/complex.mustache -d $(top_srcdir)/tests/fixtures/complex.yml

render-deep: all
	./bin/mustachec -t $(top_srcdir)/tests/fixtures/deep.mustache -d $(top_srcdir)/tests/fixtures/deep.yml

if GCOV
clean-local: gcov-clean lcov-clean

coverage-init:
	lcov --directory . --zerocounters -q

coverage-main:
	lcov --capture --compat-libtool --directory . --output-file libmustache_test.info
	lcov --remove libmustache_test.info "/usr*" --compat-libtool --output-file libmustache_test.info
	genhtml -o test_coverage -t "libmustache test coverage" --num-spaces 4 libmustache_test.info

coverage: all coverage-init check coverage-main

gcov-clean:
	find . -name '*.gcda' -delete
	find . -name '*.gcno' -delete

lcov-clean:
	rm -rf test_coverage
	rm -f libmustache_test.info
endif

if GPROF
clean-local: profile-clean

profile: check
	EXEC_NUM=10000 tests/test_spec spec/specs
	gprof tests/test_spec gmon.out > gprof.out

profile-clean:
	find . -name 'gmon.out' -delete
	find . -name 'gprof.out' -delete
endif
