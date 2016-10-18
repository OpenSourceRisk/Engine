REM Batch file to validate ORE XML files. 
@echo off

echo Conventions:
echo Checking ... 
xmllint --schema conventions.xsd --path xsd --noout ..\\Examples\Input\conventions.xml
echo ------------------------------------------------------------------
echo.

echo CurveConfig:
echo Checking ... 
xmllint --schema curveconfig.xsd --path xsd --noout ..\\Examples\Input\curveconfig.xml
echo ------------------------------------------------------------------
echo.

echo Portfolio Files Examples:
FOR /R "..\\Examples" %%I IN (portfolio*.xml) DO (xmllint --schema instruments.xsd --path xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Netting Set Files Examples:
FOR /R "..\\Examples" %%I IN (netting*.xml) DO (xmllint --schema nettingsetdefinitions.xsd --path xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Simulation Files Examples:
FOR /R "..\\Examples" %%I IN (simulation*.xml) DO (xmllint --schema simulation.xsd --path xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Pricing Engine Files Examples:
FOR /R "..\\Examples" %%I IN (pricingeng*.xml) DO (xmllint --schema pricingengines.xsd --path xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Todays Market Files Examples:
FOR /R "..\\Examples" %%I IN (todaysmarket*.xml) DO (xmllint --schema todaysmarket.xsd --path xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo ORE Examples:
FOR /R "..\\Examples" %%I IN (ore*.xml) DO (xmllint --schema ore.xsd --path xsd --noout %%I)
echo ------------------------------------------------------------------

