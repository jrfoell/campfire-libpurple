LIBNAME=libcampfire.so
.PHONY: all

all: $(LIBNAME)
 

C_SRCS:= campfire.c \
  message.c

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

PIDGIN:=$(shell which pidgin)
PIDGIN_BIN_DIR:=$(strip $(shell dirname $(PIDGIN)))
PREFIX=$(strip $(PIDGIN_BIN_DIR)/..)
PIDGIN_PLUGIN_DIR:=$(PREFIX)/lib/purple-2

.PHONY: install
install: $(LIBNAME)
	install -D $(LIBNAME) $(PIDGIN_PLUGIN_DIR)/$(LIBNAME)
	install --mode=0644 campfire16.png $(PREFIX)/share/pixmaps/pidgin/protocols/16/campfire.png
	install --mode=0644 campfire22.png $(PREFIX)/share/pixmaps/pidgin/protocols/22/campfire.png
	install --mode=0644 campfire48.png $(PREFIX)/share/pixmaps/pidgin/protocols/48/campfire.png
	
.PHONY: uninstall
uninstall: $(LIBNAME)
	rm $(PIDGIN_PLUGIN_DIR)/$(LIBNAME)
	rm $(PREFIX)/share/pixmaps/pidgin/protocols/16/campfire.png
	
.PHONY: clean

clean:
	-rm *.o
	-rm $(LIBNAME)
