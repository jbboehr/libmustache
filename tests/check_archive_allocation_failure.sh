#!/bin/sh

for invalid_value in +1 ' 1' 1x -1 999999999999999999999999; do
    invalid_output=$(./test_allocation_failure --archive-allocation-probe serialize "$invalid_value" 2>&1)
    invalid_result=$?
    if test "$invalid_result" -ne 12; then
        echo "archive allocation probe accepted $invalid_value (result $invalid_result)" >&2
        exit 1
    fi
    case $invalid_output in
        *"invalid archive allocation failure index"*) ;;
        *)
            echo "archive allocation probe did not diagnose $invalid_value" >&2
            exit 1
            ;;
    esac
done

for operation in serialize load; do
    observed_failure=no
    fail_at=0
    while test "$fail_at" -le 4095; do
        ./test_allocation_failure --archive-allocation-probe "$operation" "$fail_at"
        result=$?
        case "$result" in
            10)
                observed_failure=yes
                ;;
            0)
                if test "$observed_failure" != yes; then
                    echo "archive $operation succeeded without exercising an allocation failure" >&2
                    exit 1
                fi
                break
                ;;
            *)
                echo "archive $operation did not propagate allocation failure $fail_at (result $result)" >&2
                exit 1
                ;;
        esac
        fail_at=$((fail_at + 1))
    done
    if test "$fail_at" -gt 4095; then
        echo "archive $operation did not complete within 4096 allocation attempts" >&2
        exit 1
    fi
done
