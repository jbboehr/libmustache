SUBDIRS = src bin tests

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mustache.pc

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	README.md \
	LICENSE.md \
	CMakeLists.txt \
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
