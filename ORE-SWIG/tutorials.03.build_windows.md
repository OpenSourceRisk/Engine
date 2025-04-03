
# Building ORE Python Wrappers on Windows

This tutorial explains how to build and run the ORE wrapper and wheel on
Windows.

[Back to tutorials index](tutorials.00.index.md)

# Prerequisites

Before building ORE and ORESWIG, you must install **boost**.  The ORE project
currently uses boost version 1.72 - any version of boost equal to or greater
than that ought to work.  You also need to install **swig** and **cmake**.

## ZLib

ORE supports zlib for compressed output.  Zlib support is optional - if you
want to enable it, you must install Zlib.  You must also ensure that your copy
of `boost::iostreams` is zlib-enabled.  At either of the links below, you can
download boost binaries which include zlib-enabled `boost::iostreams`:

https://sourceforge.net/projects/boost/files/boost-binaries/

https://boostorg.jfrog.io/artifactory/main/release/

Otherwise you can compile boost yourself from source with zlib-enabled
`boost::iostreams` as described
[here](https://www.boost.org/doc/libs/1_82_0/libs/iostreams/doc/index.html).
You can confirm whether your copy of `boost::iostreams` is zlib-enabled by
looking for the relevant binaries (e.g. `libboost_zlib-vc142-mt-x64-1_72.lib`)
in your boost lib directory.

# Source code

You need to grab the source code for ORE and ORESWIG, e.g:

    git clone --recurse-submodules https://github.com/OpenSourceRisk/Engine.git ore
    git clone --recurse-submodules https://github.com/OpenSourceRisk/ORE-SWIG.git oreswig

# Environment Variables

For purposes of this tutorial, we are going to create a series of environment
variables, prefixed with `DEMO_`, pointing to the locations of the
prerequisites.  Once you have installed all of the prerequisites listed above,
set the following environment variables pointing to the relevant directories,
e.g:

    SET DEMO_BOOST_ROOT=C:\repos\boost\boost_1_72_0
    SET DEMO_BOOST_LIB=C:\repos\boost\boost_1_72_0\lib64-msvc-14.2
    SET DEMO_SWIG_DIR=C:\repos\swigwin\swigwin-4.1.1
    SET DEMO_ORE_DIR=C:\repos\oreplus\ore
    SET DEMO_ORE_SWIG_DIR=C:\repos\oreswig
    SET DEMO_ZLIB_ROOT=C:\repos\vcpkg\packages\zlib_x64-windows (optional)

# Build ORE

## Configure ORE

Below are the commands to configure the ORE build using cmake.  If you do not
require compression, then you can omit the `ZLIB_ROOT` environment variable,
and, when running cmake, you can omit flag `-DORE_USE_ZLIB=ON`.

    cd %DEMO_ORE_DIR%
    mkdir build
    cd %DEMO_ORE_DIR%\build
    SET BOOST_INCLUDEDIR=%DEMO_BOOST_ROOT%
    SET BOOST_LIBRARYDIR=%DEMO_BOOST_LIB%
    SET ZLIB_ROOT=%DEMO_ZLIB_ROOT%
    cmake -G "Visual Studio 17 2022" -A x64 .. -DMSVC_LINK_DYNAMIC_RUNTIME=OFF -DORE_BUILD_DOC=OFF -DORE_BUILD_EXAMPLES=OFF -DORE_BUILD_TESTS=OFF -DORE_BUILD_APP=OFF -DQL_BUILD_BENCHMARK=OFF -DQL_BUILD_EXAMPLES=OFF -DQL_BUILD_TEST_SUITE=OFF -DCMAKE_BUILD_TYPE=Release -DORE_USE_ZLIB=ON -DQL_ENABLE_SESSIONS=ON -DBoost_NO_SYSTEM_PATHS=ON

## Build ORE

Below are the commands to build ORE.

    cd %DEMO_ORE_DIR%\build
    cmake --build . --config Release

# Build ORESWIG

There are two ways to build ORESWIG: using **cmake**, or using **setup.py**.
With cmake, you can generate the wrapper.  With setup.py you can generate both
the wrapper and the wheel.

## Build ORESWIG Using CMake

### Configure ORESWIG

Below are the commands to configure the ORESWIG build using cmake.  If you do
not require compression, then you can omit the `ZLIB_ROOT` environment
variable, and, when running cmake, you can omit flag `-DORE_USE_ZLIB=ON`.

    cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python
    mkdir build
    cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\build
    SET BOOST_INCLUDEDIR=%DEMO_BOOST_ROOT%
    SET BOOST_LIBRARYDIR=%DEMO_BOOST_LIB%
    SET SWIG_ROOT=%DEMO_SWIG_DIR%
    SET ZLIB_ROOT=%DEMO_ZLIB_ROOT%
    cmake -Wno-dev -G "Visual Studio 17 2022" -A x64 .. -DORE=%DEMO_ORE_DIR% -DMSVC_LINK_DYNAMIC_RUNTIME=OFF -DORE_USE_ZLIB=ON -DBoost_NO_SYSTEM_PATHS=ON

### Build ORESWIG

Below are the commands to build ORESWIG.

    cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\build
    cmake --build . --config Release

### Use the wrapper

    cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\Examples
    SET PYTHONPATH=%DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\build;%DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\build\Release
    python swap.py

When you run example script `ore.py`, it writes to directory `Output` a number
of output files, including `cube.dat`.  If you have compression enabled, as
described above, then `cube.dat` is generated in compressed format (zip).  If
not then `cube.dat` is generated as a flat (plain text) file.

## Build ORESWIG Using setup.py

### Build ORESWIG

Below are the commands to build the ORESWIG Python wrapper and wheel using
setup.py.  If you do not require compression, then you can omit the
`ORE_USE_ZLIB` environment variable.

    cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python
    SET BOOST_ROOT=%DEMO_BOOST_ROOT%
    SET BOOST_LIB=%DEMO_BOOST_LIB%
    SET ORE_DIR=%DEMO_ORE_DIR%
    SET PATH=%PATH%;%DEMO_SWIG_DIR%
    SET ORE_STATIC_RUNTIME=1
    SET ORE_USE_ZLIB=1
    python setup.py wrap
    python setup.py build
    python setup.py test
    python -m build --wheel

### Use the wrapper

    cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\Examples
    SET PYTHONPATH=%DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\build\lib.win-amd64-cpython-310
    python swap.py

### Use the wheel

    cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\Examples
    python -m venv env1
    .\env1\Scripts\activate.bat
    pip install %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\dist\osre-1.8.9.1-cp310-cp310-win_amd64.whl
    python swap.py
    deactivate
    rmdir /s /q env1

