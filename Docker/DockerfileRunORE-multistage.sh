#!/bin/sh
docker-compose --env-file Docker/.env -f Docker/docker-compose-multistage.yml build
