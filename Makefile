OBJS=build/anf.o build/scope.o build/sched.o build/logic.o build/mpool.o build/htable.o
CFLAGS=-pedantic -std=c99 -Wall -Wextra

ifdef RELEASE
	CFLAGS += -DNDEBUG -O3 -march=native
else
	CFLAGS += -g
endif

all: build/libanf.a

build/.timestamp:
	mkdir -p build && touch $@

build/libanf.a: ${OBJS}
	${AR} rcs build/libanf.a ${OBJS}

build/%.o: src/%.c build/.timestamp
	${CC} ${CFLAGS} -c $< -o $@

build/test: test/main.c build/libanf.a
	${CC} ${CFLAGS} -Isrc -Lbuild $< -o $@ -lanf

test: build/test
	@build/test

clean:
	${RM} -r build

.PHONY: all test clean
