## Initial Margin Examples

### ISDA SIMM and IM Schedule

The first batch of cases in this directory performs ISDA SIMM calculations
based on a single CRIF file (par sensitivities in ISDA format) that is
provided as input. We generate SIMM reports for several SIMM versions
and for both 1d and 10d margin period of risk. This batch also performs an
IM Schedule calculation based on a "Schedule CRIF" file that contains minimal
trade information including notional, maturity and NPV.

Run this batch with <code>python run_simm.py</code>


### Dynamic Initial Margin and MVA

The second batch is a study of Dynamic Initial Margin for single trade portfolios
(Single Currency Swaps, Cross Currency Swap, European Swaption, FX Option).
Dynamic Initial Margin is computed using regression approaches and, as a
benchmark, using a Dynamic Delta VaR calculation under scenarios.

Run this batch with <code>python run_dim.py</code>


### Dynamic SIMM using AAD Sensitivities under AMC

This batch demonstrades a SIMM calculation under AMC scenarios with sensitivities
computed using AAD. This fast dynamic SIMM is validated against
(significantly slower) full SIMM calculations along a small number of MC paths.
As test portfolio we start with the same IR/FX portfolio as above, adding
Bermudan Swaption and FX TaRF to demonstrate the wider product scope of
Dynamic SIMM in ORE. We furthermore compare results to the regression DIM.

Run this batch with <code>python run_dim2.py</code>

See the user guide for a discussion of each case and expected results.

