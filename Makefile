test: test.cpp game.h
	clang++ -std=c++11 -stdlib=libc++ test.cpp -o test -g
