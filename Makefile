OBJS=main.o mpool.o htable.o
CFLAGS=-pedantic -std=c11 -g -Wall -Wextra
all: anf

anf: ${OBJS}
	${CC} ${LDFLAGS} ${OBJS} -o anf

%.o: src/%.c
	${CC} ${CFLAGS} -c $^ -o $@

clean:
	${RM} *.o