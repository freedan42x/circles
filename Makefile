main: Makefile main.cpp
	g++ -ggdb -Wall -Wextra -pedantic -Wno-missing-field-initializers -std=c++20 main.cpp -o main -Iraylib/src -Lraylib/src -l:libraylib.a -lgdi32 -lwinmm
