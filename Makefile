CXX = g++
CXXFLAGS = -g -std=c++1y

all: mm5test

mm5test: mm5nsftest.o mm5sound.o
	$(CXX) mm5nsftest.o mm5sound.o -o mm5test

mm5nsftest.o: mm5nsftest.cpp
	$(CXX) $(CXXFLAGS) -c mm5nsftest.cpp

mm5sound.o: mm5sound.cpp mm5sound.h mm5sndrom.h mm5constants.h chain_int.h
	$(CXX) $(CXXFLAGS) -c mm5sound.cpp

mm5sndrom.h: make_rom_image.lua mm5.nes
	lua make_rom_image.lua mm5.nes mm5sndrom.h

clean:
	rm -f *.o
	rm -f mm5test
	rm -f mm5test.exe

#mm5-sound.s: mm5.cfg mm5.nes
#	da65 -i mm5.cfg
