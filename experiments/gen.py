import csv
import random
from exp import Exp
import sys

INDEX_TBL_LEN = 50_000_000


def create_table_csv(path: str,
                     db_name: str,
                     table_name: str,
                     num_cols: int,
                     col_len: int):
    with open(path, 'w', newline='') as file:
        writer = csv.writer(file, lineterminator='\n')
        writer.writerow([Exp.col_path(col_len, num)
                         for num in range(num_cols)])
        range_max = col_len // 2
        range_min = -range_max
        for _ in range(col_len):
            writer.writerow([random.randint(range_min, range_max)
                            for _ in range(num_cols)])


def default():
    random.seed(1423)
    for col_len in Exp.default_lens():
        print(f"generating: {col_len}")
        create_table_csv(
            f"./csvs/rng-{col_len}.csv", Exp.DB_NAME,
            Exp.table_name(col_len), 2, col_len)


def index():
    random.seed(1941)

    print(f"generating: {INDEX_TBL_LEN}")
    create_table_csv(f"./csvs/index-{INDEX_TBL_LEN}.csv",
                     Exp.DB_NAME, Exp.table_name(INDEX_TBL_LEN),
                     3, INDEX_TBL_LEN)


if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else None
    if arg == "default":
        default()
    elif arg == "index":
        index()
    else:
        print("*.py [basic | index]")
