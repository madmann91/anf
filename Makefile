DEPS=src/main.c src/anf.c src/anf.h src/mpool.c src/mpool.h src/htable.c src/htable.h src/hash.h
OBJS=main.o anf.o mpool.o htable.o
CFLAGS=-pedantic -std=c11 -O3 -march=native -Wall -Wextra
all: anf

anf: ${OBJS}
	${CC} ${LDFLAGS} ${OBJS} -o anf

%.o: src/%.c ${DEPS}
	${CC} ${CFLAGS} -c $< -o $@

clean:
	${RM} *.o
