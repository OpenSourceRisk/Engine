set PYTHONPATH=%PYTHONPATH%;%~dp0\OREAnalytics-SWIG\Python\build;%~dp0\OREAnalytics-SWIG\Python\build\Release
echo RUN ORE Python Testsuite
cd %~dp0\OREAnalytics-SWIG\Python\Test
python OREAnalyticsTestSuite.py
pause

echo RUN QuantLib Testsuite
cd %~dp0\QuantLib-SWIG\Python\test
python -c "import sys, ORE; sys.modules['QuantLib']=ORE;import QuantLibTestSuite;QuantLibTestSuite.test()"
pause

cd %~dp0\