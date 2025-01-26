from exp import Exp
import csv

'''
Hash decision:

#define GRACE_DISK_PARTITION_LEN 174762
size_t use_grace =
    (outer.pos->len + inner.pos->len) >= GRACE_DISK_PARTITION_LEN;
'''


def make_queries(col_len: int, selectivity: float) -> str:
    JOINS_PER_CMD = 5
    joins = []
    for num in range(JOINS_PER_CMD):
        rmin, rmax = Exp.sel_range(col_len, selectivity)
        c0 = Exp.col_path(col_len, 0)
        c1 = Exp.col_path(col_len, 1)
        joins.append(f"s{num}a=select({c0},{rmin},{rmax})")
        joins.append(f"s{num}b=select({c1},{rmin},{rmax})")
        joins.append(f"f{num}a=fetch({c0},s{num}a)")
        joins.append(f"f{num}b=fetch({c1},s{num}b)")
        joins.append(
            f"t{num}a,t{num}b=join(f{num}a,s{num}a,f{num}b,s{num}b,hash)")
    return "\n".join(joins)


def main():
    TRIALS = 5
    Exp.setup()

    with open('./res/m4e1.csv', 'w', newline='') as file:
        writer = csv.writer(file, lineterminator='\n')
        writer.writerow(
            ["col_len", "selectivity", "avg_secs",
             "maj_faults", "min_faults", "est_jinput"])

        col_len = Exp.default_lens()[5]
        sel = 0.002
        while sel < 0.36:
            time_total = 0.
            majf_total = 0
            minf_total = 0
            for trial in range(TRIALS):
                print(f"{sel}: trial {trial}")
                queries = make_queries(col_len, sel)

                server = Exp.run_server()
                Exp.wipe_caches()

                majf_before, minf_before = Exp.get_maj_min_faults(server)
                time_total += Exp.run_client(queries)
                majf_after, minf_after = Exp.get_maj_min_faults(server)

                majf_total += majf_after - majf_before
                minf_total += minf_after - minf_before

                Exp.shutdown_server(server)

            writer.writerow(map(str, [
                col_len,
                sel,
                time_total / TRIALS,
                majf_total / TRIALS,
                minf_total / TRIALS,
                int(col_len * sel * 2)
            ]))
            sel *= 1.3


if __name__ == "__main__":
    main()
