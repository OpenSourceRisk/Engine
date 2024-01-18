#!/bin/sh

COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file Docker/.env -f Docker/docker-compose-ore.yml --no-cache build || exit 1
docker-compose --env-file Docker/.env -f Docker/docker-compose-test.yml --no-cache build || exit 1
COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file Docker/.env -f Docker/docker-compose-ore-app.yml --no-cache build || exit 1
