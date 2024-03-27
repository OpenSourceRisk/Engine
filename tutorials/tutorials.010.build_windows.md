
# Building ORE on Windows

This tutorial explains how to build ORE on Windows.

[Back to tutorials index](../tutorials_index.md)

# Prerequisites

This build requires:

- Visual Studio
- Boost
- CMake

# Source code

You need to grab the source code for ORE, e.g:

    git clone --recurse-submodules https://github.com/OpenSourceRisk/Engine.git ore

# Environment Variables

For purposes of this tutorial, create environment variables pointing to the ORE
and boost directories, e.g:

    SET ORE_ROOT_DIR=C:\repos\ore
    SET BOOST_INCLUDEDIR=C:\repos\boost\boost_1_72_0
    SET BOOST_LIBRARYDIR=C:\repos\boost\boost_1_72_0\lib64-msvc-14.2

# Build ORE

## Configure ORE

Below are the commands to configure the ORE build using cmake:

    mkdir %ORE_ROOT_DIR%\build
    cd %ORE_ROOT_DIR%\build
    cmake ..

## Build ORE

Below are the commands to build ORE:

    cd %ORE_ROOT_DIR%\build
    cmake --build . --config Release

# Run tests

## QuantLib

cd to the test directory:

    cd %ORE_ROOT_DIR%\build\QuantLib\test-suite\Release

Run all unit tests:

    quantlib-test-suite.exe

Or run a selected test, e.g:

    quantlib-test-suite.exe --log_level=all --run_test="QuantLib test suite/Swap tests"

## QuantExt

    cd %ORE_ROOT_DIR%/build/QuantExt/test/Release
    quantext-test-suite.exe

## OREData

    cd %ORE_ROOT_DIR%/build/OREData/test/Release
    ored-test-suite.exe

## OREAnalytics

    cd %ORE_ROOT_DIR%/build/OREAnalytics/test/Release/
    orea-test-suite.exe

