
# Running ORE Under Docker

The ORE project includes support for Docker.  You can build a docker container
including ORE binaries and tests.

## Docker scripts

Below are the scripts which the ORE project includes in support of Docker:

```
├───Docker/.env                             set environment variables
│
└───Docker/DockerfileRunORE.sh              build all docker containers
    │   
    ├───Docker/docker-compose-ore.yml       compose file for container env_ore
    │   │
    │   └────Docker/Dockerfile-ORE          docker file for container env_ore
    │   
    ├───Docker/docker-compose-test.yml      compose file for container env_ore_test
    │   │
    │   └───Docker/Dockerfile-Test          docker file for container env_ore_test
    │   
    └───Docker/docker-compose-ore-app.yml   compose file for container ore_app
        │
        └───Docker/Dockerfile-ORE-App       docker file for container ore_app
```
`env_ore` is a preliminary container for the build environment.  `env_ore_test`
is a preliminary container for the testing tools.  The minimum essentials from
these two containers are extracted into container `ore_app`, which is the final
product supporting a working ORE environment.

## Building the Docker Container

If you are on a POSIX system (e.g. linux or MacOS), you can run:

    Docker/DockerfileRunORE.sh

On Windows, you can replicate the behavior of the above script by running the
following commands at a DOS prompt:

    SET COMPOSE_DOCKER_CLI_BUILD=1    
    SET DOCKER_BUILDKIT=1    
    docker-compose --env-file Docker/.env -f Docker/docker-compose-ore.yml build    
    docker-compose --env-file Docker/.env -f Docker/docker-compose-test.yml build    
    docker-compose --env-file Docker/.env -f Docker/docker-compose-ore-app.yml build    

This creates the docker images:

    $ docker images
    REPOSITORY     TAG       IMAGE ID       CREATED          SIZE
    ore_app        latest    1861c2dba528   7 seconds ago    2.02GB
    env_ore        latest    9e016430d0dd   15 minutes ago   3.42GB
    env_ore_test   latest    55b8da9c2c8a   2 months ago     1.15GB

## Run the Docker Container Interactively

Invoke the command below to run the docker container interactively:

    docker run -it ore_app bash

Here are the commands to run the examples:

    cd /ore/Examples
    python3 run_examples_testsuite.py --with-xunitmp --xunitmp-file=examples.xml --processes=8 --process-timeout=3600

Here are the commands to run all of the unit tests in parallel:

    cd /ore
    PARAMS="--log_format=XML --log_level=test_suite --report_level=no --result_code=no"
    /ore/App/quantlib-test-suite    --nProc=4 $PARAMS --log_sink=xunit_ql.xml &
    /ore/App/quantext-test-suite    --nProc=6 $PARAMS --log_sink=xunit_qle.xml &
    /ore/App/ored-test-suite        $PARAMS --log_sink=xunit_ored.xml -- --base_data_path=/ore/OREData/test &
    /ore/App/orea-test-suite        --nProc=4 $PARAMS --log_sink=xunit_orea.xml -- --base_data_path=/ore/OREAnalytics/test &
    wait

