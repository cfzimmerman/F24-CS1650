import subprocess
import time
from typing import List, Tuple
import random
import re
import asyncio


class Exp:
    DB_NAME = "db"

    @staticmethod
    def run_server(cachegrind=False, out_file=None) -> subprocess.Popen:
        cmd = "/cs165/src/server"
        if cachegrind:
            out = ("" if out_file is None
                   else f"--cachegrind-out-file={out_file} ")
            cmd = "valgrind --tool=cachegrind " + out + cmd
        if out_file is not None and cachegrind is False:
            cmd += f" >{out_file}"
        proc = subprocess.Popen(cmd.split(' '))
        time.sleep(1)
        return proc

    @staticmethod
    def shutdown_server(server: subprocess.Popen):
        Exp.run_client("""
        shutdown
        """)
        server.wait()

    @staticmethod
    def run_server_single_thread() -> subprocess.Popen:
        proc = subprocess.Popen(
            "/usr/bin/taskset -c 3 /cs165/src/server".split(' '))
        time.sleep(1)
        return proc

    @staticmethod
    def run_client(dsl: str) -> float:
        start = time.time()
        subprocess.run(["/cs165/src/client"], input=dsl, text=True, shell=True)
        return time.time() - start

    @staticmethod
    def default_lens() -> List[int]:
        return [390_625, 781_250, 1_562_500, 3_125_000, 6_250_000,
                12_500_000, 25_000_000, 50_000_000, 100_000_000, 200_000_000]

    @staticmethod
    def get_maj_min_faults(server: subprocess.Popen) -> Tuple[int, int]:
        proc = subprocess.run(
            f"ps -o maj_flt,min_flt --no-headers -p {server.pid}".split(" "),
            stdout=subprocess.PIPE, text=True)
        faults = re.sub(" +", ",", proc.stdout.strip()).split(',')
        return (int(faults[0]), int(faults[1]))

    @staticmethod
    def get_pcpu(server: subprocess.Popen) -> float:
        proc = subprocess.run(
            f"ps -o pcpu --no-headers -p {server.pid}".split(" "),
            stdout=subprocess.PIPE, text=True)
        return float(proc.stdout.strip())

    @staticmethod
    def wipe_caches():
        # requires running container with --privileged
        subprocess.run(
            "echo 3 > /proc/sys/vm/drop_caches".split(" "), shell=True)

    @staticmethod
    def setup():
        # Seed rng
        SEED = 1
        print(f'seeded rng with {SEED}')
        random.seed(SEED)

        # Rebuild project
        subprocess.run(["cd /cs165/src && make clean && make all"],
                       shell=True)

    @staticmethod
    def table_name(col_len: int) -> str:
        return f"tbl{col_len}"

    @staticmethod
    def col_path(col_len: int, col_num: int) -> str:
        return f"{Exp.DB_NAME}.{Exp.table_name(col_len)}.col{col_num}"

    @staticmethod
    def sel_range(col_len: int, selectivity: float) -> Tuple[int, int]:
        assert (0 <= selectivity and selectivity <= 1)
        mid = col_len // 2
        start = random.randint(-mid, mid)
        return (start, int(start + selectivity * col_len))

    @staticmethod
    async def run_client_async(dsl: str) -> float:
        start = time.time()
        process = await asyncio.create_subprocess_shell(
            "/cs165/src/client",
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        await process.communicate(input=dsl.encode('utf-8'))
        return time.time() - start
