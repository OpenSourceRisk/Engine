#!/bin/bash

docker-compose -f docker-compose-boost.yml build --no-cache
docker-compose -f docker-compose-quantlib.yml build --no-cache
docker-compose -f docker-compose-ore.yml build --no-cache
