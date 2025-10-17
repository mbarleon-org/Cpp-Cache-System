#!/bin/bash

output=$(find ./mains -maxdepth 1 -type f -name 'main*.cpp' -print0 | sort -z -udf | xargs -0 -n1 sh -c 'g++ "$1" --std=c++20 && ./a.out' sh)
echo "$output"

if echo "$output" | grep -q '^[FAIL]'; then
    exit 84
fi
