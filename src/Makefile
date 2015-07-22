
CXXFLAGS=-W -Wall
PLATFORM?=posix

ifneq (,$(findstring mingw, $(CXX)))
	PLATFORM=windows
endif

ifeq ($(PLATFORM), posix)
	LIBSUFFIX=.so
	LIBFLAGS=-shared -fPIC
	LIBS=-pthread
endif

ifeq ($(PLATFORM), windows)
	LIBSUFFIX=.dll
endif

all: libsingleinstance$(LIBSUFFIX)

clean:
	rm -f libsingleinstance.so

libsingleinstance$(LIBSUFFIX): singleinstance-$(PLATFORM).cpp singleinstance.h
	$(CXX) $(CXXFLAGS) $(LIBFLAGS) -o $@ $< $(LIBS)

.PHONY: all clean
