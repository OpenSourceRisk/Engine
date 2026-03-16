# Example 31

## About
This example shows how to use a yield curve built from a DiscountRatio segment. In particular, it builds a GBP collateralised in EUR discount curve by referencing three other discount curves:

1. a GBP collateralised in USD curve
2. a EUR collateralised in USD curve
3. a EUR OIS curve i.e. a EUR collateralised in EUR curve

The implicit assumption in building the curve this way is that EUR/GBP FX forwards collateralised in EUR have the same fair market rate as EUR/GBP FX forwards collateralised in USD. This assumption is illustrated in the example by the npv of the two forward instruments in the portfolio returning exactly 0 under both discounting regimes i.e. under USD collateralisation with *direct* curve building **and** under EUR collateralisation with the discount ratio modified `GBP-IN-EUR` curve.

Also, in this example, an assumption is made that there are no direct GBP/EUR FX forward or cross currency quotes available which in general is false. The example is merely for illustration.