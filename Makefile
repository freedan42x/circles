main: Makefile main.cpp
	g++ -ggdb -O0 main.cpp -o main -Iraylib/src -Lraylib/src -l:libraylib.a -lgdi32 -lwinmm
