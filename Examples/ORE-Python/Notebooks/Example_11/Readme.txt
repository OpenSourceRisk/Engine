Example 11 - PnL SWIG API Showcase

Demonstrates:
  - SensitivityInMemoryStream iteration with SensitivityRecord
  - MarketRiskConfiguration enums and RiskFilter usage
  - HistoricalScenarioLoader and HistoricalScenarioGenerator
  - HistoricalSensiPnlCalculator, PNLCalculator, CovarianceCalculator
  - Reusing Example_8/Example_62 detailed market risk XML inputs for InputParameters
  - PnlAnalytic / PnlExplainAnalytic construction and helper symbol availability

Prerequisites:
  - Python 3 (64-bit)
  - ORE Python package: pip install open-source-risk-engine
    or set ORESWIG_PKG to a local ORE-SWIG build directory, e.g.:
      set ORESWIG_PKG=<build>\ORE-SWIG;<build>\ORE-SWIG\RelWithDebInfo
  - pip install jupyter pandas

Output logs: Output/
