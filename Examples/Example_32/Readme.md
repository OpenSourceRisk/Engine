# Example 32

## About
This example demonstrates exposure calculation with Variation Margin (VM) taken
into account as in Example 31, and additional posting of Initial Margin (IM).

IM is modelled dynamically by computing portfolio sensitivities and Delta (or
Delta-Gamma) VaRs along all Monte Carlo paths, broadly consistent with the
actual IM model - same holding period and confidence level. The covariance 
matrix of the VaR model is kept fixed and is calibrated such that it is
consistent with the simulation model's covariances. This gives rise to a
discrepancy between current IM and current DIM which can be ironed out by
a constant DIM scaling factor (per netting set).

We are using the case as in Example 10 here (perfect CSA with zero threshold
and minimum transfer amount), so that the remaining exposure is solely due
to the MPOR lag effect and dynamic IM.

