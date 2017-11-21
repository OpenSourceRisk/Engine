REM Batch file to validate ORE XML files. 
@echo off

echo Conventions:
echo Checking ... 
xmllint --schema input.xsd --noout ..\\Examples\Input\conventions.xml
echo ------------------------------------------------------------------
echo.

echo CurveConfig:
echo Checking ... 
xmllint --schema input.xsd --noout ..\\Examples\Input\curveconfig.xml
echo ------------------------------------------------------------------
echo.

echo Portfolio Files Examples:
FOR /R "..\\Examples" %%I IN (portfolio*.xml) DO (xmllint --schema input.xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Netting Set Files Examples:
FOR /R "..\\Examples" %%I IN (netting*.xml) DO (xmllint --schema input.xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Simulation Files Examples:
FOR /R "..\\Examples" %%I IN (simulation*.xml) DO (xmllint --schema input.xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Pricing Engine Files Examples:
FOR /R "..\\Examples" %%I IN (pricingeng*.xml) DO (xmllint --schema input.xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Todays Market Files Examples:
FOR /R "..\\Examples" %%I IN (todaysmarket*.xml) DO (xmllint --schema input.xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Sensitivities Files Examples:
FOR /R "..\\Examples" %%I IN (sensi*.xml) DO (xmllint --schema input.xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo Sensitivities Files Examples:
FOR /R "..\\Examples" %%I IN (stress*.xml) DO (xmllint --schema input.xsd --noout %%I)
echo ------------------------------------------------------------------
echo.

echo ORE Examples:
FOR /R "..\\Examples" %%I IN (ore*.xml) DO (xmllint --schema input.xsd --noout %%I)
echo ------------------------------------------------------------------

