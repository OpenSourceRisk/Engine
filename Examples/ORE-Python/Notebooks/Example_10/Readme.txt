Example 10 - Cross-Currency Swap Pricing with the ORE Python Addin

Prices a 1-year EUR/USD cross-currency (xccy) basis swap using the ORE SWIG
Python wrapper, mirroring discovery2.ipynb cell by cell while using the
file-based OREApp(Parameters) workflow of the other Example_N notebooks.

Demonstrates:
  - ZeroRateConvention, FXConvention, CrossCcyBasisSwapConvention in Python
  - EngineData, CurveConfigurations, TodaysMarketParameters in Python
  - ScheduleData, FloatingLegData, LegData, ORESwap: programmatic trade
  - OREApp(Parameters) file-based NPV and stress analytics
  - StressTestScenarioData, ScenarioSimMarketParameters object API
  - Portfolio.fromFile and trade introspection
  - CSV report extraction into pandas DataFrames

Prerequisites:
  - Python 3.12 (64-bit)
  - ORE SWIG extension built; set ORESWIG_PKG, e.g.:
      set ORESWIG_PKG=<build>\ORE-SWIG;<build>\ORE-SWIG\Release
  - pip install jupyter pandas lxml setuptools

Market data: Input/
Output logs and CSV: Output/
