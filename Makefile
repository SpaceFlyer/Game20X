test: test.cpp game.h xnd.h
	clang++ -std=c++11 -stdlib=libc++ test.cpp -o test

wood: game.h wood.h concatwood
	mv wood.out wood.cpp; clang++ -std=c++11 -stdlib=libc++ wood.cpp -o wood -g; mv wood.cpp wood.out

woodc: game.h wood.h woodc.cpp
	clang++ -std=c++11 -stdlib=libc++ woodc.cpp -o woodc -g

concatwood: wood.h game.h
	cat game.h wood.h > wood.out

bronze: game.h bronze.h xnd.h concatbronze
	mv bronze.out bronze.cpp; clang++ -DTEST -std=c++11 -stdlib=libc++ bronze.cpp -o bronze -g; mv bronze.cpp bronze.out

bronzec: game.h bronze.h xnd.h bronzec.cpp
	clang++ -std=c++11 -stdlib=libc++ bronzec.cpp -o bronzec -g

concatbronze: bronze.h game.h xnd.h
	cat game.h xnd.h bronze.h > bronze.out
