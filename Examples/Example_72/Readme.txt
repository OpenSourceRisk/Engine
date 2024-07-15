XVA Calculation with firstMporCollateralAdjustment

4 Netting Sets:

all nettingsets contains of one 20Y IR Swap (pay or receive). 

two of the netting sets are fully collateralized, the other two have a gap between t0 mtm and initial vm collateral balance.

Is the flag set to true, the difference between initial collateral balance and mtm is carried over during the first mpor period, if
the difference increase our exposure.

Run:

ore ./Input/ore.xml for AMC 