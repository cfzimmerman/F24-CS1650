#!/bin/bash

set -e

# the test directory, containing generated data dl, exp, and csvs
INPUT_DIR=/cs165/local_test_inputs
OUTPUT_DIR=/cs165/local_test_outputs

TEST=$(printf "%02d" "$1")

echo "diff for test $TEST"

diff "$INPUT_DIR/test${TEST}gen.exp" "$OUTPUT_DIR/test${TEST}gen.cleaned.out"
