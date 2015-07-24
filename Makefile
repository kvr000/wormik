PREFIX ?= /usr/

VERSION=2.0

CC=gcc
CXX=g++

#ADV=-DTESTOPTS

LIBS=-lSDL2 -lSDL2_image -lSDL2_ttf
CFLAGS=-DSVERSION=\"2.0\" -DNDEBUG -Isrc/main/cxx/ -Wall -O2 $(ACFLAGS) -fmessage-length=0 -g
LDFLAGS=$(LIBS) -g
#CFLAGS=-Wall -D_GNU_SOURCE -g
#LDFLAGS=-lpng -L/usr/X11R6/lib -lX11 -g
RESOURCES=target/wormik_0.png target/wormik_1.png target/wormik_2.png target/wormik_3.png target/README target/LICENSE
TARGET=target/wormik target/wormik_0.png

SOURCES= \
	src/main/cxx/cz/znj/sw/wormik/main.cxx \
	src/main/cxx/cz/znj/sw/wormik/WormikGameImpl.cxx \
	src/main/cxx/cz/znj/sw/wormik/gui_common.cxx \
	src/main/cxx/cz/znj/sw/wormik/SdlWormikGui.cxx \

OBJECTS= \
	target/object/cz/znj/sw/wormik/main.o \
	target/object/cz/znj/sw/wormik/WormikGameImpl.o \
	target/object/cz/znj/sw/wormik/SdlWormikGui.o \
	target/object/cz/znj/sw/wormik/gui_common.o \

default: $(TARGET) $(RESOURCES)

run: r$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS)

no_tags:
	rm -f tags
tags: no_tags
	ctags -R

tar:
	p=`pwd` && b=`basename $$p` && cd .. && tar fcv - $$b/src/ $$b/Makefile $$b/LICENSE $$b/PORTS $$b/README | gzip -9 >$$b/target/wormik-$(VERSION).tar.gz

btar:
	cd target/ && tar fcv - README wormik wormik_?.png | gzip -9 > wormik-$(VERSION)-`uname -s`-`uname -i`.tar.gz

install:
	cd target/ && for f in wormik_?.png; do mkdir -p $(PREFIX)/share/wormik && cp $$f $(PREFIX)/share/wormik/ || break; done
	cd target/ && for f in wormik; do cp $$f $(PREFIX)/bin/ || break; done

target/wormik: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)
	echo "xyz $(CFLAGS)" | grep -- -O0 >/dev/null || strip $@

target/object/cz/znj/sw/wormik/main.o: src/main/cxx/cz/znj/sw/wormik/main.cxx
	[ -d `dirname $@` ] || mkdir -p `dirname $@`
	$(CXX) -o $@ -c $< $(CFLAGS)
target/object/cz/znj/sw/wormik/WormikGameImpl.o: src/main/cxx/cz/znj/sw/wormik/WormikGameImpl.cxx
	[ -d `dirname $@` ] || mkdir -p `dirname $@`
	$(CXX) -o $@ -c $< $(CFLAGS)
target/object/cz/znj/sw/wormik/gui_common.o: src/main/cxx/cz/znj/sw/wormik/gui_common.cxx
	[ -d `dirname $@` ] || mkdir -p `dirname $@`
	$(CXX) -o $@ -c $< $(CFLAGS)
target/object/cz/znj/sw/wormik/SdlWormikGui.o: src/main/cxx/cz/znj/sw/wormik/SdlWormikGui.cxx
	[ -d `dirname $@` ] || mkdir -p `dirname $@`
	$(CXX) -o $@ -c $< $(CFLAGS)

target/wormik_0.png: src/main/resources/wormik_0.png
	cp -a $< $@
target/wormik_1.png: src/main/resources/wormik_1.png
	cp -a $< $@
target/wormik_2.png: src/main/resources/wormik_2.png
	cp -a $< $@
target/wormik_3.png: src/main/resources/wormik_3.png
	cp -a $< $@
target/README: README
	cp -a $< $@
target/LICENSE: LICENSE
	cp -a $< $@

depends:
	$(CXX) -MM $(SOURCES) >target/.depends

target/.depends:
	[ -d target ] || mkdir target
	touch target/.depends
	$(MAKE) depends

%.o: %.cxx
	$(CXX) -o $@ -c $< $(CFLAGS)
%.s: %.cxx
	$(CC) -o $@ -S $< $(CFLAGS)

include target/.depends
