# Collateralised Exposure Examples

The examples in this folder demonstrate exposure simulation and XVA
for a collateralised small portfolio consisting of three Swaps in different currencies.
We show how to run the simulation once, store the NPV cube, and then postprocess
to analyse several collateralisation cases (variation margin with thresholds and minimum
transfer amounts, impact of initial margin).

See the user guide for a discussion of each case and expected results.


## Biweekly Grid

Here we use a bi-weekly simulation grid, i.e. adjacent date grid points are separated by the
2W margin period of risk. For long-dated portfolios this leads to a large number of simulation
dates.

Run with <code>python run_biweekly.py</code>


## Close-out Grid

Alternatively we can use a setup with a main grid which might be fine at the short
end and more coarse at the longer end, possibly with fewer number grid points than above.
And then we use an auxiliary grid which is a copy of the main grid but offset by the margin
period of risk. The main grid is used to compute default values, the auxiliary grid is used
to compute close-out values.

Run with <code>python run_closeout.py</code>


## First Margin Period of Risk Adjustment

We run a simple XVA calculation for a Receiver Swap (in netting set CPTY_A) and a Payer Swap (in netting set CPTY_B)
with two initial VM balances each such that the netting set is fully collateralised resp. over/under-collateralised.

The new flag firstMporCollateralAdjustment in ore.xml's XVA section affects the first MPoR, 2 weeks in the example.
If set to true, the difference between initial collateral balance and mtm is carried over during the first MPoRr period,
if the difference increases our exposure.

Run with <code>python run_firstmpor.py</code>

