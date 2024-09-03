use anyhow::Result;
use clap::{Parser, ValueEnum};
use std::{fs::File, io::BufReader, path::PathBuf, time::Instant};

#[derive(Parser)]
struct Args {
    nums: PathBuf,
    algo: Algorithm,
    greater_than: usize,
}

#[derive(ValueEnum, Clone, Copy, Debug)]
enum Algorithm {
    Count,
    Fold,
    ForIf,
    ForNoIf,
}

fn main() -> Result<()> {
    let args = Args::parse();
    let nums: Vec<usize> = serde_json::from_reader(BufReader::new(File::open(&args.nums)?))?;

    let start = Instant::now();
    let count = match args.algo {
        Algorithm::Count => count_count(&nums, args.greater_than),
        Algorithm::Fold => count_fold(&nums, args.greater_than),
        Algorithm::ForIf => count_for_if(&nums, args.greater_than),
        Algorithm::ForNoIf => count_for(&nums, args.greater_than),
    };
    let dur = start.elapsed();
    println!(
        "{:?} took {:?} on {:?} nums: {count}",
        args.algo,
        dur,
        nums.len()
    );
    Ok(())
}

fn count_count(nums: &[usize], geq: usize) -> usize {
    nums.iter().copied().filter(|&num| num > geq).count()
}

fn count_fold(nums: &[usize], geq: usize) -> usize {
    nums.iter()
        .copied()
        .fold(0, |acc, num| if num > geq { acc + 1 } else { acc })
}

fn count_for_if(nums: &[usize], geq: usize) -> usize {
    let mut count = 0;
    for num in nums {
        if *num > geq {
            count += 1;
        }
    }
    count
}

fn count_for(nums: &[usize], geq: usize) -> usize {
    let mut count = 0;
    for num in nums {
        count += (*num > geq) as usize
    }
    count
}
