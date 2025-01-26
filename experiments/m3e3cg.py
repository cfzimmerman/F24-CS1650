from exp import Exp
import time
import sys


def make_query(col_len: str, col_id: int) -> str:
    SELECTIVITY = 0.005
    csel = Exp.col_path(col_len, col_id)

    selects = []
    for num in range(64):
        rmin, rmax = Exp.sel_range(col_len, SELECTIVITY)
        selects.append(f's{num}=select({csel},{rmin},{rmax})')
    return "\n".join(selects)


def main(exp_num: int):
    Exp.setup()
    col_len = Exp.default_lens()[-1]

    server = Exp.run_server(cachegrind=True)
    Exp.wipe_caches()

    print("sleeping to load indexes")
    time.sleep(30)

    queries = make_query(col_len, 0)

    rtime = Exp.run_client(queries)
    print(f"took {rtime} secs")

    Exp.shutdown_server(server)


if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else None
    if arg == "sorted":
        main(2)
    elif arg == "btree":
        main(3)
    else:
        print("*.py [sorted | btree]")
