# Exposure Examples

This folder contains a set of examples that demonstrate exposure simulation
and XVA for uncollateralised single trades across ORE's product range.

The examples can be run individually (see below) or all together (<code>python run.py</code>).

See the user guide for a discussion of each case and expected results.


## Cases demonstrating vanilla products:
- Swap with flat yield curve: <code>python run_swapflat.py</code>
- Swap with normal yield curves: <code>python run_swap.py</code>
- FRA: <code>python run_fra.py</code>
- European/American/Bermudan Swation and CallableSwap: <code>python run_swaption.py</code>
- Caps/Floors: <code>python run_capfloor.py</code>
- FX Forward and FX Option: <code>python run_fx.py</code>
- Resetting and Non-Resetting Cross Currency Swaps: <code>python run_ccs.py</code>
- Equity Forwards and Option: <code>python run_equity.py</code>
- Commodity Forward, Option, Swaption, APO: <code>python run_commodity.py</code>
- Inflation CPI and YOY Swap, the simulation is run twice using a Dodgson-Kainth and Jarrow-Yildirim model: <code>python run_inflation.py</code>
- Credit Default Swap: <code>python run_credit.py</code>


## More complex payoffs:
- Capped/Floored CMS Spread, Digital CMS Spread: <code>python run_cmsspread</code> 
- Capped CMS Spread and Digital CMS Spread with formula-based payoff (slow): <code>python run_fbc.py</code>

Further complex products will be covered in the American Monte Carlo Examples section


## Features:
- Long-term simulation with and without horizon shift in the Linear Gauss Markov model: <code>python run_longterm.py</code>
- Simulation in different measures: <code>python run_measures.py</code>
- Simulation in the two-factor Hull-White model: <code>python run_hw2f.py</code>
- Wrong-Way-Risk: <code>python run_wwr.py</code>
- Flip View, switch perspectives easily for XVA: <code>python run_flipview.py</code>

## Calibrations:
- HW n-factor historical calibration : <code> python run_hwhistoricalcalibration.py</code>


