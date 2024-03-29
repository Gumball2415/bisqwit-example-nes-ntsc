# Create the Make rules for ntsc-small
#
CXX=g++
CXXFLAGS=-Wall -W -pedantic -Ofast -std=c++0x

ntsc-small: ntsc-small.o
	$(CXX) -o "$@" "$<" $(CXXFLAGS)

ntsc-small.o: ntsc-small.cc
	$(CXX) -c -o "$@" "$<" $(CXXFLAGS)