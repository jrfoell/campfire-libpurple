LIBNAME=libcampfire.so
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
LDFLAGS=-shared -Wl

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIBNAME): $(C_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(PURPLE_LIBS)

.PHONY: install
install: $(LIBNAME)
	[ -d ~/.purple/plugins ] || mkdir -p ~/.purple/plugins
	cp $(LIBNAME) ~/.purple/plugins
	
.PHONY: clean

clean:
	-rm *.o
	-rm $(LIBNAME)
