# DSL Modifications

I made changes to what the _server_ sees for certain commands.

### Load

The client parses a single input CSV into smaller chunks that don't overload network transmission. The server can take an arbitrary number of loading commands, and it will
only rebuild indexes when the rebuild_indexes(db.table) command is received.

**Client receives**

```
load("/path/to/myfile.txt")
```

**Server receives**

```
load(db.tbl.col1,db.tbl.col2,db.tbl.col3\n1,2,3\n...)
...
load(db.tbl.col1,db.tbl.col2,db.tbl.col3\n10000000,20000000,30000000\n...)
rebuild_indexes(db.tbl)
```

### Batched commands

The client buffers batched selection operations into a single `batch_select` operation, which the server receives. This assumes/enforces (1) batched operations are always selections and (2) batched selections always operate on the same column.

**Client receives**

```
batch_queries()
s1=select(DB.TBL.COL,min1,max1)
s2=select(DB.TBL.COL,min2,max2)
s3=select(DB.TBL.COL,min3,max3)
...
batch_execute()
```

**Server receives**

```
batch_select(DB.TBL.COL,s1,min1,max1,s2,min2,max2,s3,min3,max3...)
```
