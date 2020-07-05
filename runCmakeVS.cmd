mkdir build
cmake -G "Visual Studio 16 2019" -A x64 -D BOOST_LIBRARYDIR=C:\dev\boost\lib -DBOOST_ROOT=C:\dev\boost -B build
cmake --build build -v --config Release
pause
