#!/bin/bash

set -u
set -o pipefail

output_file=$(mktemp)
trap 'rm -f "$output_file"' EXIT

# shellcheck disable=SC2016
if ! find ./tests -maxdepth 1 -type f -name '*_test.cpp' -print0 \
    | sort -z -udf \
    | xargs -0 -n1 sh -c 'printf "\n[TEST] %s\n" "$1"; g++ "$1" -I . --std=c++20 && ./a.out' sh \
    | tee "$output_file" \
    | awk '{
        if ($0 ~ /^\[OK\]/) {
            print "\033[32m" $0 "\033[0m"
        } else if ($0 ~ /^\[FAIL\]/) {
            print "\033[31m" $0 "\033[0m"
        } else {
            print
        }
        fflush()
    }'; then
    exit 84
fi

if grep -q '^\[FAIL\]' "$output_file"; then
    exit 84
fi
