#!/bin/bash

set -e

# bash test.ts [optional - 'f' to skip regenerating inputs]

# the test directory, containing generated data dl, exp, and csvs
INPUT_DIR=/cs165/local_test_inputs
OUTPUT_DIR=/cs165/local_test_outputs

mkdir -p $INPUT_DIR
mkdir -p $OUTPUT_DIR

DATA_SIZE=1000000
RAND_SEED=36
SLEEP=0.25s

cleanup() {
  if pgrep server; then
    echo "Cleaning up: Killing server process"
    pkill server
  fi
}

trap cleanup EXIT

cd /cs165/project_tests/data_generation_scripts

if [ $# -eq 0 ]; then
  echo "generating m1"
  python milestone1.py $DATA_SIZE $RAND_SEED $INPUT_DIR $INPUT_DIR

  echo "generating m2"
  python milestone2.py $DATA_SIZE $RAND_SEED $INPUT_DIR $INPUT_DIR

  echo "generating m3"
  python milestone3.py $DATA_SIZE $RAND_SEED $INPUT_DIR $INPUT_DIR

  echo "generating m4"
  python milestone4.py 500000 10000 10000 200000 $RAND_SEED 1.0 1000 $INPUT_DIR $INPUT_DIR
  # python milestone4.py $TBL_SIZE $JOIN_DIM1_SIZE $JOIN_DIM2_SIZE $JOIN_SELECT_SIZE $RAND_SEED $ZIPFIAN_PARAM $NUM_UNIQUE_ZIPF   ${OUTPUT_TEST_DIR} ${DOCKER_TEST_DIR}

else
  echo "skipping datagen"
fi

# setup code
cd /cs165/src
make clean
make distclean
make all

./server &
sleep $SLEEP

for test in {1..100}; do
  TEST=$(printf "%02d" "$test")
  echo ""
  echo "running test $TEST"

  time ./client <$INPUT_DIR/test${TEST}gen.dsl 1>$OUTPUT_DIR/test${TEST}gen.out
  sleep $SLEEP

  bash /cs165/infra_scripts/verify_output_standalone.sh $test "${OUTPUT_DIR}/test${TEST}gen.out" "${INPUT_DIR}/test${TEST}gen.exp" "${OUTPUT_DIR}/test${TEST}gen.cleaned.out" "${OUTPUT_DIR}/test${TEST}gen.cleaned.sorted.out"

  if grep -q "shutdown" "$INPUT_DIR/test${TEST}gen.dsl"; then
    # Restart the server for the next command
    ./server &
    sleep $SLEEP
  fi

done
