from exp import Exp
import csv


def make_queries(col_len: str) -> str:
    QUERY_CT = 5
    SELECTIVITY = 0.005

    c0 = Exp.col_path(col_len, 0)
    c1 = Exp.col_path(col_len, 1)
    cmds = []
    for ct in range(QUERY_CT):
        rmin, rmax = Exp.sel_range(col_len, SELECTIVITY)
        assert rmin <= rmax
        cmds.append(f's{ct}=select({c0},{rmin},{rmax})')
        cmds.append(f'f{ct}=fetch({c1},s{ct})')
    return "\n".join(cmds)


def main():
    TRIALS_PER_SIZE = 5

    Exp.setup()
    server = Exp.run_server()

    with open('./res/m1e1.csv', 'w', newline='') as file:
        writer = csv.writer(file, lineterminator='\n')
        writer.writerow(
            ["col_len", "avg_time", "micros/len", "avg_page_faults"])

        for col_len in Exp.default_lens():
            time_total = 0.
            faults_total = 0
            results = [str(col_len)]

            for trial in range(TRIALS_PER_SIZE):
                queries = make_queries(col_len)

                faults_before, _ = Exp.get_maj_min_faults(server)
                rtime = Exp.run_client(queries)
                faults_total += Exp.get_maj_min_faults(
                    server)[0] - faults_before
                time_total += rtime

            avg_time = time_total / TRIALS_PER_SIZE
            for item in [avg_time,
                         (avg_time * 1_000_000) / col_len,
                         faults_total / TRIALS_PER_SIZE]:
                results.append(str(item))
            writer.writerow(results)

    Exp.shutdown_server(server)


if __name__ == "__main__":
    main()
