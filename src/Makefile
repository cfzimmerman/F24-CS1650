# CS165 MakeFile
# This file includes automatic dependency tracking
# Please see example of "utils" to see how to add additional file
# to your project
#
# Tags: git tag milestoneX-submit -f && git push --tags -f

all: client server

# C-compiler settings
# CC = gcc -std=c99 -g -ggdb3

CC = gcc -std=c11 -g -ggdb3

# Default optimization level
O ?= 3

# Flags and other libraries
override CFLAGS += -Wall -Wextra -pedantic -pthread -O$(O) -I$(INCLUDES)
LDFLAGS =
LIBS = -lm
INCLUDES = include


# this is the DB server and client's unix socket path
# Note: the extra backslash is for escaping the quotes for makefile format
SOCK_PATH=\"/tmp/cs165_unix_socket\"

####### Automatic dependency magic #######
# Set-up dependency directory
DEPSDIR := .deps
BUILDSTAMP := $(DEPSDIR)/rebuildstamp
DEPFILES := $(wildcard $(DEPSDIR)/*.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif
DEPCFLAGS = -MD -MF $(DEPSDIR)/$*.d -MP -DSOCK_PATH=$(SOCK_PATH)

# Dependency compilation
ifneq ($(DEP_CC),$(CC) $(CFLAGS) $(DEPCFLAGS) $(O))
DEP_CC := $(shell mkdir -p $(DEPSDIR); echo >$(BUILDSTAMP); echo "DEP_CC:=$(CC) $(CFLAGS) $(DEPCFLAGS) $(O)" >$(DEPSDIR)/_cc.d)
endif

# Make sure dependency directories are generated
$(DEPSDIR)/stamp $(BUILDSTAMP):
	mkdir -p $(@D)
	touch $@

####### Automatic dependency magic #######

%.o : %.c $(BUILDSTAMP)
	$(CC) $(CFLAGS) $(DEPCFLAGS) -O$(O) -o $@ -c $<

##
# To include additional non-executable files (e.g. selects.c, utils.c, etc),
# you'll need to add an additional build dependency to the file that requires
# the new file.  For example, see that client and server both require utils.o
#
# If you create a new file such as selects.c, then you will need a "selects.o"
# dependency on the right side of whichever one requires the file.
##

client: client.o utils.o vector.o client_tools.o parse2.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

server: server.o utils.o client_context.o catalogue.o json_parse.o \
	vector.o parse2.o operators.o op_joins.o op_selects.o bitvec.o queue.o channel.o threadpool.o \
	sorting.o bptree.o bptree2.o hash_table.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_vector: test_vector.o vector.o utils.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_parse: test_parse.o parse2.o vector.o utils.o client_context.o utils.o bitvec.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_catalogue: test_operators.o operators.o op_joins.o op_selects.o catalogue.o utils.o json_parse.o \
	vector.o bitvec.o client_context.o threadpool.o channel.o queue.o sorting.o \
	bptree.o bptree2.o hash_table.o 
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_json_parse: test_json_parse.o json_parse.o vector.o utils.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_operators: test_operators.o operators.o op_joins.o op_selects.o catalogue.o utils.o json_parse.o \
	vector.o bitvec.o client_context.o threadpool.o channel.o queue.o sorting.o \
	bptree.o bptree2.o hash_table.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_client_tools: test_client_tools.o client_tools.o utils.o vector.o parse2.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_queue: test_queue.o queue.o utils.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_channel: test_channel.o channel.o queue.o utils.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_threadpool: test_threadpool.o threadpool.o channel.o queue.o utils.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_sorting: test_sorting.o sorting.o utils.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_btree: test_btree.o btree.o utils.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_bptree: test_bptree.o bptree.o utils.o sorting.o utils.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_bptree2: test_bptree2.o bptree2.o utils.o sorting.o utils.o vector.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_hash_table: test_hash_table.o hash_table.o vector.o utils.o 
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test: test_vector test_parse test_catalogue \
	test_json_parse test_operators test_client_tools \
	test_queue test_channel test_threadpool test_sorting \
	test_btree test_bptree test_bptree2 test_hash_table
	./test_vector && ./test_parse && ./test_catalogue && \
		./test_json_parse && ./test_operators && ./test_client_tools && \
		./test_queue && ./test_channel && ./test_threadpool && \
		./test_sorting && ./test_btree && ./test_bptree && \
		./test_bptree2 && ./test_hash_table 

perf_bptree: perf_bptree.o bptree.o bptree2.o utils.o sorting.o utils.o vector.o
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f client server \
		test_vector test_parse test_catalogue test_json_parse \
		test_operators test_client_tools test_queue test_channel \
		test_threadpool test_sorting test_btree test_bptree \
		test_bptree2 test_hash_table \
		perf_bptree \
		*.o *~ *.bak core *.core $(SOCK_PATH)
	rm -rf .deps

distclean: clean
	rm -rf $(DEPSDIR) ../db

.PHONY: all clean distclean
