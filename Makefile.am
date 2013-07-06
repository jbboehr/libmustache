
EXTRA_DIST = Readme.md
SUBDIRS = doc src bin test

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mustache.pc

ACLOCAL_AMFLAGS = -I m4


standard: all
	./bin/mustache -e -t test/fixtures/standard.mustache -d test/fixtures/standard.yml
	
standardc: all
	./bin/mustache -c -t test/fixtures/standard.mustache -o test/fixtures/standard.bin

standardr: all
	./bin/mustache -c -r -t test/fixtures/complex.mustache -o test/fixtures/standard.log
	cat test/fixtures/standard.log

standardce: all standardc
	./bin/mustache -e -t test/fixtures/standard.bin -d test/fixtures/standard.yml

standardcr: all standardc
	./bin/mustache -p -t test/fixtures/standard.bin -r

complexc: all
	./bin/mustache -c -t test/fixtures/complex.mustache -o test/fixtures/complex.bin

complexr: all
	./bin/mustache -c -r -t test/fixtures/complex.mustache -o test/fixtures/complex.log
	cat test/fixtures/complex.log

complexce: all complexc
	./bin/mustache -e -t test/fixtures/complex.bin -d test/fixtures/complex.yml

deepc: all
	./bin/mustache -c -t test/fixtures/deep.mustache -o test/fixtures/deep.bin

deepr: all
	./bin/mustache -c -r -t test/fixtures/deep.mustache -o test/fixtures/deep.log
	cat test/fixtures/deep.log

deepce: all complexc
	./bin/mustache -e -t test/fixtures/deep.bin -d test/fixtures/deep.yml
	
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
	rm test/gmon.out
	EXEC_NUM=10000 test/spec/test_spec spec/specs
	gprof test/spec/test_spec gmon.out > gprof.out

profile-clean:
	find . -name 'gmon.out' -delete
	find . -name 'gprof.out' -delete
endif
