# configure gcc, no open mp support
./configure CXXFLAGS="-m64 -O3 -g -Wall -std=c++11 -Qunused-arguments -Wno-unused-local-typedef"
