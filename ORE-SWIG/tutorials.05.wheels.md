
# Managing Wheels

This document contains some notes regarding the management of wheels.  The
basic process of generating wheels on Github Actions is documented elsewhere
and is not repeated here.  This document contains some useful information
regarding specific details of managing Python wheels for the ORE project.  The
commands listed here are for Ubuntu, the approach is similar for Windows and
MacOS.

[Back to tutorials index](tutorials.00.index.md)

# Generating Source Tarballs

We need to upload the ORE and ORESWIG source to the GitHub Actions jobs which
build the wheels.  Here are the commands for generating the tarballs.

    pip install git-archive-all
    cd $DEMO_ORE_DIR
    git-archive-all ore.tgz
    mv ore.tgz ~/projects/wheels/tarballs
    cd $DEMO_ORE_SWIG_DIR
    git-archive-all oreswig.tgz
    mv oreswig.tgz ~/projects/wheels/tarballs

# Running the Wrapper

We do not install swig to the Github Actions Runner.  Instead, before uploading
the ORESWIG tarball, we uncompress it, run the swig wrapper, and then compress
it again.  Here are the commands to run the wrapper:

    cd ~/projects/wheels/tarballs/oreswig/OREAnalytics-SWIG/Python
    export BOOST_INC=$DEMO_BOOST_INC
    export BOOST_LIB=$DEMO_BOOST_LIB
    export ORE=$DEMO_ORE_DIR
    python3 setup.py wrap

# Uploading Wheels to the Test Server

The test server for Python wheels is: https://test.pypi.org/

The upload command relies on twine which uses the following configuration file to authenticate to the server:

    ~/.pypirc

Here are the commands to upload the wheels to the test server:

    python3 -m pip install --upgrade pip
    python3 -m pip install --upgrade twine
    cd ~/projects/wheels
    python3 -m twine upload --repository testpypi *.whl

Here is the command to install the wheel from the test server (run this within a virtual environment):

    pip install -i https://test.pypi.org/simple/ open-source-risk-engine

# Uploading Wheels to the Production Server

The production server for Python wheels is: https://pypi.org/

The command to upload the wheels to the production server is:

    twine upload -r pypi *whl

Here is the command to install the wheel from the production server (run this within a virtual environment):

    pip install open-source-risk-engine

# Running cibuildwheels Locally

Github Actions uses cibuildwheels to generate the wheels.  For troubleshooting purposes, it can be helpful to run cibuildwheels locally.  Here are the commands to do that:

    cd $DEMO_ORE_SWIG_DIR/OREAnalytics-SWIG/Python
    python3 -m venv env1
    . ./env1/bin/activate
    pip install cibuildwheel
    export CIBW_BUILD='cp310-manylinux_x86_64'
    export CIBW_BUILD_VERBOSITY='2'
    export CIBW_ENVIRONMENT_LINUX='CXXFLAGS="-O3 -g0"'
    export CIBW_ENVIRONMENT_PASS_LINUX='CXXFLAGS'
    export CIBW_BEFORE_ALL_LINUX='./before_all_linux.local.sh'
    export CIBW_REPAIR_WHEEL_COMMAND_LINUX='LD_LIBRARY_PATH=/host~/erik/quaternion/boost_1_81_0/stage/lib auditwheel repair -w {dest_dir} {wheel}'
    cibuildwheel --platform linux
    deactivate
    rm -rf env1

