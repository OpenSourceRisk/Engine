#!/usr/bin/env python3`
import argparse
from flask import Flask, request, make_response
from oreApi import oreApi, MyCustomException

app = Flask(__name__)


@app.route('/api/analytics', methods=['POST'])
def handle_analytics_request():
    try:
        ore_instance = oreApi(request)
        message = ore_instance.buildInputParameters()
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
        response = make_response(f"error: {error_message}")
        response.status_code = 500
        return response


if __name__ == '__main__':
    argparser = argparse.ArgumentParser(description='Simple file server')
    argparser.add_argument('--host', type=str, help='The host', default="localhost")
    argparser.add_argument('--port', type=str, help='The server port', default="5001")
    argparser.add_argument('--input_dir', type=str, help='Path containing input files', default='Input')
    argparser.add_argument('--output_dir', type=str, help='Path to write output', default='Output')
    main_args = argparser.parse_args()

    app.run(host=main_args.host, port=main_args.port)

