CC=gcc
CXX=g++

#ADV=-DTESTOPTS

LIBS=-lSDL -lSDL_image -lSDL_ttf
CFLAGS=-DSVERSION=\"2.0\" -DNDEBUG -Wall -O2 $(ACFLAGS) -fmessage-length=0 -g
LDFLAGS=$(LIBS) -g
#CFLAGS=-Wall -D_GNU_SOURCE -g
#LDFLAGS=-lpng -L/usr/X11R6/lib -lX11 -g
TARG=wormik

SOURCES= \
	main.cxx \
	WormikGame.cxx \
	gui_common.cxx \
	Wormik_SDL.cxx \

OBJECTS= \
	 main.o \
	 WormikGame.o \
	 Wormik_SDL.o \
	 gui_common.o \

default: $(TARG)

run: r$(TARG)

clean:
	rm -f $(TARG) $(OBJECTS)

no_tags:
	rm -f tags
tags: no_tags
	ctags -R

tar:
	p=`pwd` && b=`basename $$p` && cd .. && tar fcv - $$b/*.cxx $$b/*.h $$b/port* $$b/Makefile $$b/PORTS $$b/README $$b/wormik_?.png | gzip -9 >$$b/wormik.tar.gz

btar:
	p=`pwd` && b=`basename $$p` && cd .. && tar fcv - $$b/{README,wormik,wormik_?.png} | gzip -9 >$$b/wormik-bin.tar.gz

wormik: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)
	echo $(CFLAGS) | grep -- -O0 >/dev/null || strip $@

depends:
	$(CXX) -MM $(SOURCES) >.depends

.depends:
	touch .depends
	$(MAKE) depends

%.o: %.cxx
	$(CXX) -o $@ -c $< $(CFLAGS)
%.s: %.cxx
	$(CC) -o $@ -S $< $(CFLAGS)

include .depends
