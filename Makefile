COMPOSE_FILE=docker-compose.yml
CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

kmcc: $(OBJS)
	$(CC) -o kmcc $(OBJS) $(LDFLAGS)

$(OBJS): kmcc.h

test: kmcc
	./kmcc tests.kmc > tmp.s
	gcc -static -o tmp tmp.s
	./tmp

clean:
	rm -f kmcc *.o *~ tmp*

sh:
	docker-compose run --rm app zsh

docker:
	docker-compose build

.PHONY: test clean bash docker
