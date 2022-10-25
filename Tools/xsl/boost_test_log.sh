#!/bin/sh

sed -i -e 's/<TestLog>//g' $1*.xml
sed -i -e 's/<\/TestLog>//g' $1*.xml
find . -name "$1_*.xml" | xargs cat > $1.xml
find . -name "$1_*.xml" | xargs rm
sed -i '1i <TestLog>' $1.xml
sed -i '$a</TestLog>' $1.xml
