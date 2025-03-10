#!/bin/sh
COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file ore/Docker/.env -f ore/ORE-SWIG/Docker/docker-compose-oreswig.yml build || exit 1
COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file ore/Docker/.env -f ore/ORE-SWIG/Docker/docker-compose-oreswig-app.yml build || exit 1