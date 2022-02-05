OBJS = $(subst .cpp,.o,$(wildcard *.cpp))

OUTPUT = ctrcdnfetch_debug
LIBS = -lcurl -lssl -lcrypto -lz
CXXFLAGS = -g -Wall -std=c++11
CXX = g++
STRIP = strip
CP = cp
ZIP = zip

all: $(OBJS)
	$(CXX) -o "bin/$(OUTPUT)" $(OBJS) $(LIBS)

release: all
	$(CP) "bin/$(OUTPUT)" "bin/ctrcdnfetch"
	$(STRIP) "bin/ctrcdnfetch"
	$(ZIP) "bin/ctrcdnfetch.zip" "bin/$(OUTPUT)" "bin/ctrcdnfetch"

clean:
	rm  "bin/$(OUTPUT)"* $(OBJS) -rf
