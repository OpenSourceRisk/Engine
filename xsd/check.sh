#!/bin/sh

echo Conventions:
/bin/echo -n "Checking ... "
xmllint --schema conventions.xsd --path xsd --noout Examples/Input/conventions.xml
echo -------

echo CurveConfig:
/bin/echo -n "Checking ... "
xmllint --schema curveconfig.xsd --path xsd --noout Examples/Input/curveconfig.xml
echo -------

echo Portfolio Files Examples:
find Examples -name 'portfolio*.xml' -print0 | while read -d $'\0' file
do
  /bin/echo -n "Checking ... "
  xmllint --schema instruments.xsd --path xsd --noout $file
done
echo -------

echo Netting Set Files Examples:
find Examples -name 'netting*.xml' -print0 | while read -d $'\0' file
do
  /bin/echo -n "Checking ... "
  xmllint --schema nettingsetdefinitions.xsd --path xsd --noout $file
done
echo -------

echo Simulation Files Examples:
find Examples -name 'simulation*.xml' -print0 | while read -d $'\0' file
do
  /bin/echo -n "Checking ... "
  xmllint --schema simulation.xsd --path xsd --noout $file
done
echo -------

echo Pricing Engine Files Examples:
find Examples -name 'pricingeng*.xml' -print0 | while read -d $'\0' file
do
  /bin/echo -n "Checking ... "
  xmllint --schema pricingengines.xsd --path xsd --noout $file
done
echo -------

echo Todays Market Files Examples:
find Examples -name 'todaysmarket*.xml' -print0 | while read -d $'\0' file
do
  /bin/echo -n "Checking ... "
  xmllint --schema todaysmarket.xsd --path xsd --noout $file
done
echo -------

echo ORE Examples:
find Examples -name 'ore*.xml' -print0 | while read -d $'\0' file
do
  /bin/echo -n "Checking ... "
  xmllint --schema ore.xsd --path xsd --noout $file
done
echo -------
