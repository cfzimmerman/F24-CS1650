from exp import Exp
import csv
import asyncio
from m4e1 import make_queries


async def main():
    TRIALS = 5
    Exp.setup()

    with open('./res/m4e2.csv', 'w', newline='') as file:
        writer = csv.writer(file, lineterminator='\n')
        writer.writerow(
            ["col_len", "mode", "selectivity", "secs", "pcpu", "est_jinput"])

        for idx, mode in enumerate(["single_thread", "multi_thread"]):
            server = (Exp.run_server_single_thread()
                      if idx == 0 else Exp.run_server())
            col_len = Exp.default_lens()[5]
            sel = 0.002
            while sel < 0.36:
                time_total = 0.
                max_pcpu = 0.
                for trial in range(TRIALS):
                    print(f"{sel}: trial {trial}")
                    queries = make_queries(col_len, sel)
                    client = asyncio.create_task(Exp.run_client_async(queries))
                    while not client.done():
                        max_pcpu = max(max_pcpu, Exp.get_pcpu(server))
                        await asyncio.sleep(0.01)
                    time_total += await client

                writer.writerow(map(str, [
                    col_len,
                    mode,
                    sel,
                    time_total / TRIALS,
                    max_pcpu,
                    int(col_len * sel * 2)
                ]))
                sel *= 1.3

            Exp.shutdown_server(server)


if __name__ == "__main__":
    asyncio.run(main())
