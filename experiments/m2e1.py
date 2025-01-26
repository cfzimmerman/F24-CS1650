from exp import Exp
import csv


def make_queries(col_len: int, sel_ct: int) -> str:
    SELECTIVITY = 0.005
    queries = []
    col = Exp.col_path(col_len, 0)
    for ct in range(sel_ct):
        rmin, rmax = Exp.sel_range(col_len, SELECTIVITY)
        queries.append(f's{ct}=select({col},{rmin},{rmax})')

    selects = "\n".join(queries)
    return f"""
    batch_queries()
    {selects}
    batch_execute()
    """


def main():
    COL_LEN = Exp.default_lens()[-2]
    TRIALS_PER_SIZE = 5

    Exp.setup()

    with open('./res/m2e1.csv', 'w', newline='') as file:
        writer = csv.writer(file, lineterminator='\n')
        writer.writerow(
            ["col_len", "select_ct", "avg_time",
             "secs/select", "avg_maj_page_faults", "avg_min_page_faults"])

        select_ct = 1
        while select_ct <= 256:
            time_total = 0.
            majf_total = 0
            minf_total = 0

            for trial in range(TRIALS_PER_SIZE):
                print(f'{select_ct}: trial {trial}')
                server = Exp.run_server()
                Exp.wipe_caches()
                queries = make_queries(COL_LEN, select_ct)

                maj_before, min_before = Exp.get_maj_min_faults(server)
                rtime = Exp.run_client(queries)
                maj_after, min_after = Exp.get_maj_min_faults(server)

                majf_total += maj_after - maj_before
                minf_total += min_after - min_before
                time_total += rtime

                server.terminate()
                server.wait()

            avg_time = time_total / TRIALS_PER_SIZE
            writer.writerow([
                COL_LEN,
                select_ct,
                avg_time,
                avg_time / select_ct,
                majf_total / TRIALS_PER_SIZE,
                minf_total / TRIALS_PER_SIZE
            ])
            select_ct *= 2


if __name__ == "__main__":
    main()
