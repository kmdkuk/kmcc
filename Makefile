COMPOSE_FILE=docker-compose.yml
bash:
	docker-compose run --rm app bash
docker:
	docker-compose build
