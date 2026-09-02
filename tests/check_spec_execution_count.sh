#!/bin/sh

# Set by Automake's TESTS_ENVIRONMENT.
# shellcheck disable=SC2154
for invalid_value in +1 ' 1' 1x 0 -1 999999999999999999999999; do
    output=$(EXEC_NUM="$invalid_value" ./test_spec "$mustache_spec_dir" 2>&1)
    result=$?
    if test "$result" -ne 1; then
        echo "specification test accepted EXEC_NUM=$invalid_value (result $result)" >&2
        exit 1
    fi
    case $output in
        *"Invalid EXEC_NUM: expected a positive decimal integer"*) ;;
        *)
            echo "specification test did not diagnose EXEC_NUM=$invalid_value" >&2
            exit 1
            ;;
    esac
done
