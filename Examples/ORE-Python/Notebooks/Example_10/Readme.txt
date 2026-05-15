Example 10 - Cross-Currency Swap Pricing with the ORE Python Addin

Prices a 1-year EUR/USD cross-currency (xccy) basis swap using the ORE SWIG
Python wrapper.  All market configurations and market data are defined
entirely in Python — no XML configuration files or external data files are
required.

Demonstrates:
  - ZeroRateConvention, FXConvention in Python
  - EngineData, CurveConfigurations, TodaysMarketParameters in Python
  - ScheduleData, FloatingLegData, LegData, ORESwap: programmatic trade
  - InputParameters + OREApp + MarketDataInMemoryLoader: NPV analytics
  - StressTestScenarioData, ScenarioSimMarketParameters object API
  - In-memory report extraction into pandas DataFrames

Prerequisites:
  - Python 3 (64-bit)
  - ORE Python package: pip install open-source-risk-engine
    or set ORESWIG_PKG to a local ORE-SWIG build directory, e.g.:
      set ORESWIG_PKG=<build>\ORE-SWIG;<build>\ORE-SWIG\Release
  - pip install jupyter pandas

Output logs: Output/
