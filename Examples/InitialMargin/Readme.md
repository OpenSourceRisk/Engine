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

This example demonstrates a SIMM calculation under AMC scenarios with sensitivities
computed using AAD for a Bermudan Swaption case, comparing exposure without collateral,
exposure with variation margin only and both variation and initial margin.

Run this batch with <code>python run_dim2.py</code>

See also the related Jupyter notebook with <code>python -m jupyterlab ore_dynamicsimm.ipynb</code>
with brief discussion.


### Dynamic SIMM Benchmarking

This section also contains tools for validating Dynamic SIMM against several benchmarks
- full SIMM calculations along a small number of MC paths, using a limited IR/FX CRIF
generator applied to simulated market data in Input/DimValidation;
- dynamic SIMM based on analytical sensitivities rather than AAD sensitivities
(see Input/Dim2/ore_benchmark.xml);
- simple regression DIM

The benchmarking involves various ORE runs for market generation, pathwise SIMM comparisons
and SIMM distribution comparisons, all orchestrated by
<code>run_dynamicsimm_benchmark.py</code> and <code>run_simmcube.py</code>.


