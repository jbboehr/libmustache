
EXTRA_DIST = README.md spec/specs
SUBDIRS = doc src bin tests

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mustache.pc

ACLOCAL_AMFLAGS = -I m4


standard: all
	./bin/mustache -e -t tests/fixtures/standard.mustache -d tests/fixtures/standard.yml
	
standardc: all
	./bin/mustache -c -t tests/fixtures/standard.mustache -o tests/fixtures/standard.bin

standardr: all
	./bin/mustache -c -r -t tests/fixtures/complex.mustache -o tests/fixtures/standard.log
	cat tests/fixtures/standard.log

standardce: all standardc
	./bin/mustache -e -t tests/fixtures/standard.bin -d tests/fixtures/standard.yml

standardcr: all standardc
	./bin/mustache -p -t tests/fixtures/standard.bin -r

complexc: all
	./bin/mustache -c -t tests/fixtures/complex.mustache -o tests/fixtures/complex.bin

complexr: all
	./bin/mustache -c -r -t tests/fixtures/complex.mustache -o tests/fixtures/complex.log
	cat tests/fixtures/complex.log

complexce: all complexc
	./bin/mustache -e -t tests/fixtures/complex.bin -d tests/fixtures/complex.yml

deepc: all
	./bin/mustache -c -t tests/fixtures/deep.mustache -o tests/fixtures/deep.bin

deepr: all
	./bin/mustache -c -r -t tests/fixtures/deep.mustache -o tests/fixtures/deep.log
	cat tests/fixtures/deep.log

deepce: all deepc
	./bin/mustache -e -t tests/fixtures/deep.bin -d tests/fixtures/deep.yml
	
if GCC
# These flags are gcc specific
export AM_CFLAGS = -Wall -Wextra -Wdeclaration-after-statement
endif

test: all check

# Coverage
if GCOV
clean-local:: gcov-clean lcov-clean

coverage-init: 
	lcov --directory . --zerocounters -q

coverage-main:
	lcov --capture --compat-libtool --directory . --output-file libmustache_test.info
	lcov --remove libmustache_test.info "/usr*" --compat-libtool --output-file libmustache_test.info
	genhtml -o test_coverage -t "libmustache test coverage" --num-spaces 4 libmustache_test.info

coverage: all coverage-init test coverage-main

gcov-clean:
	find . -name '*.gcda' -delete
	find . -name '*.gcno' -delete

lcov-clean:
	rm -rf test_coverage
	rm -f libmustache_test.info
endif

# profile
if GPROF
clean-local:: profile-clean
	
profile: test
	rm tests/gmon.out
	EXEC_NUM=10000 tests/spec/test_spec spec/specs
	gprof tests/spec/test_spec gmon.out > gprof.out

profile-clean:
	find . -name 'gmon.out' -delete
	find . -name 'gprof.out' -delete
endif
