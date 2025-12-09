CC ?= gcc
AR ?= ar
SQLITE_TOP ?= ../..
SQLITE_SRC ?= $(SQLITE_TOP)/sqlite3.c
SQLITE_OBJ ?= sqlite3_amalgam.o
BASE_CFLAGS ?= -std=c99 -Wall -Wextra -Iinclude -I$(SQLITE_TOP) -I$(SQLITE_TOP)/src -DSQLITE_CORE \
  -DSQLITE_THREADSAFE=1
SQLITE_OMIT_FLAGS ?= -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_DEPRECATED \
  -DSQLITE_OMIT_TRACE -DSQLITE_OMIT_PROGRESS_CALLBACK -DSQLITE_OMIT_SHARED_CACHE \
  -DSQLITE_OMIT_AUTOMATIC_INDEX -DSQLITE_OMIT_COMPILEOPTION_DIAGS -DSQLITE_DEFAULT_MEMSTATUS=0
STORAGE_CFLAGS ?= $(BASE_CFLAGS) $(SQLITE_OMIT_FLAGS) -DSQLITE_PRIVATE=
SQLITE3_CFLAGS ?= $(BASE_CFLAGS) $(SQLITE_OMIT_FLAGS) -DSQLITE_PRIVATE=

SRCS = storage_api.c record_codec.c schema_service.c
OBJS = $(SRCS:.c=.o)
LIB = libstorage_api.a

.PHONY: all clean

all: $(LIB) $(SQLITE_OBJ) kv_demo

$(LIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)

kv_demo: examples/kv_demo.o $(LIB) $(SQLITE_OBJ)
	$(CC) $(STORAGE_CFLAGS) -o $@ $^ -lm -lpthread -ldl

$(SQLITE_OBJ): $(SQLITE_SRC)
	$(CC) $(SQLITE3_CFLAGS) -c $< -o $@

%.o: %.c include/storage.h
	$(CC) $(STORAGE_CFLAGS) -c $< -o $@

examples/%.o: examples/%.c include/storage.h
	$(CC) $(STORAGE_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIB) $(SQLITE_OBJ) examples/*.o kv_demo
