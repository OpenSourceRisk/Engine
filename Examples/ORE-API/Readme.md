ORE as a Service
================

This folder contains a proof of concept implementation of a web service around ORE written in Python.
It is based on
- the flask web framework https://flask.palletsprojects.com, and
- the ORE Python module which can be installed using "pip install open-source-risk-engine"
  or built from sources following the instructions in the ORE-SWIG repository at
  https://github.com/opensourcerisk/ore-swig.

Main Files
----------

- **restapi.py** runs a flask api as analytics service that takes a post request (e.g. from a server like
postman, or from a python script): The request contains a json body which corresponds to the data usually
contained in the master input file ore.xml. restapi.py calls the **oreApi.py** class to do the work. By default,
the analytics service listens for requests on port 5001.

- **oreApi.py** reads the json body, compiles all ORE input parameters, calling into a data service (see below)
to retrieve additional data. Then it kicks off an ORE run to process the request.  And finally it posts
resulting reports through the data service.

- **simplefileserver.py** runs a flask api as a data service. It takes requests from the
analytics service above in the form of urls of xml files that contain additional data required by ORE (market
data, fixing data, portfolio, configuration data). By default, the data service listens on port 5000,
reads from the Input directory in ore/Api and writes reports to the Output directory in ore/Api.

Run a local Example
-------------------

- start the data service
  python3 simplefileserver.py &

- start the analytics service
  python3 restapi.py &

- send a request, the equivalent of Example 1
  python3 request.py
 