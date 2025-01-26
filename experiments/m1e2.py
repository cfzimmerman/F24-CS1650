from exp import Exp
import csv


def make_queries(col_len: str, selectivity: float, fetch: bool) -> str:
    QUERY_CT = 20

    c0 = Exp.col_path(col_len, 0)
    cmds = []
    for ct in range(QUERY_CT):
        rmin, rmax = Exp.sel_range(col_len, selectivity)
        assert rmin <= rmax
        cmds.append(f's{ct}=select({c0},{rmin},{rmax})')
        if fetch:
            cmds.append(f'f{ct}=fetch({c0},s{ct})')
    return "\n".join(cmds)


def main():
    TRIALS = 5

    col_len = Exp.default_lens()[5]
    with open('./res/m1e2.csv', 'w', newline='') as file:
        writer = csv.writer(file, lineterminator='\n')
        writer.writerow(["col_len", "selectivity", "time_sel", "time_selfetch",
                        "majf_sel", "majf_selfetch", "minf_sel",
                         "minf_selfetch"])

        sel = 0.001
        while sel < 0.61:
            times = [0., 0.]
            majf = [0, 0]
            minf = [0, 0]
            for trial in range(TRIALS):
                for idx, mode in enumerate(["select", "select_fetch"]):
                    print(f"{sel}: trial {trial} - {mode}")
                    queries = make_queries(col_len, sel, idx == 1)
                    server = Exp.run_server()
                    Exp.wipe_caches()

                    majf_before, minf_before = Exp.get_maj_min_faults(server)
                    times[idx] += Exp.run_client(queries)
                    majf_after, minf_after = Exp.get_maj_min_faults(server)

                    majf[idx] = majf_after - majf_before
                    minf[idx] = minf_after - minf_before

                    Exp.shutdown_server(server)
            writer.writerow(map(str, [
                col_len,
                sel,
                times[0] / TRIALS,
                times[1] / TRIALS,
                majf[0] / TRIALS,
                majf[1] / TRIALS,
                minf[0] / TRIALS,
                minf[1] / TRIALS
            ]))
            sel += 0.02


if __name__ == "__main__":
    main()
