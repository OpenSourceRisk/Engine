#!/bin/sh

max=8
i=1

until [ $i -gt  $max ]
do
    echo Running Example_$i...
    cd Example_$i
    sh run.sh
    echo ====================
    echo
    cd ..
    i=`expr $i + 1`
done

