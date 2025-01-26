from exp import Exp
import csv
import time
import sys


def make_query(col_len: str, col_id: int) -> str:
    SELECTIVITY = 0.005
    csel = Exp.col_path(col_len, col_id)
    cfet = Exp.col_path(col_len, (col_id + 1) % 2)

    selects = []
    for num in range(5):
        rmin, rmax = Exp.sel_range(col_len, SELECTIVITY)
        selects.append(f's{num}=select({csel},{rmin},{rmax})')
        selects.append(f'f{num}=fetch({cfet},s{num})')
    return "\n".join(selects)


def main(exp_num: int):
    TRIALS_PER_SIZE = 5
    Exp.setup()

    with open(f'./res/m3e{exp_num}.csv', 'w', newline='') as file:
        writer = csv.writer(file, lineterminator='\n')
        writer.writerow(
            ["col_len", "idx_time", "default_time",
             "idx_maj_faults", "idx_min_faults",
             "default_maj_faults", "default_min_faults"])

        IDX = 0
        DEF = 1
        for col_len in Exp.default_lens():
            time_total = [0, 0]
            majf_total = [0, 0]
            minf_total = [0, 0]

            for trial in range(TRIALS_PER_SIZE):
                server = Exp.run_server()
                Exp.wipe_caches()
                print("sleeping to load indexes")
                time.sleep(15)
                print(f"{col_len}: trial {trial}")

                for col_id in [IDX, DEF]:
                    queries = make_query(col_len, col_id)

                    majf_before, minf_before = Exp.get_maj_min_faults(server)
                    time_total[col_id] += Exp.run_client(queries)
                    majf_after, minf_after = Exp.get_maj_min_faults(server)

                    majf_total[col_id] += majf_after - majf_before
                    minf_total[col_id] += minf_after - minf_before

                Exp.shutdown_server(server)
            writer.writerow(map(str, [
                col_len,
                time_total[IDX] / TRIALS_PER_SIZE,
                time_total[DEF] / TRIALS_PER_SIZE,
                majf_total[IDX] / TRIALS_PER_SIZE,
                minf_total[IDX] / TRIALS_PER_SIZE,
                majf_total[DEF] / TRIALS_PER_SIZE,
                minf_total[DEF] / TRIALS_PER_SIZE
            ]))


if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else None
    if arg == "sorted":
        main(2)
    elif arg == "btree":
        main(3)
    else:
        print("*.py [sorted | btree]")
