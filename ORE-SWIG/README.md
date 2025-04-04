
ORE-SWIG: Language bindings for ORE
===================================

The [Open Source Risk](http://opensourcerisk.org) project aims at
establishing a transparent peer-reviewed framework for pricing and risk
analysis. Open Source Risk Engine (ORE) is based on QuantLib
(<http://quantlib.org>) and consists of three libraries written in
C++.

Similar to QuantLib-SWIG, ORE-SWIG intends to provide the means
to use the ORE libraries side by side with QuantLib from a
number of languages including Python, Ruby, Perl, Java and C#.

Tutorials
---------

For a series of in depth tutorials on installing and/or building python
wrappers and wheels, please refer to this document: [Tutorials
Index](tutorials.00.index.md).  For a general overview on using the ORE python
wrapper, continue reading below.

Download and usage
------------------

ORE and ORE-SWIG can be downloaded from the
[OpenSourceRisk](http://opensourcerisk.org) project site which points
to the Open Source Risk repositories on [GitHub](http://github.com/OpenSourceRisk).

The ORE-SWIG project directory contains QuantExt-SWIG, OREData-SWIG and
OREAnalytics-SWIG folders. Within the ORE-SWIG project directory, pull
in the QuantLib-SWIG project by running

    git submodule init
    git submodule update

Prerequisite for building the wrappers is building the latest ORE release
following the steps outlined in ORE's user guide, e.g. using CMake.

To build ORE-SWIG on Windows, macOS or Linux using CMake and
Ninja: Edit the top level CMakeLists.txt to select/deselect
specific library/language folders, create a subdirectory "build",
change to the build directory, and run

    cmake \
        -G Ninja \
        -D ORE=<ORE Root Directory> \
        [-D BOOST_ROOT=<Top level boost include directory> ] \
        [-D BOOST_LIB=<Location of the compiled boost libraries> ] \
        [-D PYTHON_LIBRARY=<Full name including path to the 'libpython*' library> ]    \
        [-D PYTHON_INCLUDE_DIR=<Directory that contains Python.h> ] \
        ..
    ninja

To build on Windows using CMake and an existing Visual Studio installation you can e.g. run
this from the top-level oreswig directory

    mkdir builddir
    cmake -G "Visual Studio 17 2022" \
          -A x64 \
          -D SWIG_DIR=C:\dev\swigwin\Lib \
          -D SWIG_EXECUTABLE=C:\dev\swigwin\swig.exe \
          -D ORE=<ORE Root Directory> \
          -D BOOST_INCLUDEDIR=<Top level boost include directory> \
          -D BOOST_LIBRARYDIR=<Location of the compiled boost libraries>\
          -D ORE_USE_ZLIB=[ON/OFF] (depending on your ORE build)
          -D MSVC_LINK_DYNAMIC_RUNTIME=[ON/OFF] (depending on your ORE build)
          -D Boost_NO_SYSTEM_PATHS=ON
          -S OREAnalytics-SWIG/Python \
          -B builddir
    cmake --build builddir -v

To try e.g. an OREAnalytics Python example, update your PYTHONPATH so
that it includes the directory that contains the newly built python module and
associated native library (both in
ORE-SWIG/build/OREAnalytics-SWIG/Python), change to
ORE-SWIG/OREAnalytics-SWIG/Python/Examples and run

    python ore.py

You can also try the IPython example in the same directory: Launch

    jupyter notebook

wait for your browser to open, select ore.ipy from the list of files
and then run all cells.

Also note the test suite in ORE-SWIG/OREAnalytics-SWIG/Python/test:

    python OREAnalyticsTestSuite.py

When the QuantExt-SWIG and OREData-SWIG modules are built as well (edit top-level CMakeLists.txt)
then similar test suites in ORE-SWIG/OREData-SWIG/Python/test and ORE-SWIG/QuantExt-SWIG/Python/test
can be run

    python OREDataTestSuite.py
    python QuantExtTestSuite.py

To try a simple OREAnalytics Java example, change to
ORE-SWIG/OREAnalytics-SWIG/Java/Examples and run

    java -Djava.library.path=../../../build/OREAnalytics-SWIG/Java \
        -jar ../../../build/OREAnalytics-SWIG/Java/ORERunner.jar \
        Input/ore.xml

Python Bindings on Windows
--------------------------

On Windows you can also use the following steps to build the Python
bindings using the provided setup.py script:

    1. Include SWIG path to the Path environment variable, e.g.
       set Path=%Path%;C:\swigwin-4.3.0

    2. Add PYTHON_INCLUDE and PYTHON_LIB variables to the system environment, e.g.
       set PYTHON_INCLUDE="C:\Users\Name\AppData\Local\Continuum\anaconda3\include"
       set PYTHON_LIB="C:\Users\Name\AppData\Local\Continuum\anaconda3\libs"

    3. Add BOOST_ROOT and BOOST_LIB variables to the system environment, e.g.
       set BOOST_ROOT=C:\repos\boost_1_65_1
       set BOOST_LIB=C:\repos\boost_1_65_1\lib\x64\lib\lib

    4. Add ORE_DIR and QL_DIR variables to the system environment, e.g.
       set ORE_DIR=C:\dev\ORE

    5. Change to directory OREAnalytics-SWIG/Python and run the following
       python scripts to build and install ORE Analytics Python module:
       cd Python
       python setup.py wrap
       python setup.py build
       python setup.py install

    6. Try examples (all work):
       cd Examples
       python ore.py
       python swap.py
       python market.py
       python commodityforward.py
       python conventions.py
       python portfolio.py

Contributing
------------

ORE-SWIG initially contains the framework for building the wrappers
including QuantLib wrappers and a limited number of wrapped classes in
each of the three ORE libraries to demonstrate the principle. Overall, ORE
contains more than 500 classes that could be added to ORE-SWIG over
time.

The easiest way to contribute additional interface files is through
pull requests on GitHub.  Get a GitHub account if you don't have it
already and clone the repository at
<https://github.com/OpenSourceRisk/ORE-SWIG> with the "Fork" button
in the top right corner of the page. Check out your clone to your
machine, code away, push your changes to your clone and submit a pull
request; instructions are available at
<https://help.github.com/articles/fork-a-repo>.  (In case you need
them, more detailed instructions for creating pull requests are at
<https://help.github.com/articles/using-pull-requests>, and a basic
guide to GitHub is at
<https://guides.github.com/activities/hello-world/>.

We're looking forward to your contributions.
