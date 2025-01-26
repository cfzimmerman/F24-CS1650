from exp import Exp
import csv
import asyncio


def make_queries(col_len: int, single_core: bool) -> str:
    SELECTS = 48
    SELECTIVITY = 0.005
    col = Exp.col_path(col_len, 0)

    selects = []
    for ct in range(SELECTS):
        rmin, rmax = Exp.sel_range(col_len, SELECTIVITY)
        selects.append(f's{ct}=select({col},{rmin},{rmax})')

    queries = "\n".join(selects)
    return f"""
    batch_queries()
    {queries}
    batch_execute()
    """


async def main():
    TRIALS = 5
    Exp.setup()

    with open('./res/m2e2.csv', 'w', newline='') as file:
        writer = csv.writer(file, lineterminator='\n')
        writer.writerow(
            ["col_len", "mode", "secs", "pcpu"])

        for idx, mode in enumerate(["single_thread", "multi_thread"]):
            server = (Exp.run_server_single_thread()
                      if idx == 0 else Exp.run_server())
            for col_len in Exp.default_lens():
                time_total = 0.
                max_pcpu = 0.
                for trial in range(TRIALS):
                    print(f"{col_len}: trial {trial} - {mode}")
                    queries = make_queries(col_len, idx == 0)
                    client = asyncio.create_task(
                        Exp.run_client_async(queries))
                    while not client.done():
                        max_pcpu = max(max_pcpu, Exp.get_pcpu(server))
                        await asyncio.sleep(0.01)
                    time_total += await client
                writer.writerow(map(str, [
                    col_len,
                    mode,
                    time_total / TRIALS,
                    max_pcpu
                ]))
            Exp.shutdown_server(server)


if __name__ == "__main__":
    asyncio.run(main())
