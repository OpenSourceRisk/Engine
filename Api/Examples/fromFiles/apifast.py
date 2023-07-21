#!/usr/bin/env python
import argparse
import traceback
import uvicorn
from ORE import OREApp
from urllib.parse import unquote
import ORE as ql
import os
from fastapi import FastAPI
app = FastAPI()


@app.get('/file/{filename}')
def get_file(filename):
    try:
        filename = unquote(filename)
        fdir, file = os.path.split(filename)

        print("Loading parameters...")
        params = ql.Parameters()
        params.fromFile(filename)

        print("Creating OREApp...")
        os.chdir(fdir + r"\..")
        ore = OREApp(params)

        print("Running ORE process...")
        ore.run()

        print("Run time: %.6f sec" % ore.getRunTime())

        print("ORE process done")

        return {}

    except Exception as e:
        print("An error occurred:")
        print(str(e))
        traceback.print_exc()
        return {"error": "An error occurred"}


if __name__ == '__main__':
    '''uvicorn.run(app, host="localhost", port=5000)'''
    argparser = argparse.ArgumentParser(description='Simple file server')
    argparser.add_argument('--host', type=str, help='The host', default="localhost")
    argparser.add_argument('--port', type=str, help='The server port', default="5000")
    argparser.add_argument('--input_dir', type=str, help='Path containing input files', default='Input')
    argparser.add_argument('--output_dir', type=str, help='Path to write output', default='Output')
    main_args = argparser.parse_args()

    uvicorn.run(app, host=main_args.host, port=main_args.port)
