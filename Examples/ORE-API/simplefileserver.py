from doctest import Example
import flask
import argparse
import os
from pathlib import Path
from urllib.parse import unquote

def getMimeType(filename):
    if filename.endswith(".xml"):
        return "text/xml"
    elif filename.endswith(".json"):
        return "application/json"
    else:
        return "application/octet-stream"

api = flask.Flask(__name__)

file_type_header_map = {
    "csv": "text/csv",
    "txt": "text/plain",
    "xml": "text/xml",
    "json": "application/json",
}

def is_directory_traversal(file_name):
    examples_directory = os.path.normpath(Path(__file__).resolve().parents[1])
    if os.path.basename(os.path.split(examples_directory)[0]) == "ore" and os.path.split(examples_directory)[1] == "Examples":
        requested_path = os.path.relpath(file_name, start=examples_directory)
        requested_path = os.path.abspath(requested_path)
        common_prefix = os.path.normpath(os.path.commonprefix([requested_path, examples_directory]))
        return common_prefix != examples_directory
    else:
        return True

@api.route('/file/<path:filename>', methods=['GET', 'POST'])
def get_file(filename):
    # responses are truncated if the request is large (even if it is not used as here), this appears to be a bug
    # adding request.get_data() seems to work around that for some reason. observed in flask version 1.1.2
    flask.request.get_data()
    file_type = filename.rsplit(".", 1)[1]
    fdir, file = os.path.split(unquote(filename))
    filepath = os.path.join(main_args.input_dir, fdir)
    if not is_directory_traversal(filepath):
        response = flask.make_response(flask.send_from_directory(filepath, file, mimetype=getMimeType(filename)))
        response.headers['Content-Type'] = file_type_header_map[file_type]
        return response
    else:
        flask.abort(400, description="backwards directory traversal detected!")

@api.route('/file/<path:filename>/<notused>', methods=['GET', 'POST'])
def get_file2(filename, notused):
    flask.request.get_data()
    fdir, file = os.path.split(unquote(filename))
    filepath = os.path.join(main_args.input_dir, fdir)
    if not is_directory_traversal(filepath):
        return flask.send_from_directory(os.path.join(main_args.input_dir, fdir), file, mimetype=getMimeType(filename))
    else:
        flask.abort(400, description="backwards directory traversal detected!")
        
@api.route('/report/<path:filename>', methods=['POST'])
def post_report(filename):
    fdir, file = os.path.split(unquote(filename))
    filename = ''
    if main_args.output_dir:
        filename = os.path.join(main_args.output_dir, os.path.join(fdir, file.replace('-', '_')))
    else:
        filename = os.path.join(os.path.join(fdir, file.replace('-', '_')))
    file_dir = os.path.dirname(os.path.realpath(filename))
    if not os.path.exists(file_dir) and not is_directory_traversal(file_dir):
        try:
            os.makedirs(file_dir)
        except OSError as e:
            raise
    elif is_directory_traversal(file_dir):
        return flask.abort(400, description="backwards directory traversal detected!")
    data = flask.request.get_data(as_text=True)
    data = unquote(data)
    clean_data = data.replace("data=", "#", 1)
    if not is_directory_traversal(filename):
        with open(filename, "w", newline='') as f:
            f.write(clean_data)
            return flask.Response()
    else:
        return flask.abort(400, description="backwards directory traversal detected!")

if __name__ == '__main__':

    argparser = argparse.ArgumentParser(description='Simple file server')
    argparser.add_argument('--host', type=str, help='The host', default="localhost")
    argparser.add_argument('--port', type=str, help='The server port', default="5000")
    argparser.add_argument('--input_dir', type=str, help='Path containing input files', default='Input')
    argparser.add_argument('--output_dir', type=str, help='Path to write output', default='Output')
    main_args = argparser.parse_args()

    api.run(host=main_args.host, port=main_args.port)
