COMPOSE_FILE=docker-compose.yml
CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

kmcc: $(OBJS)
	$(CC) -o kmcc $(OBJS) $(LDFLAGS)

$(OBJS): kmcc.h

test: kmcc
	./kmcc tests.kmc > tmp.s
	echo 'int char_fn() { return 257; }' | gcc -xc -c -o tmp2.o -
	gcc -static -o tmp tmp.s tmp2.o
	./tmp

clean:
	rm -f kmcc *.o *~ tmp*

sh:
	docker-compose run --rm app zsh

docker:
	docker-compose build

.PHONY: test clean bash docker
