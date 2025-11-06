# XVA Risk

See the user guide for a discussion of each case below and expected results.


## XVA Stress Testing

We run XVA stress tests with two shift scenarios defined in stresstest.xml
- parallel shift of the EURIBOR 6M curve, all par rates 200 bps up 
- parallel shift of the Eonia curve, all par rates 200 bps up
and re-using the "benchmark" portfolio from section AMC.

The test is run twice, using an AMC and a classic simulation.

Run both stress tests with <code>python run_stress.py</code> or inividually with
- <code>ore Input/ore_stress_amc.xml</code>
- <code>ore Input/ore_stress_classic.xml</code>

To replicate the AMC stress test results, run the XVA analysis with the two stressed market inputs provided:
- <code>ore Input/ore_amc_shifted_eonia.xml</code>
- <code>ore Input/ore_amc_shifted_eur6m.xml</code>


## XVA P&L Explain

ORE computes the market implied XVA change between two evaluation dates, with breakdown by risk factors, full revaluation
based rather than sensitivity based.
The XVA P&L Explain analytic uses the XVA Stress analytic internally with an auto-generated par stress test configuration
driven by the sensitivity configuration which defines the par rates to be shifted. Note that the resulting report shows only
a few stress scenario results, because the two markets were chosen such that most of the scenarios have zero par rate
differences to reduce computation time in this example case.

Run with <code>python run_xvaexplain.py</code>


## XVA Sensitivities

We run bump & reval sensitivites of XVA using the sensitivity configuration in sensitivity.xml
using a small portfolio (two Swaps), a subset of the stress test portfolio above.

Run with <code>python run_sensi.py</code>


## CVA Capital: SA-CVA and BA-CVA

SA-CVA is based on CVA Sensitivities, and the analytic in ORE can be seen as a post-processor for the XVA
sensitivity analytic's output. The sensitivity input can be computed on the fly or passed as a pre-computed file.

Run SA-CVA with: <code>python run_sacva.py</code>


The basic approach calculation, BA-CVA, is based on Exposure-At-Default values from SA-CCR, so it uses the SA-CCR analytic internally. We run BA-CVA here, but SA-CCR itself is discussed in the CreditRisk section.  

Run BA-CVA with: <code>python run_bacva.py</code>


