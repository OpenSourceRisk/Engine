#!/bin/bash

pip3 install docker
cd ORE-SWIG
cp Wheels-gitlab/oreanalytics-config .
chmod +x oreanalytics-config
python3 setup.py wrap

