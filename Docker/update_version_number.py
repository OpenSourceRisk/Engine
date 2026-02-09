
# Invoke this file from the ore directory to
# overwrite the version number in one of the files below:
#
# ORE-SWIG/setup.py
# QuantExt/qle/version.hpp
#
# e.g:
# cd /path/to/ore
# python /path/to/update_version_number.py -v 1.23.45 -f setup.py
# python /path/to/update_version_number.py -v 1.23.45 -f version.hpp

import optparse
import shutil
import re
import os.path

SETUP_PY = "ORE-SWIG/setup.py"
VERSION_HPP = "QuantExt/qle/version.hpp"

# Convert a version string into a string
# containing a corresponding numerical value:
# 1.8.14.0     ->  1081400
# 1.234.5      -> 12340500
# 1.234.5.dev1 -> 12340501
def version_string_to_number(v):
    ret = ""
    # match x.x.x or x.x.x.x
    # where x = one or more alphanumeric characters
    p = r"(\w*)\.(\w*)\.(\w*)(?:\.(\w*))?"
    m = re.match(p, v)
    if not m:
        raise Exception("version string '{v}' has invalid format, expected x.x.x or x.x.x.x where x in [a-zA-Z0-9_]")
    for x in m.groups():
        x = re.sub("[^0-9]", "", x or "")
        if x:
            ret += x.zfill(2)
        else:
            ret += '00'
    return ret.lstrip('0')

# Parse the command line arguments
parser = optparse.OptionParser()
parser.add_option('-f', '--file')
parser.add_option('-v', '--version')
opts, args = parser.parse_args()
file = opts.file
version = opts.version

if file is None:
    raise Exception("missing input parameter --file")

if version is None:
    raise Exception("missing input parameter --version")

def find_and_replace(file, repl):

    if not os.path.isfile(file):
        raise Exception(f"invalid path: {file}")

    shutil.copy(file, file + ".bak")

    with open(file) as f:
        s = f.read()

    for (p, r) in repl:
        s = re.sub(p, r, s)

    with open(file, 'w') as f:
        f.write(s)

if file == "setup.py":
    find_and_replace(SETUP_PY, [(r'(version\s*=\s*").*(")', fr'\g<1>{version}\g<2>')])
elif file == "version.hpp":
    version_num = version_string_to_number(version)
    find_and_replace(VERSION_HPP, [
        (r'(#define OPEN_SOURCE_RISK_VERSION ").*(")', fr'\g<1>{version}\g<2>'),
        (r'(#define OPEN_SOURCE_RISK_VERSION_NUM ).*', fr'\g<1>{version_num}')])
else:
    raise Exception(f"unrecognized file: {file}")

