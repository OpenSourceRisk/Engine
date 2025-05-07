
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

For purposes of this tutorial, we are going to create a series of environment
variables, prefixed with `DEMO_`, pointing to the locations of the
prerequisites.  Once you have installed all of the prerequisites listed above,
set the following environment variables pointing to the relevant directories,
e.g:

    SET DEMO_BOOST=C:\repos\boost\boost_1_86_0
    SET DEMO_BOOST_LIB64=C:\repos\boost\boost_1_86_0\lib64-msvc-14.2
    SET DEMO_SWIG_DIR=C:\repos\swigwin\swigwin-4.3.0
    SET DEMO_ORE_DIR=C:\repos\Engine
    SET DEMO_ZLIB_ROOT=C:\repos\vcpkg\packages\zlib_x64-windows (optional)

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

    cd %DEMO_ORE_DIR%
    mkdir build
    cd %DEMO_ORE_DIR%\build
    SET BOOST_INCLUDEDIR=%DEMO_BOOST_ROOT%
    SET BOOST_LIB64=%DEMO_BOOST_LIB%
    SET ZLIB_ROOT=%DEMO_ZLIB_ROOT%
    SET SWIG_ROOT=%DEMO_SWIG_DIR%
    cmake -G "Visual Studio 17 2022" -A x64 .. -DBOOST_INCLUDEDIR=%BOOST% -DBOOST_LIBRARYDIR=%BOOST_LIB64% -DMSVC_LINK_DYNAMIC_RUNTIME=OFF -DORE-BUILD-SWIG=ON -DORE_BUILD_DOC=OFF -DORE_BUILD_EXAMPLES=OFF -DORE_BUILD_TESTS=OFF -DORE_BUILD_APP=OFF -DQL_BUILD_BENCHMARK=OFF -DQL_BUILD_EXAMPLES=OFF -DQL_BUILD_TEST_SUITE=OFF -DCMAKE_BUILD_TYPE=Release -DORE_USE_ZLIB=ON -DQL_ENABLE_SESSIONS=ON -DBoost_NO_SYSTEM_PATHS=ON
    cmake --build . --config Release

Make sure to add the location of the ORE-SWIG package to your PYTHONPATH, e.g.

    SET PYTHONPATH=%DEMO_ORE_DIR%\build;%DEMO_ORE_DIR%\build\ORE-SWIG\Release

## Build ORE/ORESWIG via setup.py

If using the setup.py method, you would have to build ORE with cmake as demonstrated 
above, but, when configuring cmake, disable the flag `-DORE_BUILD-SWIG=OFF`. Once 
ORE libraries have been built, below are the commands to build the ORESWIG Python
wrapper and wheel using setup.py.  If you do not require compression, then you can
omit the `ORE_USE_ZLIB` environment variable.

    cd Engine\ORE-SWIG\
    SET ORE_STATIC_RUNTIME=1
    SET ORE_USE_ZLIB=1
    python setup.py wrap
    python setup.py build
    python setup.py test
    python setup.py install
    python -m build --wheel

The `python setup.py install` should take care of installing ORE as a package
to be universally used in Python, without needing to set PYTHONPATH. Otherwise,

    SET PYTHONPATH=%DEMO_ORE_DIR%\ORE-SWIG\build\lib.win-amd64-cpython-310

### Use the wrapper

    cd %DEMO_ORE_DIR%\Examples\ORE-Python\ExampleScripts
    python swap.py

When you run example script `ore.py`, it writes to directory `Output` a number
of output files, including `cube.dat`.  If you have compression enabled, as
described above, then `cube.dat` is generated in compressed format (zip).  If
not then `cube.dat` is generated as a flat (plain text) file.


### Use the wheel

    cd %DEMO_ORE_DIR%\ORE-SWIG\ORE-Python\Examples
    python -m venv env1
    .\env1\Scripts\activate.bat
    pip install %DEMO_ORE_DIR%\ORE-SWIG\dist\open_source_risk_engine_-1.8.13-cp310-cp310-win_amd64.whl
    python swap.py
    deactivate
    rmdir /s /q env1


