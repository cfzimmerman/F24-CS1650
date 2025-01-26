from subprocess import run
from exp import Exp
import sys
from gen import INDEX_TBL_LEN
from typing import Optional

'''
Loads db tables of varying sizes and then shuts down.
Tables are ready for queries on next startup.
'''


def setup_table(col_len: int, index=Optional[str]) -> str:
    tbl_name = f"tbl{col_len}"

    idx = ""
    if index is not None:
        idx = f"create(idx, db.{tbl_name}.col0, {index}, clustered)"

    return f'''
    create(tbl, {tbl_name}, db, 2)
    create(col, "col0", db.{tbl_name})
    create(col, "col1", db.{tbl_name})
    {idx}
    load("/cs165/experiments/csvs/rng-{col_len}.csv")
    '''


def default(index=None):
    run(["cd /cs165/src && make clean && make distclean && make all"],
        shell=True)
    tbls = "\n".join(setup_table(col_len, index=index)
                     for col_len in Exp.default_lens())
    load_args = f'''
    create(db, "db")
    {tbls}
    shutdown
    '''
    print(load_args)

    server = Exp.run_server()
    print(f"loaded in {Exp.run_client(load_args)} secs")

    code = server.wait()
    if code != 0:
        print(f"server exit code: {code}")


def index1():
    run(["cd /cs165/src && make clean && make distclean && make all"],
        shell=True)

    tbl_name = Exp.table_name(INDEX_TBL_LEN)
    load_args = f'''
    create(db, "db")

    create(tbl, "{tbl_name}", db, 2)

    create(col, "col0", db.{tbl_name})
    create(col, "col1", db.{tbl_name})
    create(col, "col2", db.{tbl_name})

    create(idx, db.{tbl_name}.col0, sorted, clustered)
    create(idx, db.{tbl_name}.col1, sorted, unclustered)
    create(idx, db.{tbl_name}.col2, btree, unclustered)

    load("/cs165/experiments/csvs/index-{INDEX_TBL_LEN}.csv")

    shutdown
    '''

    server = Exp.run_server()
    print(f"loaded in {Exp.run_client(load_args)} secs")

    code = server.wait()
    if code != 0:
        print(f"server exit code: {code}")


if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else None
    if arg == "default":
        default()
    elif arg == "secondary":
        index1()
    elif arg == "default-sorted":
        default(index="sorted")
    elif arg == "default-btree":
        default(index="btree")
    else:
        print("*.py [default | secondary | default-sorted | default-btree]")
