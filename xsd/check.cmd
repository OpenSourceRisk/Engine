@echo off

REM Batch file to validate ORE XML files. 
echo Perform schema validation ...
echo.
for /r %~dp0..\Examples %%i in (*.xml) do (xmllint --schema %~dp0input.xsd --noout %%i)
for /r %~dp0..\OREAnalytics\test\input %%i in (*.xml) do (xmllint --schema %~dp0input.xsd --noout %%i)
for /r %~dp0..\OREData\test\input %%i in (*.xml) do (xmllint --schema %~dp0input.xsd --noout %%i)
for /r %~dp0..\QuantExt\test\input %%i in (*.xml) do (xmllint --schema %~dp0input.xsd --noout %%i)
