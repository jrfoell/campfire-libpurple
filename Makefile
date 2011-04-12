.PHONY: all

all: libcampfire
 
#PURPLE_LIBS = $(shell pkg-config --libs purple)
CC:=gcc
CFLAGS=-DPURPLE_PLUGINS
CFLAGS+=$(shell pkg-config --cflags purple)
CFLAGS+=$(shell pkg-config --cflags pidgin)
CFLAGS+=-Wall -pthread -I. -g -pipe -fPIC -DPIC

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

libcampfire: campfire_im.o
	$(LD) $(LDFLAGS) -shared $< $(PURPLE_LIBS) -o $@
