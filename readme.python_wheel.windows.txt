
1. Introduction
================

This document provides a HOWTO for building and using a python wheel for OREAnalytics.

1.1 Other documentation

This document is fairly terse.  Refer to the resources below for more extensive information on the build and on the SWIG wrappers.

ore\userguide.pdf
ore-swig\README
ore-swig\README.md
ore-swig\OREAnalytics-SWIG\README

1.2 Prerequisites

- python - This HOWTO will require you to have the following tools up to date:

python -m ensurepip
pip install build

- boost and swig: You need to either install the binaries,
  or install the source code and build yourself

- ore and oreswig: You need to install the source code.
  Instructions for building with cmake are provided below.

1.3 Environment variables

For purposes of this HOWTO, set the following environment variables to the paths where the above items live on your machine, e.g:

SET DEMO_BOOST_ROOT=C:\ORE\repos\boost\boost_1_81_0
SET DEMO_BOOST_LIB=C:\ORE\repos\boost\boost_1_81_0\lib\x64\lib
SET DEMO_SWIG_DIR=C:\ORE\repos\swigwin-4.1.1
SET DEMO_ORE_DIR=C:\ORE\repos\ore
SET DEMO_ORE_SWIG_DIR=C:\ORE\repos\oreswig

2. Build ORE
============

2.1 Generate the project files

cd %DEMO_ORE_DIR%
mkdir build
cd %DEMO_ORE_DIR%\build
set BOOST_INCLUDEDIR=%DEMO_BOOST_ROOT%
set BOOST_LIBRARYDIR=%DEMO_BOOST_LIB%
"C:\Program Files\CMake\bin\cmake.exe" -DMSVC_LINK_DYNAMIC_RUNTIME=OFF -DBoost_NO_WARN_NEW_VERSIONS=1 -DORE_USE_ZLIB=OFF -Wno-dev -G "Visual Studio 17 2022" -A x64 ..
-> %DEMO_ORE_DIR%\build\ORE.sln

2.2 Build

cd %DEMO_ORE_DIR%\build
"C:\Program Files\CMake\bin\cmake.exe" --build . --config Release
-> %DEMO_ORE_DIR%\build\OREAnalytics\orea\Release\OREAnalytics-x64-mt.lib

3. Build ORE-SWIG
=================

3.1 Build ORE-SWIG (wrapper and wheel)

cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python
set BOOST_ROOT=%DEMO_BOOST_ROOT%
set BOOST_LIB=%DEMO_BOOST_LIB%
set ORE_DIR=%DEMO_ORE_DIR%
set PATH=%PATH%;%DEMO_SWIG_DIR%
set ORE_STATIC_RUNTIME=1
python setup.py wrap
python setup.py build
python -m build --wheel

3.2 Use the wrapper

cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\Examples
set PYTHONPATH=%DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\build\lib.win-amd64-cpython-310
python commodityforward.py

3.3 Use the wheel

cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\Examples
python -m venv env1
.\env1\Scripts\activate.bat
pip install %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\dist\osre-1.8.3.2-cp310-cp310-win_amd64.whl
python commodityforward.py
deactivate
rmdir /s /q env1

