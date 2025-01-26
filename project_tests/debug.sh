#!/bin/bash

set -e

cleanup() {
  if pgrep server; then
    echo "Cleaning up: Killing server process"
    pkill server
  fi
}

trap cleanup EXIT

cd /cs165/src
make clean
make distclean
make all

./server &
sleep 1

time ./client </cs165/project_tests/debug.dsl && sleep 1
