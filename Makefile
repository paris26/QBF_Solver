all:
	gcc -Wall -Wextra -std=c99 -pedantic -g3 -o blocksqbf blocksqbf.c
opt:
	gcc -Wall -Wextra -std=c99 -pedantic -DNDEBUG -O3 -o blocksqbf blocksqbf.c
clean:
	rm *~ blocksqbf
