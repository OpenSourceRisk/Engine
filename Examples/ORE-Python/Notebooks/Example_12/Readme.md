## Historical Simulation VaR SWIG Workflow

This example demonstrates the public Python/SWIG interface for ORE historical
simulation VaR. The notebook runs both full-revaluation and sensitivity-based
P&L, then inspects the resulting VaR, trade-level P&L, and risk-factor-level
P&L reports.

All required ORE inputs are versioned in the local `Input` directory.

## Prerequisites

- Python 3 with Jupyter installed
- An ORE Python binding available in the active environment (`import ORE`)

## Run the Notebook

From this directory, open `ore.ipynb` in Jupyter and run all cells:

```powershell
python -m jupyter lab
```

To execute it non-interactively:

```powershell
python -m jupyter nbconvert --to notebook --execute --inplace ore.ipynb `
  --ExecutePreprocessor.timeout=1800
```

## Reports

The notebook runs the `HISTSIM_VAR` analytic in full-revaluation and
sensitivity-based modes. It retrieves its analytic-owned reports with
`HistoricalSimulationVarAnalytic.getReport()`:

- `var` contains VaR and expected-shortfall results.
- `historical_PnL` contains rows for each trade, historical scenario, risk class,
  and risk type when `setTradePnl(True)` is enabled. Its first ORE column is
  labelled `Portfolio`, but contains the trade ID in this mode.
- `riskFactor_PnL` contains the risk-factor P&L contribution for each trade and
  historical scenario when `setRiskFactorBreakdown(True)` is enabled.

The notebook displays the head of both raw P&L reports. It also aggregates the
`All`/`All` rows from `historical_PnL` by scenario to plot the portfolio P&L
distribution and calculate 99% VaR and expected shortfall.
