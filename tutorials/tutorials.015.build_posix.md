
# Building ORE on Posix Systems (e.g. MacOS and Linux)

This tutorial explains how to build ORE on Posix systems such as MacOS and
Linux.  The commands listed here were tested on Ubuntu and you will likely have
to modify the commands slightly on other systems.

[Back to tutorials index](../tutorials_index.md)

# Prerequisites

This build requires:

- gcc
- Boost
- CMake

# Source code

You need to grab the source code for ORE, e.g:

    git clone --recurse-submodules https://github.com/OpenSourceRisk/Engine.git ore

# Environment Variables

For purposes of this tutorial, create environment variable
`ORE_ROOT_DIR`pointing to the root directory of your build, e.g:

    ORE_ROOT_DIR=/home/repos/ore

If you installed boost from packages (e.g. `sudo apt install
libboost-all-dev`), then your build will probably find boost automatically.  If
you built boost yourself from source, then export some environment variables
pointing to the boost include and lib directories, e.g:

    export BOOST_INCLUDEDIR=/home/repos/boost/install/include
    export BOOST_LIBRARYDIR=/home/repos/boost/install/lib

# Build ORE

## Configure ORE

Below are the commands to configure the ORE build using cmake:

    mkdir $ORE_ROOT_DIR/build
    cd $ORE_ROOT_DIR/build
    cmake ..

## Build ORE

Below are the commands to build ORE.

    cd $ORE_ROOT_DIR/build
    cmake --build .

# Run tests

## QuantLib

cd to the test directory:

    cd $ORE_ROOT_DIR/build/QuantLib/test-suite

Run all unit tests:

    ./quantlib-test-suite

Run a selected test, e.g:

    ./quantlib-test-suite --log_level=all --run_test="QuantLib test suite/Swap tests"

## QuantExt

    cd $ORE_ROOT_DIR/build/QuantExt/test
    ./quantext-test-suite

## OREData

    cd $ORE_ROOT_DIR/build/OREData/test
    ./ored-test-suite

## OREAnalytics

    cd $ORE_ROOT_DIR/build/OREAnalytics/test
    ./orea-test-suite

