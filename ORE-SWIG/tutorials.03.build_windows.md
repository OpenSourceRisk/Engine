
# Building ORE Python Wrappers on Windows

This tutorial explains how to build and run the ORE wrapper and wheel on
Windows.

[Back to tutorials index](../tutorials_index.md)

# Prerequisites

Before building ORE and ORESWIG, you must install **boost**.  The ORE project
currently uses boost version 1.72 - any version of boost equal to or greater
than that ought to work.  You also need to install **swig** and **cmake**.
Swig must be 4.3.0 or greater.

## ZLib

ORE supports zlib for compressed output.  Zlib support is optional - if you
want to enable it, you must install Zlib.  You must also ensure that your copy
of `boost::iostreams` is zlib-enabled.  At either of the links below, you can
download boost binaries which include zlib-enabled `boost::iostreams`:

https://sourceforge.net/projects/boost/files/boost-binaries/

https://archives.boost.io/release/

Otherwise you can compile boost yourself from source with zlib-enabled
`boost::iostreams` as described
[here](https://www.boost.org/doc/libs/1_86_0/libs/iostreams/doc/index.html).
You can confirm whether your copy of `boost::iostreams` is zlib-enabled by
looking for the relevant binaries (e.g. `libboost_zlib-vc143-mt-x64-1_86.lib`)
in your boost lib directory.

# Source code

You need to grab the source code for ORE and ORESWIG, e.g:

    git clone --recurse-submodules https://github.com/OpenSourceRisk/Engine.git ore

# Environment Variables

For purposes of this tutorial, create environment variables pointing to ORE and
its prerequisites, e.g:

    SET BOOST_INCLUDEDIR=C:\repos\boost\boost_1_72_0
    SET BOOST_LIBRARYDIR=C:\repos\boost\boost_1_72_0\lib64-msvc-14.2
    SET SWIG_ROOT=C:\repos\swigwin\swigwin-4.3.0
    SET ZLIB_ROOT=C:\repos\vcpkg\packages\zlib_x64-windows (optional)
    SET ORE_DIR=C:\repos\ore

# Build ORE and ORESWIG

You can use **cmake** to build ORE and the ORESWIG wrapper.  You can use
**setup.py** to build the ORESWIG wrapper and wheel.  If you use setup.py to
build ORE-SWIG, you must first build ORE.

## Build ORE/ORESWIG via CMake

Below are the commands to configure and build ORE and ORE-SWIG using cmake. If
you do not require compression, then you can omit the `ZLIB_ROOT` environment
variable, and, when running cmake, you can omit flag `-DORE_USE_ZLIB=ON`.

    cd %ORE_DIR%
    mkdir build
    cd %ORE_DIR%\build
    cmake .. -DMSVC_LINK_DYNAMIC_RUNTIME=OFF -DORE_USE_ZLIB=ON -DQL_ENABLE_SESSIONS=ON -DORE_BUILD_DOC=OFF -DORE_BUILD_EXAMPLES=OFF -DORE_BUILD_TESTS=OFF -DORE_BUILD_APP=OFF -DQL_BUILD_BENCHMARK=OFF -DQL_BUILD_EXAMPLES=OFF -DQL_BUILD_TEST_SUITE=OFF
    cmake --build . --config Release

Make sure to add the location of the ORE-SWIG package to your PYTHONPATH, e.g:

    SET PYTHONPATH=%ORE_DIR%\build\ORE-SWIG;%ORE_DIR%\build\ORE-SWIG\Release

## Build ORESWIG via setup.py

If using the setup.py method, you would have to build ORE with cmake as
demonstrated above, but, when configuring cmake, disable the SWIG build with:
`-DORE_BUILD-SWIG=OFF`. Once ORE libraries have been built, below are the
commands to build the ORESWIG Python wrapper using setup.py.  If you
do not require compression, then you can omit the `ORE_USE_ZLIB` environment
variable.

In the commands below, to avoid polluting your primary installation of python,
we create a virtual environment into which we install the packages required by
setup.py.

    cd %ORE_DIR%\ORE-SWIG
    SET BOOST=C:\repos\boost\boost_1_72_0
    SET BOOST_LIB64=C:\repos\boost\boost_1_72_0\lib64-msvc-14.2
    SET ORE_STATIC_RUNTIME=1
    SET ORE_USE_ZLIB=1
    SET PATH=%PATH%;C:\path\to\swig
    python -m venv env1
    .\env1\Scripts\activate.bat
    python -m pip install --upgrade pip
    python -m pip install build pynose pytest setuptools
    python setup.py wrap
    python setup.py build
    python setup.py test
    python setup.py install
    python -m build --wheel
    deactivate

The `python setup.py install` should take care of installing ORE as a package
to be universally used in Python, without needing to set PYTHONPATH. Otherwise,

    SET PYTHONPATH=%ORE_DIR%\ORE-SWIG\build\lib.win-amd64-cpython-312

### Use the wrapper

    cd %ORE_DIR%\Examples\ORE-Python\ExampleScripts
    python swap.py

When you run example script `ore.py`, it writes to directory `Output` a number
of output files, including `cube.dat`.  If you have compression enabled, as
described above, then `cube.dat` is generated in compressed format (zip).  If
not then `cube.dat` is generated as a flat (plain text) file.

## Build the wheel with setup.py

Whether you used cmake or setup.py above to build the wrapper, you can use
setup.py to build the wheel.  If you used setup.py before then you already have
the python virtual environment, and you can reuse it, otherwise you can create
a new one as shown below.

    cd %ORE_DIR%\ORE-SWIG
    SET BOOST=C:\repos\boost\boost_1_72_0
    SET BOOST_LIB64=C:\repos\boost\boost_1_72_0\lib64-msvc-14.2
    python -m venv env1
    .\env1\Scripts\activate.bat
    python -m pip install --upgrade pip
    python -m pip install build
    python -m build --wheel
    deactivate

### Use the wheel

    cd %ORE_DIR%\Examples\ORE-Python\ExampleScripts
    python -m venv env1
    .\env1\Scripts\activate.bat
    pip install %ORE_DIR%\ORE-SWIG\dist\open_source_risk_engine-1.8.13.1-cp312-cp312-win_amd64.whl
    python swap.py
    deactivate

