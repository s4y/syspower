syspower: syspower.cpp Makefile
	c++ -std=c++14 -framework IOKit -Os -o $@ $<
