LIBNAME=libcampfire
.PHONY: all

all: $(LIBNAME)
 

C_SRCS:= campfire_im.o \
  campfire.c

# Object file names using 'Substitution Reference'
C_OBJS:=$(C_SRCS:.c=.o)

PURPLE_LIBS = $(shell pkg-config --libs purple)
CC:=gcc
LD:=$(CC)
CFLAGS=-DPURPLE_PLUGINS -DPIC -DENABLE_NLS
CFLAGS+=$(shell pkg-config --cflags purple)
#CFLAGS+=$(shell pkg-config --cflags pidgin)
CFLAGS+=-Wall -fPIC
LDFLAGS=$(CFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME): $(C_OBJS)
	$(LD) $(LDFLAGS) -shared $^ $(PURPLE_LIBS) -o $@

.PHONY: clean

clean:
	-rm *.o
	-rm $(LIBNAME)
