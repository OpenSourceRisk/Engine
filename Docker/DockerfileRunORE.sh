#!/bin/sh

docker-compose --env-file Docker/.env -f Docker/docker-compose-boost.yml build || exit 1
COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file Docker/.env -f Docker/docker-compose-quantlib.yml build --no-cache || exit 1
COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file Docker/.env -f Docker/docker-compose-ore.yml build --no-cache  || exit 1
docker-compose --env-file Docker/.env -f Docker/docker-compose-test.yml build --no-cache || exit 1
COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file Docker/.env -f Docker/docker-compose-ore-app.yml build --no-cache  || exit 1
