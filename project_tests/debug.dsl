create(db,"db1")

create(tbl,"tbl5_fact",db1,4)
create(col,"col1",db1.tbl5_fact)
create(col,"col2",db1.tbl5_fact)
create(col,"col3",db1.tbl5_fact)
create(col,"col4",db1.tbl5_fact)
load("/cs165/local_test_inputs/data5_fact.csv")

create(tbl,"tbl5_dim1",db1,3)
create(col,"col1",db1.tbl5_dim1)
create(col,"col2",db1.tbl5_dim1)
create(col,"col3",db1.tbl5_dim1)
load("/cs165/local_test_inputs/data5_dimension1.csv")

create(tbl,"tbl5_dim2",db1,2)
create(col,"col1",db1.tbl5_dim2)
create(col,"col2",db1.tbl5_dim2)
load("/cs165/local_test_inputs/data5_dimension2.csv")

create(tbl,"tbl5_sel1",db1,2)
create(col,"col1",db1.tbl5_sel1)
create(col,"col2",db1.tbl5_sel1)
load("/cs165/local_test_inputs/data5_selectivity1.csv")

create(tbl,"tbl5_sel2",db1,2)
create(col,"col1",db1.tbl5_sel2)
create(col,"col2",db1.tbl5_sel2)
load("/cs165/local_test_inputs/data5_selectivity2.csv")


-- END LOAD

-- p1=select(db1.tbl5_fact.col2,null, 80000)
-- p2=select(db1.tbl5_dim1.col3,null, 1600)
-- f1=fetch(db1.tbl5_fact.col1,p1)
-- f2=fetch(db1.tbl5_dim1.col1,p2)
-- t1,t2=join(f1,p1,f2,p2,hash)
-- col2joined=fetch(db1.tbl5_fact.col2,t1)
-- col1joined=fetch(db1.tbl5_dim1.col1,t2)
-- a1=sum(col2joined)
-- a2=avg(col1joined)
-- print(a1,a2)

shutdown
