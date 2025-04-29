#!/usr/bin/env python3`
import argparse
from urllib.parse import unquote
import os
from flask import Flask, request, make_response
import ORE as ore
from ORE import OREApp
from oreApi import oreApi, MyCustomException

app = Flask(__name__)


@app.route('/api/analytics', methods=['POST'])
def handle_analytics_request():
    try:
        ore_instance = oreApi(request)
        message = ore_instance.runORE()
        if message == True:
            return "Success"
        else:
            return message
    except MyCustomException as e:
        # Handle any custom exceptions via MyCustomException
        error_message = str(e.args[0])
        response = make_response(f"{error_message}")
        response.status_code = 404
        return response

    except Exception as e:
        # Handle any exceptions raised by ore_instance.buildInputParameters()
        error_message = "Internal server error from restapi failing"
        error = str(e.args[0])
        response = make_response(f"error: {error_message} : {error}")
        response.status_code = 500
        return response

@app.route('/api/analytics/file/<path:filename>', methods=['GET', 'POST'])
def get_file(filename):
    filename = unquote(filename)
    fdir, file = os.path.split(filename)

    print("Loading parameters...")
    params = ore.Parameters()
    params.fromFile(filename)

    print("Creating OREApp...")
    ore_app = OREApp(params, True)

    print("Running ORE process...")
    ore_app.run()

    print("Run time: %.6f sec" % ore_app.getRunTime())

    print("ORE process done")

    return {}

if __name__ == '__main__':
    argparser = argparse.ArgumentParser(description='Simple file server')
    argparser.add_argument('--host', type=str, help='The host', default="localhost")
    argparser.add_argument('--port', type=str, help='The server port', default="5001")
    main_args = argparser.parse_args()

    app.run(host=main_args.host, port=main_args.port)

