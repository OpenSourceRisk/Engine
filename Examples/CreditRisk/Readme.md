# Credit Risk

The two applications described below can be run in one shot with: <code>python run.py</code>


## Standard Approach CCR Capital: SA-CCR

Demonstration of the SA-CCR implementation in ORE.

Run with: <code>python run_saccr.py</code>


## Credit Portfolio Model

This directory demonstrates the implementation of a Credit Portfolio Model, inspired by Credit Metrics, in ORE.
Key is the contruction of a portfolio loss distribution due to credit migrations and defaults
driven by systemic/idiosyncratic factors.
We also show how to combine this credit loss distribution with the P&L due to the evolution of the usual market factors
within ORE's simulation framework for an integrated Credit/Market Risk view.

We use test portfolios of varying size and compostion
- a single bond
- bond and swap
- 3 bonds
- 10 bonds
- single CDS
- bond and CDS
- 100 bonds

Separate documentation of the model can be found in Docs/CreditModel.

Run all cases with: <code> python run_cpm.py </code>

