from exp import Exp
from gen import INDEX_TBL_LEN
import csv
import time


def make_query(selectivity: float, col_num: int) -> str:
    col = Exp.col_path(INDEX_TBL_LEN, col_num)
    rmin, rmax = Exp.sel_range(INDEX_TBL_LEN, selectivity)

    return f"""
    sel=select({col},{rmin},{rmax})
    """


def main():
    TRIALS_PER_SEL = 5

    with open('./res/m3e1.csv', 'w', newline='') as file:
        writer = csv.writer(file, lineterminator='\n')
        writer.writerow(
            ["col_len", "col_type", "selectivity", "avg_time",
             "avg_maj_page_faults", "avg_min_page_faults"])

        col_types = ["sorted", "btree"]
        for col_id in range(1, 3):
            server = Exp.run_server()
            print("sleeping 30 to let indexes build")
            time.sleep(30)

            sel = 0.002
            while sel < 0.36:
                time_total = 0.
                majf_total = 0
                minf_total = 0
                for trial in range(TRIALS_PER_SEL):
                    print(f"sel: {sel}, col: {col_id}, trial: {trial}")

                    Exp.wipe_caches()
                    query = make_query(sel, col_id)
                    maj_before, min_before = Exp.get_maj_min_faults(server)
                    rtime = Exp.run_client(query)
                    maj_after, min_after = Exp.get_maj_min_faults(server)

                    majf_total += maj_after - maj_before
                    minf_total += min_after - min_before
                    time_total += rtime

                writer.writerow(map(str, [
                    INDEX_TBL_LEN,
                    col_types[col_id - 1],
                    sel,
                    time_total / TRIALS_PER_SEL,
                    majf_total / TRIALS_PER_SEL,
                    minf_total / TRIALS_PER_SEL
                ]))
                sel *= 1.3

            server.terminate()
            server.wait()


if __name__ == "__main__":
    main()
