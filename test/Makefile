CXXFLAGS=-W -Wall

BINARIES=basic withsignals auto

all: $(BINARIES)

clean:
	rm -f $(BINARIES)

%: %.cpp ../src/libsingleinstance.so
	$(CXX) $(CXXFLAGS) -I../src -o $@ $< ../src/libsingleinstance.so

../src/libsingleinstance.so:
	$(MAKE) -C ../src/ $@
