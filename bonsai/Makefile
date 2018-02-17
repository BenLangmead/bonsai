.PHONY=all tests clean obj
CXX=g++
CC=gcc

GMATCH=$(findstring g++,$(CXX))

CLHASH_CHECKOUT = "&& git checkout master"
WARNINGS=-Wall -Wextra -Wno-char-subscripts \
		 -Wpointer-arith -Wwrite-strings -Wdisabled-optimization \
		 -Wformat -Wcast-align -Wno-unused-function -Wno-unused-parameter \
		 -pedantic -DUSE_PDQSORT -Wunused-variable \
		-Wduplicated-branches -Wdangling-else  # -Wconversion
ifndef EXTRA
	EXTRA:=
endif
ifndef INCPLUS
	INCPLUS:=
endif
ifndef EXTRA_LD
	EXTRA_LD:=
endif
DBG:=
OS:=$(shell uname)
FLAGS=

OPT:= -O3 -funroll-loops \
	  -fopenmp \
	  -pipe -fno-strict-aliasing -march=native -mpclmul $(FLAGS) $(EXTRA) -DHLL_HEADER_ONLY
XXFLAGS=-fno-rtti
CXXFLAGS=$(OPT) $(XXFLAGS) -std=c++17 $(WARNINGS)
CCFLAGS=$(OPT) $(CFLAGS) -std=c11 $(WARNINGS)
LIB=-lz
LD=-L. $(EXTRA_LD)

OBJS=$(patsubst %.c,%.o,$(wildcard src/*.c) ../klib/kthread.o) $(patsubst %.cpp,%.o,$(wildcard src/*.cpp)) ../klib/kstring.o clhash.o libzstd.a
DOBJS=$(patsubst %.c,%.do,$(wildcard src/*.c) ../klib/kthread.o) $(patsubst %.cpp,%.do,$(wildcard src/*.cpp)) ../klib/kstring.o clhash.o
ZOBJS=$(patsubst %.c,%.zo,$(wildcard src/*.c) ../klib/kthread.o) $(patsubst %.cpp,%.zo,$(wildcard src/*.cpp)) ../klib/kstring.o clhash.o

TEST_OBJS=$(patsubst %.cpp,%.o,$(wildcard test/*.cpp))

EXEC_OBJS=$(patsubst %.cpp,%.o,$(wildcard bin/*.cpp))
ZW_OBJS=$(patsubst %.c,%.o,$(wildcard ../zstd/zlibWrapper/*.c))

EX=$(patsubst bin/%.cpp,%,$(wildcard bin/*.cpp))
DEX=$(patsubst %_d,%,$(EX))

ZFLAGS=-DZWRAP_USE_ZSTD=1
ZCOMPILE_FLAGS= $(ZFLAGS) -lzstd
ALL_ZOBJS=$(ZOBJS) $(ZW_OBJS)

INCLUDE=-Iclhash/include -I. -I.. -Iinclude -I../zstd/zlibWrapper -I../zstd/lib -I../zstd/lib/common $(INCPLUS)

all: $(OBJS) $(EX) $(ZW_OBJS) unit lib/libbonsai.a

#libhll.a:
#	cd hll && make CXX=$(CXX) && cp libhll.a ..

obj: $(OBJS) $(DOBJS) $(ZOBJS) $(ZW_OBJS)

libzstd.a:
	cd ../zstd && make && mv lib/libzstd.a ../bonsai && cd ../bonsai

clhash.o: ../clhash/src/clhash.c
	ls $@ 2>/dev/null || mv ../clhash/clhash.o . 2>/dev/null || (cd ../clhash $(CLHASH_CHECKOUT) && make && cd ../bonsai && ln -s ../clhash/clhash.o .)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -DNDEBUG -c $< -o $@ $(LIB)

%.do: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@ $(LIB)

%.zo: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@ $(LIB)

test/%.o: test/%.cpp
	$(CXX) $(CXXFLAGS) $(DBG) $(INCLUDE) $(LD) -c $< -o $@ $(LIB)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DBG) $(INCLUDE) $(LD) -DNDEBUG -c $< -o $@ $(LIB)

%.do: %.cpp
	$(CXX) $(CXXFLAGS) $(DBG) $(INCLUDE) $(LD) -DNDEBUG=0 -c $< -o $@ $(LIB)

%.zo: %.cpp
	$(CXX) $(CXXFLAGS) $(DBG) $(INCLUDE) $(LD) $(ZFLAGS) -DNDEBUG=0 -c $< -o $@ $(LIB)

%: bin/%.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(DBG) $(INCLUDE) $(LD) $(OBJS) -DNDEBUG $< -o $@ $(LIB)

%_z: bin/%.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(DBG) $(INCLUDE) $(LD) $(ALL_ZOBJS) -DNDEBUG $< -o $@ $(ZCOMPILE_FLAGS) $(LIB)

%_d: bin/%.cpp $(DOBJS)
	$(CXX) $(CXXFLAGS) $(DBG) $(INCLUDE) $(LD) $(DOBJS) -DNDEBUG=0 $< -o $@ $(LIB)

bonsai/fahist: src/fahist.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(DBG) $(INCLUDE) $(LD) ../klib/kthread.o $< -o $@ -lz

lib/libbonsai.a: $(OBJS)
	ar cr $@ $(OBJS)

tests: clean unit

unit: $(OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(TEST_OBJS) $(LD) $(OBJS) -o $@ $(LIB)
	# $(CXX) $(CXXFLAGS) $(DBG) $(INCLUDE) test/hll.o test/main.o $(LD) $(OBJS) -o $@ $(LIB)


clean:
	rm -f $(OBJS) $(DEX) $(EX) $(TEST_OBJS) $(DOBJS) unit lib/*o src/*o \
	&& cd ../hll && make clean && cd ../bonsai

mostlyclean:
	rm -f $(DEX) $(DOBJS) $(OBJS) $(EX) $(TEST_OBJS) unit lib/*o src/*o