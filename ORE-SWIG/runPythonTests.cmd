@echo off
setlocal

REM Run the Python test suites against a locally built ORE module.
REM Picks up the setup.py build output; harmless no-op if you installed
REM the package instead with `python setup.py install`.
for /d %%d in ("%~dp0build\lib.*") do set "PYTHONPATH=%PYTHONPATH%;%%d"

cd /d "%~dp0test"

echo RUN ORE Python Testsuite
python OREAnalyticsTestSuite.py
if errorlevel 1 exit /b 1

REM testrunner.py aliases QuantLib to ORE before collection, which the
REM QuantLib-SWIG tests require; they import QuantLib, not ORE.
echo RUN QuantLib Testsuite
python testrunner.py
if errorlevel 1 exit /b 1

echo All Python test suites passed
