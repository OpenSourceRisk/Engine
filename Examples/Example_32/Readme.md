# Example 32 - Swaps with VM and IM

## About
This example demonstrates exposure calculation with Variation Margin (VM) taken
into account as in Example 31, and additional posting of Initial Margin (IM).

The simulation base currency is EUR. The portfolio contains either
- a single at-the-money Swap in EUR (portfolio_eur.xml), or
- a single currency swap in USD (portfolio_usd.xml), or
- a single cross currency swap EUR/USD, non-resetting (portfolio_eurusd.xml)

IM is modelled dynamically by
a) regression as in Example 13
b) computing portfolio sensitivities and Delta (or Delta-Gamma) VaRs along all
   Monte Carlo paths, broadly consistent with the actual IM model - same holding
   period and confidence level; the covariance matrix of the VaR model is kept
   fixed and is calibrated such that it is consistent with the simulation model's
   covariances.
These approaches give rise to a discrepancy between current IM and current DIM
which can be ironed out by a constant DIM scaling factor (per netting set).

We are using the case as in Example 10 here (perfect CSA with zero threshold
and minimum transfer amount), so that the remaining exposure is solely due
to the MPOR lag effect and dynamic IM.

