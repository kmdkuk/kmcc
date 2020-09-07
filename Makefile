COMPOSE_FILE=docker-compose.yml
CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

kmcc: $(OBJS)
	$(CC) -o kmcc $(OBJS) $(LDFLAGS)

$(OBJS): kmcc.h

test: kmcc
	./test.sh

clean:
	rm -f kmcc *.o *~ tmp*

bash:
	docker-compose run --rm app bash

docker:
	docker-compose build

.PHONY: test clean bash docker
