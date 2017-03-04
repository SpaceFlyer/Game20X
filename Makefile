test: test.cpp game.h
	clang++ -std=c++11 -stdlib=libc++ test.cpp -o test -O3

wood: game.h wood.h concat
	clang++ -std=c++11 -stdlib=libc++ wood.cpp -o wood -g

woodc: game.h wood.h woodc.cpp
	clang++ -std=c++11 -stdlib=libc++ woodc.cpp -o woodc -g

concat: wood.h game.h
	cat game.h wood.h > wood.cpp
