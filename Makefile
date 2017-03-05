CC=g++
STDLIB=
MACSTDLIB=-stdlib=libc++
test: test.cpp game.h xnd.h
	$(CC) -std=c++11 $(STDLIB) test.cpp -o test

wood: game.h wood.h concatwood
	mv wood.out wood.cpp; clang++ -std=c++11 $(STDLIB) wood.cpp -o wood -g; mv wood.cpp wood.out

woodc: game.h wood.h woodc.cpp
	$(CC) -std=c++11 $(STDLIB) woodc.cpp -o woodc -g

concatwood: wood.h game.h
	cat game.h wood.h > wood.out

bronze: game.h bronze.h xnd.h concatbronze
	mv bronze.out bronze.cpp; $(CC) -DTEST -std=c++11 $(STDLIB) bronze.cpp -o bronze -g; mv bronze.cpp bronze.out

bronzec: game.h bronze.h xnd.h bronzec.cpp
	$(CC) -std=c++11 $(STDLIB) bronzec.cpp -o bronzec -g

concatbronze: bronze.h game.h xnd.h
	cat game.h xnd.h bronze.h > bronze.out
