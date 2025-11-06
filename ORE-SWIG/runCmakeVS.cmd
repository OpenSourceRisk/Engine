REM This scripts build ORE and ORE_SWIG
REM Adjust paths
SET GENERATOR="Visual Studio 17 2022"
SET DEMO_BOOST_ROOT=C:\local\boost_1_72_0
SET DEMO_BOOST_LIB=C:\local\boost_1_72_0\lib64-msvc-14.2
SET DEMO_SWIG_DIR=C:\dev\swigwin-4.1.1
SET DEMO_ORE_DIR=C:\dev\ore
SET DEMO_ORE_SWIG_DIR=C:\dev\oreswig
REM the next path is optional, but if not set, one need to set -DORE_USE_ZLIB=OFF
SET DEMO_ZLIB_ROOT=C:\dev\vcpkg\packages\zlib_x64-windows
REM SET VCPKG_ROOT=C:\dev\vcpkg
REM If you use VCPKG for zlib and eigen, add -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake to the cmake command


REM building ORE, one can skip this part, make sure to delete the build dir in the ORE_DIR if there is any
REM it is recommended to install the eigen and zlib library on the system before building ORE
ECHO BUILD ORE
cd %DEMO_ORE_DIR%
mkdir build
cd %DEMO_ORE_DIR%\build
SET BOOST_INCLUDEDIR=%DEMO_BOOST_ROOT%
SET BOOST_LIBRARYDIR=%DEMO_BOOST_LIB%
SET ZLIB_ROOT=%DEMO_ZLIB_ROOT%
cmake -G %GENERATOR% -A x64 .. -DMSVC_LINK_DYNAMIC_RUNTIME=OFF -DORE_BUILD_DOC=OFF -DORE_BUILD_EXAMPLES=OFF -DORE_BUILD_TESTS=OFF -DORE_BUILD_APP=OFF -DQL_BUILD_BENCHMARK=OFF -DQL_BUILD_EXAMPLES=OFF -DQL_BUILD_TEST_SUITE=OFF -DCMAKE_BUILD_TYPE=Release -DORE_USE_ZLIB=ON -DQL_ENABLE_SESSIONS=ON -DBoost_NO_SYSTEM_PATHS=ON

cd %DEMO_ORE_DIR%\build
cmake --build . --config Release

ECHO BUILD ORESWIG
cd %DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python
mkdir build
cd build
SET BOOST_INCLUDEDIR=%DEMO_BOOST_ROOT%
SET BOOST_LIBRARYDIR=%DEMO_BOOST_LIB%
SET SWIG_ROOT=%DEMO_SWIG_DIR%
SET ZLIB_ROOT=%DEMO_ZLIB_ROOT%
cmake -Wno-dev -G %GENERATOR% -A x64 .. -DORE=%DEMO_ORE_DIR% -DMSVC_LINK_DYNAMIC_RUNTIME=OFF -DORE_USE_ZLIB=ON -DBoost_NO_SYSTEM_PATHS=ON
cmake --build . --config Release

REM now set your python path to build dir
SET PYTHONPATH=%DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\build;%DEMO_ORE_SWIG_DIR%\OREAnalytics-SWIG\Python\build\Release

