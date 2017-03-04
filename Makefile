test: test.cpp game.h
	clang++ -std=c++11 -stdlib=libc++ test.cpp -o test -O3

wood: wood.cpp game.h
	clang++ -std=c++11 -stdlib=libc++ wood.cpp -o wood -g
