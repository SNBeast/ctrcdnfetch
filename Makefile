OBJS = $(subst .cpp,.o,$(wildcard *.cpp))

OUTPUT = ctrcdnfetch
LIBS = -lcurl -lssl -lcrypto -lz
CXXFLAGS = -g -Wall -std=c++11
CXX = g++
STRIP = strip

#wildcard on strip and rm because if it's a g++ for windows, it will add .exe

all: $(OBJS)
	$(CXX) -o "bin/$(OUTPUT)" $(OBJS) $(LIBS)

clean:
	rm -rf "bin/$(OUTPUT)"* $(OBJS)
