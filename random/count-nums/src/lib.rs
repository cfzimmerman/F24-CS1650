pub fn count_count(nums: &[usize], geq: usize) -> usize {
    nums.iter().copied().filter(|&num| num > geq).count()
}

pub fn count_fold(nums: &[usize], geq: usize) -> usize {
    nums.iter()
        .copied()
        .fold(0, |acc, num| if num > geq { acc + 1 } else { acc })
}

pub fn count_for_if(nums: &[usize], geq: usize) -> usize {
    let mut count = 0;
    for num in nums {
        if *num > geq {
            count += 1;
        }
    }
    count
}

pub fn count_for(nums: &[usize], geq: usize) -> usize {
    let mut count = 0;
    for num in nums {
        count += (*num > geq) as usize
    }
    count
}
