use criterion::{criterion_group, criterion_main, Criterion};
use std::{fs::File, io::BufReader};

const NUMS: &str = "./benches/nums.json";

fn counting_bench(criterion: &mut Criterion) {
    let nums: Vec<usize> =
        serde_json::from_reader(BufReader::new(File::open(NUMS).unwrap())).unwrap();

    criterion.bench_function(".count", |b| {
        b.iter(|| {
            count_nums::count_count(&nums, 500);
        })
    });

    criterion.bench_function(".fold", |b| {
        b.iter(|| {
            count_nums::count_fold(&nums, 500);
        })
    });

    criterion.bench_function("for if", |b| {
        b.iter(|| {
            count_nums::count_for_if(&nums, 500);
        })
    });

    criterion.bench_function("for no if", |b| {
        b.iter(|| {
            count_nums::count_for(&nums, 500);
        })
    });
}

criterion_group!(benches, counting_bench);
criterion_main!(benches);
