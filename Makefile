OBJS=main.o mpool.o htable.o
CFLAGS=-pedantic -std=c11 -O3 -march=native -Wall -Wextra
all: anf

anf: ${OBJS}
	${CC} ${LDFLAGS} ${OBJS} -o anf

%.o: src/%.c
	${CC} ${CFLAGS} -c $^ -o $@

clean:
	${RM} *.o
