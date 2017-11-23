REM Batch file to validate ORE XML files. 
@echo off


echo Portfolio Files Examples:
FOR /R "..\\Examples" %%I IN (*.xml) DO (xmllint --schema input.xsd --noout %%I)
echo ------------------------------------------------------------------
