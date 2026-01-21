# Market Risk

The examples in this folder demonstrate ORE usage in the Market Risk context.
See the user guide for a discussion of each case below.


## Sensitivity Analysis

A moderately sized and diverse portfolio (Vanilla Swaps, Cross Currency Swaps, European Swaption,
FX Forward and Options, Equity Forward and Options, Bond, CPI and YoY Inflation Swaps) is run
through ORE's sensitivity analysis including par conversion.

Run with <code>python run_sensi.py</code>


## Sensitivity Analysis with Smile

We focus on sensitivity analysis of European Options priced with volatility smile.

Run with <code>python run_sensismile.py</code>


## Parametric VaR

Based on the sensisitivities precalculated above, we demonstrate a parametric VaR
calculation (here Delta Gamma Normal VaR) with an external covariance matrix
(see Input/covariance.csv).

Run with <code>python run_parametricvar.py</code>


## Stress Testing

Using the same portfolio as in the Sensitivity Analysis case above we demonstrate
a stress test run.

Run with <code>python run_stress.py</code>


## Stress Testing in the Par Rate Domain

Focussing on a Cross Currency Swap, Interest Rate Swap, Cap and CDS we demonstrate
a stress test defined in the par rate domain. This test is internally converted into the
"raw" domain (of zero rates, optionlet vols and survival probabilities) with the
help of the sensitivity config provided, and then the usual stress test machinery above
is applied.

Run with <code>python run_parstress.py</code>


## Stressed Sensitivity Analysis

Using the same portfolio as in the Sensitivity Analysis case above we demonstrate
a sensitivity test run after applying stress scenario to T0 market.

Run with <code>python run_sensistress.py</code>


## Historical Simulation VaR

This case demonstrates the HistSim analytic based on precalculated historical market scenarios
(in Input/HistSimVar/scenarios.csv).

Run with: <code>python run_histsimvar.py</code>


## Basic Market Risk Capital: SMRC

Run with: <code>python run_smrc.py</code>


## P&L and P&L Explain

Using simplistic trades (Single-leg Swaps) we demsontrate P&L calculationwith and without
cash flows in the P&L period, P&L explanation (analysing the effect of market moves by full
revaluation and sensitivity-based, in terms of "raw" and par sensivitities).

Run with: <code>python run_pnlexplain.py</code>


## Par Conversion

Converts externally provided or precalculated raw sensitivities into par sensitivities.

Run with: <code>python run_parconversion.py</code>


## Base Scenario Export

This case shows how to extract a "base scenario" from ORE. This is a market snapshot extracted from
ORE's "simulation market" in a standardised format. This is e.g. used internally in the P&L
analytics. It can also be used to compile -- date by date -- the scenario file used
in the Historical Simulation VaR calculation.

Run with: <code>python run_basescenario.py</code>


## Conversion of Zero Rate Shifts to Par Rate Shifts

Utility used to construct par rate shifts from provided zero rate shifts (hypothetical or historical).
The zero rate shifts are defined as stress tests. This functionality is used internally in the
par-sensitivity-based P&L explainer.

Run with: <code>python run_zerotoparshift.py</code>

## Sensitivity Analysis with credit index decomposition

Decompose credit index sensitivities into basket constituent sensitivities, decomposition method is controlled by 
pricing engine setting.

Run with: <code>python run_sensi_cr_decomp.py</code>

## Sensitivity and Stress Analysis with curve algebra for commodity basis curves

When the base curve is shifted in sensitivity or stresstest scenario, the basis curve will be shifted along when 
curve algebra is defined in the simulation market config. The test portfolio contains of a basis swap, which should generate
only a sensitivty under the basis shift and a option on the derived instrument which should show identical sensitivity on the base and basis curve.

Run with: <code>python run_curvealgebra.py</code>