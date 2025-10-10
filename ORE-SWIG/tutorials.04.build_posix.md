
# Building ORE Python Wrappers on Posix Systems (e.g. MacOS and Linux)

This tutorial explains how to build and run the ORE wrapper and wheel on Posix
systems such as MacOS and Linux.  The commands listed here were tested on
Ubuntu and you will likely have to modify the commands slightly on other
systems.

[Back to tutorials index](../tutorials_index.md)

# Prerequisites

Before building ORE and ORESWIG, you must install **boost**.  The ORE project
currently uses boost version 1.72 - any version of boost equal to or greater
than that ought to work.  You also need to install **swig** and **cmake**.

## ZLib

ORE supports zlib for compressed output.  Zlib support is optional - if you
want to enable it, you must install Zlib.  You must also ensure that your copy
of `boost::iostreams` is zlib-enabled.  The instructions for compiling
`boost::iostreams` with zlib support can be found
[here](https://www.boost.org/doc/libs/1_82_0/libs/iostreams/doc/index.html).

# Source code

You need to grab the source code for ORE and ORESWIG, e.g:

    git clone --recurse-submodules https://github.com/OpenSourceRisk/Engine.git ore

# Environment Variables

For purposes of this tutorial, we are going to create a series of environment
variables, prefixed with `DEMO_`, pointing to the locations of the
prerequisites.  Once you have installed all of the prerequisites listed above,
set the following environment variables pointing to the relevant directories,
e.g:

    DEMO_BOOST_INC=/home/repos/boost/install/include
    DEMO_BOOST_LIB=/home/repos/boost/install/lib
    DEMO_ORE_DIR=/home/repos/Engine
    DEMO_ZLIB_ROOT=/home/repos/zlib/zlib-1.2.13 (optional)

# Build ORE and ORE-SWIG
There are two ways to build ORESWIG: using **cmake**, or using **setup.py**.
With cmake, you can generate the wrapper.  With setup.py you can generate both
the wrapper and the wheel. Regardless, ORE libraries must be built first.

## Build ORE/ORESWIG via CMake

Below are the commands to configure the ORE and ORE-SWIG build using cmake. If you do not
require compression, then you can omit the `ZLIB_ROOT` environment variable,
and, when running cmake, you can omit flag `-DORE_USE_ZLIB=ON`. 

To build ORE and ORE-SWIG simulataneously With cmake, you can generate the wrapper by 
simply including `-DORE_BUILD-SWIG=ON` when configuring your ORE. 

    cd $DEMO_ORE_DIR
    mkdir build
    cd $DEMO_ORE_DIR/build
    export BOOST_INCLUDEDIR=$DEMO_BOOST_INC
    export BOOST_LIBRARYDIR=$DEMO_BOOST_LIB
    export ZLIB_ROOT=%DEMO_ZLIB_ROOT%
    cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DORE_BUILD_DOC=OFF -DORE_BUILD_EXAMPLES=OFF -DORE-BUILD-SWIG=ON -DORE_BUILD_TESTS=OFF -DORE_BUILD_APP=OFF -DQL_BUILD_BENCHMARK=OFF -DQL_BUILD_EXAMPLES=OFF -DQL_BUILD_TEST_SUITE=OFF -DBoost_NO_WARN_NEW_VERSIONS=1 -DBoost_NO_SYSTEM_PATHS=1 -DORE_USE_ZLIB=ON -DQL_ENABLE_SESSIONS=ON
    cmake --build .


## Build ORE/ORESWIG via setup.py

If using the setup.py method, you would have to build ORE with cmake as demonstrated 
above, but, when configuring cmake, disable the flag `-DORE_BUILD-SWIG=OFF`. Once 
ORE libraries have been built, below are the commands to build the ORESWIG Python
wrapper and wheel using setup.py.  If you do not require compression, then you can
omit the `ORE_USE_ZLIB` environment variable.

    cd $DEMO_ORE_DIR/ORE-SWIG
    export BOOST_INC=$DEMO_BOOST_INC
    export BOOST_LIB=$DEMO_BOOST_LIB
    export ORE=$DEMO_ORE_DIR
    export ORE_USE_ZLIB=1
    python3 setup.py wrap
    python3 setup.py build
    python setup.py test
    python3 setup.py install
    python3 -m build --wheel


### Use the wrapper

    cd $DEMO_ORE_DIR/Examples/ORE-Python/ExampleScripts
    export PYTHONPATH=$DEMO_ORE_DIR/build/ORE-SWIG
    export LD_LIBRARY_PATH=$DEMO_ORE_DIR/build/OREAnalytics/orea:$DEMO_ORE_DIR/build/OREData/ored:$DEMO_ORE_DIR/build/QuantExt/qle:$DEMO_ORE_DIR/build/QuantLib/ql:$DEMO_BOOST_LIB
    python3 swap.py

When you run example script `ore.py`, it writes to directory `Output` a number
of output files, including `cube.dat`.  If you have compression enabled, as
described above, then `cube.dat` is generated in compressed format (.tar.gz).
If not then `cube.dat` is generated as a flat (plain text) file.


### Use the wheel

    cd $DEMO_ORE_DIR/Examples/ORE-Python/ExampleScripts
    python3 -m venv env1
    . ./env1/bin/activate
    pip install $DEMO_ORE_DIR/ORE-SWIG/dist/open-source-risk-engine-1.8.13-cp310-cp310-linux_x86_64.whl
    python3 swap.py
    deactivate
    rm -rf env1

