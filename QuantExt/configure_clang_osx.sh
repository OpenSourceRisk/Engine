# configure gcc, no open mp support
./configure CXXFLAGS="-m64 -O3 -g -Wall -std=c++11 -Qunused-arguments -Wno-unused-local-typedef" --with-ql-include=/Users/peter/QuantLib/ --with-ql-lib=/Users/peter/QuantLib/ql/.libs --with-mu-include=/Users/peter/quaternion/dev/muparser-2.2.5/include --with-mu-lib=/Users/peter/quaternion/dev/muparser-2.2.5/lib
