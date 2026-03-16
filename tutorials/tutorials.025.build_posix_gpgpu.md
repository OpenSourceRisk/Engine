
# Building ORE with GPGPU Support on Posix Systems (e.g. MacOS and Linux)

This tutorial explains how to build ORE with
[GPGPU](https://en.wikipedia.org/wiki/General-purpose_computing_on_graphics_processing_units)
support on Windows.

[Back to tutorials index](../tutorials_index.md)

This build is based upon the vanilla Posix build of ORE:

[Building ORE on Posix Systems (e.g. MacOS and Linux)](tutorials.015.build_posix.md)

With the differences noted below:

# Build ORE

## Configure ORE

When you run cmake, add the flag `-DORE_ENABLE_OPENCL=ON`:

    cmake .. -DORE_ENABLE_OPENCL=ON

# Run tests

## QuantExt

Below are the commands to run the unit tests for the ORE GPGPU Framework:

    cd $ORE_ROOT_DIR/build/QuantExt/test
    ./quantext-test-suite --log_level=message --run_test=QuantExtTestSuite/ComputeEnvironmentTest

# Design

Below is a UML Class Diagram for the ORE GPGPU Framework:

![ORE GPGPU Framework](ore_gpgpu.png "ORE GPGPU Framework")

