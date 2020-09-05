COMPOSE_FILE=docker-compose.yml
CFLAGS=-std=c11 -g -static

kmcc: kmcc.c

test: kmcc
	./test.sh

clean:
	rm -f bin/* tmp/*

bash:
	docker-compose run --rm app bash

docker:
	docker-compose build
