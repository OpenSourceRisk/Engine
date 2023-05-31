mkdir build
rem amend the BOOST variables to your environment
cmake -G "Visual Studio 17 2022" -A x64 -DBOOST_INCLUDEDIR=%BOOST% -DBOOST_LIBRARYDIR=%BOOST_LIB64% -DENABLE_SESSIONS=ON -B build
cmake --build build -v --config Release
pause
