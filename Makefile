
CC=g++
CFLAGS=-std=c++11 -Wall -O3
LDFLAGS=-pthread
EXECUTABLES=mccleaner mcinspector
CLEANER_OBJS=common.o mc_cleaner.o
INSPECTOR_OBJS=common.o expired_item_dumper.o file_dumper.o item_aggregator.o item_dumper.o item_processor.o mc_inspector.o 

all: $(EXECUTABLES)

%.o: %.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $^

mccleaner: $(CLEANER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

mcinspector: $(INSPECTOR_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -rf $(EXECUTABLES) $(CLEANER_OBJS) $(INSPECTOR_OBJS)

rebuild: clean all

