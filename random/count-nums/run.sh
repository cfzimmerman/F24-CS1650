#!/bin/zsh

cargo build --release;

for attempt in {1..5}; do 
  echo "starting $attempt"
  for method in {fold,for-if,count,for-no-if}; do
    ./target/release/count-nums "./nums.json" "$method" 500
  done
done
