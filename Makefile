CC = gcc

#for release version, use -O/2 or other parameters for optimization!
FLAG1 = -O2 -Wmissing-declarations
#FLAG1 = -g

SHELL = /bin/sh

AWK = awk

BFLAG = -DRELEASE -D_GNU_SOURCE

LIB = -lpthread -lrt

srcdir=$(shell pwd)

MKPROTO_SH = $(srcdir)/mkproto.sh

INCLUDE = ./include

MEMOBJ = lib/mempool.o lib/link_mempool.o lib/lock.o

TESTOBJ = lib/test.o

PICOBJS = $(MEMOBJ:.o=.po)

PROTOOBJ = $(MEMOBJ)

CFLAGS = $(FLAG1)

LDSHFLAGS = -shared -Wl,-Bsymbolic

LIBTAGET = ./libmempool.so

TESTTAGET = ./mem_test

LIBVERSION = 0

##############################################################################################
.SUFFIXES:
.SUFFIXES: .c .o .po

.c.o:
	@echo Compiling $*.c to $*.o
	@$(CC) -I$(INCLUDE) $(CFLAGS) $(BFLAG) -c $< -o $*.o

.c.po:
	@echo Compiling $*.c with -fPIC
	@$(CC) -I$(INCLUDE) $(CFLAGS) $(BFLAG) -fPIC -c $< -o $*.po

$(TESTTAGET): $(MEMOBJ) $(TESTOBJ)
	@$(CC) $(CFLAGS) -o $@ $(MEMOBJ) $(TESTOBJ) $(LIB)

#$(LIBTAGET): proto $(PICOBJS)
#	@$(CC) $(CFLAGS) $(LDSHFLAGS) -o $@ $(PICOBJS) $(LIB)	\
#		-Wl,-soname=`basename $@`.$(LIBVERSION)
#	ln -snf $(LIBTAGET) $(LIBTAGET).$(LIBVERSION)

$(LIBTAGET): $(PICOBJS)
	@$(CC) $(CFLAGS) $(LDSHFLAGS) -o $@ $(PICOBJS) $(LIB)

#target proto could generate all the function prototype into proto.h
proto:
	$(SHELL) $(MKPROTO_SH) $(AWK) -h _PROTO_H $(INCLUDE)/proto.h $(PROTOOBJ)

test: $(TESTTAGET)

lib: $(LIBTAGET)
	
clean:
	@rm -f *.core *.[ps]o */*.o */*.[ps]o $(TESTTAGET)
	
