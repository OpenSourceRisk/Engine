#!/bin/sh

export $(cat Docker/.env | xargs)

docker-compose --env-file Docker/.env -f Docker/docker-compose-boost.yml build || exit 1
if docker inspect "env_quantlib:$QL_TAG" > /dev/null 2>&1 ; then  
    echo "env_quantlib:$QL_TAG already exists"
else
    COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file Docker/.env -f Docker/docker-compose-quantlib.yml build || exit 1
fi

COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file Docker/.env -f Docker/docker-compose-ore.yml build || exit 1
COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker-compose --env-file Docker/.env -f Docker/docker-compose-ore-app.yml build || exit 1

