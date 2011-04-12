.PHONY: all

all: libcampfire
 
PURPLE_LIBS = $(shell pkg-config --libs purple)

CC:=gcc
LD:=$(CC)
CFLAGS=-DPURPLE_PLUGINS
CFLAGS+=$(shell pkg-config --cflags purple)
#CFLAGS+=$(shell pkg-config --cflags pidgin)
CFLAGS+=-Wall
LDFLAGS=$(CFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

libcampfire: campfire_im.o
	$(LD) $(LDFLAGS) -shared $< $(PURPLE_LIBS) -o $@
