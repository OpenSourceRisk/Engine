#!/bin/sh

# Silly little script to copy files from Output to ExpectedOutput, it only copies files that are already
# in ExpectedOutput
# This can be used when we make a code change and lots of the outputs change

for dir in `find . -maxdepth 1 -mindepth 1 -type d`
do
  echo $dir
  cd $dir
  if [[ -d "ExpectedOutput" && -d "Output" ]]
  then
    cd ExpectedOutput
    for file in *
    do
      echo "Updating $dir-$file"
      cd ../Output
      if [ -f "$file" ]
      then
        cp $file ../ExpectedOutput
      fi
      cd ../ExpectedOutput
    done
    cd ..
  fi
  cd ..
done
