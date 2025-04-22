#!/bin/bash

set -e

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "pwd"
pwd
echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"

CURRENT_DIR=$(pwd)

#echo "XYZ BEGIN unpack eigen"
#curl -O -L https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
#tar zxvf eigen-3.4.0.tar.gz
#cd eigen-3.4.0
#mkdir build
#cd build
#cmake ..
#cd ../..
#echo "XYZ END unpack eigen"

#echo "XYZ BEGIN unpack zlib"
#curl -O -L https://www.zlib.net/zlib-1.3.1.tar.gz
#tar xzvf zlib-1.3.1.tar.gz
#cd zlib-1.3.1
#./configure
#make
#cd ..
#echo "XYZ END unpack zlib"

echo "XYZ BEGIN unpack boost"
# Setup Boost
curl -O -L https://archives.boost.io/release/1.80.0/source/boost_1_80_0.tar.gz
tar xfz boost_1_80_0.tar.gz
cd boost_1_80_0
export Eigen3_DIR=$CURRENT_DIR/eigen-3.4.0
./bootstrap.sh --with-libraries=date_time,filesystem,iostreams,log,regex,serialization,system,thread,timer
./b2 install -sZLIB_SOURCE=$CURRENT_DIR/zlib-1.3.1
cd ..
echo "XYZ END unpack boost"

echo "XYZ BEGIN build ORE"
pwd
cd ORE
mkdir build
cd build
cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DORE_USE_ZLIB=ON -DORE_BUILD_SWIG=OFF -DORE_BUILD_DOC=OFF -DORE_BUILD_EXAMPLES=OFF -DORE_BUILD_TESTS=OFF -DORE_BUILD_APP=OFF -DQL_BUILD_BENCHMARK=OFF -DQL_BUILD_EXAMPLES=OFF -DQL_BUILD_TEST_SUITE=OFF -DQL_ENABLE_SESSIONS=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j4
cmake --install .
cd ..
echo "XYZ END build ORE"

echo "XYZ BEGIN wrap oreswig"
pwd
ls
cd ORE-SWIG
python setup.py wrap

