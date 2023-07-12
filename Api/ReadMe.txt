1) oreApi.py 

oreApi.py creates a class that builds all input parameters based on a json file that contains data for the given example (Example.json for our example which is just the json version of example.xml).

2) restapi.py

Restapi.py runs a flask api that takes a post request from a server, like postman, that contains a json file with data for the given example and it calls the oreApi.py class to build the input parameters to produce an output.  

3) simplefileserver.py

Simplefileserver.py runs a flask api on a localhost, normally 5000, and takes requests from postman that are urls of xml files that contain data that is needed to build the input parameters. This script needs to be running and communicating with restapi.py to work.

4) Docker Files

Dockerfile_restapi and Dockerfile_simplefileserver
These docker files copy the needed files, run the necessary installations, and create input/output directories. They also call entrypoint bash files that specify the ports that the docker images later to be created will run on. 
Docker-compose-build-sfs.yml and Docker-compose-build-restapi.yml
Based on the above docker files these two yml files actually build the docker images and their containers are filled later as these two scripts just build the images.  
Docker-compose-start.yml
Based on the above docker images that were created this yml file actually runs and fills the containers of the docker images and most importantly both run on the same network specified in the yml file so that they can communicate with each other. 


5) Running everything 

To actually run the example first you need to build the docker images by running this command:
docker-compose -f dockerfiles/docker-compose-build-sfs.yml -f dockerfiles/docker-compose-build-restapi.yml build

Once the images are built to run the two images you can run this command:
docker-compose -f dockerfiles/docker-compose-start.yml up

Note for the above command: Since the dockerfiles are in a seperate subdirectory the dockerfiles/ is needed. Also, if you want to specify the Input and Output separately from the docker file through an environment variable you have to use the –env-file=name.env flag with name being the name of the environment variable so for this example it would be: docker-compose -f docker-compose-start.yml --env-file=docker-envfile.env up

Then once the images are running on postman send a post request in a json format that contains the data for the example. All xml data files should be sent to the simplefileserver image in the URL format that is encoded twice that matches the dockerimage name eg. dockerfiles-simplefileserver-1

Once the request is sent the docker images should build the results and send the results to the output path specified. 

Last note: In the fromFiles directory in the Examples directory, apifast.py and apiflask.py are basic scripts that produce the same output as above except they do not build the input parameters using python. Fast Api is very similar to flask and for the above example it should be easy to transition between flask and Fast Api simply by changing the api requests to the syntax of Fast Api which can be seen in the apifast.py script

