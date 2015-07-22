PREFIX?=/usr/local
LIB_PREFIX=$(PREFIX)/lib
INCLUDE_PREFIX=$(PREFIX)/include
MAN_PREFIX=$(PREFIX)/share/man

all:
	$(MAKE) -C src
	$(MAKE) -C test

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean
	rm -rf doxygen.generated doc/html doc/man

doc: doxygen.generated

doxygen.generated: doc/doxygen doc/mainpage.dox src/singleinstance.h
	doxygen doc/doxygen
	touch doxygen.generated

install:
	install -m 644 -D src/libsingleinstance.so $(LIB_PREFIX)/libsingleinstance.so

install-dev: install-headers install-doc

install-doc:
	install -d $(MAN_PREFIX)/man3
	install -m 644 -t $(MAN_PREFIX)/man3 doc/man/man3/{singleInstance,SingleInstance,LibSingleInstance}*

install-headers:
	install -m 644 -D src/singleinstance.h $(INCLUDE_PREFIX)/singleinstance.h

uninstall:
	rm -f $(LIB_PREFIX)/libsingleinstance.so
	rm -f $(MAN_PREFIX)/man3/{singleInstance,SingleInstance,LibSingleInstance}*
	rm -f $(INCLUDE_PREFIX)/singleinstance.h

.PHONY:
	all clean doc install install-dev install-doc install-headers
