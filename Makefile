OBJS = $(subst .cpp,.o,$(wildcard *.cpp))

OUTPUT = ctrcdnfetch
LIBS = -lcurl -lssl -lcrypto -lz
CXXFLAGS = -g -Wall -std=c++11
CXX = g++

all: $(OBJS)
	mkdir -p "bin" -v
	$(CXX) -o "bin/$(OUTPUT)" $(OBJS) $(LIBS)

release: CXXFLAGS += -O3
release: all
	strip "bin/ctrcdnfetch" -v
	zip "bin/ctrcdnfetch.zip" "bin/$(OUTPUT)" -vj

debug_release: all
	cp "bin/$(OUTPUT)" "bin/$(OUTPUT)_debug" -v
	zip "bin/$(OUTPUT)_debug.zip" "bin/$(OUTPUT)_debug" -vj

clean:
	rm $(OBJS) -rfv
