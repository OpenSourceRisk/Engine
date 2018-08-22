@echo off

REM Batch file to validate ORE XML files. 
echo Perform schema validation ...
echo.
for /r %~dp0..\Examples %%i in (*.xml) do (xmllint --schema %~dp0input.xsd --noout %%i)
