# FIXME: fix the rpath link to the python runtime lib of binary given as argument 1 (known issue on osx), path needs to be edited here
install_name_tool -change @rpath/Python3.framework/Versions/3.9/Python3 /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/lib/libpython3.9.dylib $1
