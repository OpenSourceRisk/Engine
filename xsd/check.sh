#!/bin/sh

echo Conventions:
/bin/echo -n "Checking ... "
xmllint --schema conventions.xsd --path xsd --noout Input/conventions.xml
echo -------

echo CurveConfig:
/bin/echo -n "Checking ... "
xmllint --schema curveconfig.xsd --path xsd --noout Input/curveconfig.xml
echo -------

echo Portfolio Files Examples:
find . -name 'portfolio*.xml' -print0 | while read -d $'\0' file
do
  /bin/echo -n "Checking ... "
  xmllint --schema instruments.xsd --path xsd --noout $file
done
echo -------


echo Netting Set Files Examples:
find . -name 'netting*.xml' -print0 | while read -d $'\0' file
do
  /bin/echo -n "Checking ... "
  xmllint --schema nettingsetdefinitions.xsd --path xsd --noout $file
done
echo -------
