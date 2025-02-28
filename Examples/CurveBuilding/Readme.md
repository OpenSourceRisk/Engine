# Curve Building Examples

This section bundles several examples around curve building. 

The cases can be run individually (see below) or all together with: <code>python run.py</code>

## 1. Bootstrap Consistency

This example confirms that bootstrapped curves correctly reprice the
bootstrap instruments (FRAs, Interest Rate Swaps, FX Forwards, Cross
Currency Basis Swaps) using three pricing setups with
- EUR collateral discounting (XOIS EUR)
- USD collateral discounting (XOIS USD)
- in-currency OIS discounting 

Run with <code>python run_consistency.py</code>

Check that NPV = 0 in
- Output/consistency/ois/npv.txt
- Output/consistency/xoiseur/npv.txt
- Output/consistency/xoisusd/npv.txt

Note that the three portfolios can be auto-generated with: <code>python TradeGenerator.py</code>

## 2. Discount Ratio Curves

This example shows how to use a yield curve built from a DiscountRatio segment. In particular, it builds a GBP collateralised in EUR discount curve by referencing three other discount curves:

1. a GBP collateralised in USD curve
2. a EUR collateralised in USD curve
3. a EUR OIS curve i.e. a EUR collateralised in EUR curve

The implicit assumption in building the curve this way is that EUR/GBP FX forwards collateralised in EUR have the same fair market rate as EUR/GBP FX forwards collateralised in USD. This assumption is illustrated in the example by the npv of the two forward instruments in the portfolio returning exactly 0 under both discounting regimes i.e. under USD collateralisation with *direct* curve building **and** under EUR collateralisation with the discount ratio modified `GBP-IN-EUR` curve.

Also, in this example, an assumption is made that there are no direct GBP/EUR FX forward or cross currency quotes available which in general is false. The example is merely for illustration.

Run with <code>python run_discountratio.py</code>

## 3. Fixed vs Float Cross Currency Helpers

This example demonstrates using fixed vs. float cross currency swap helpers.
In particular, it builds a TRY collateralised in USD discount curve using TRY annual
fixed vs USD 3M Libor swap quotes.

The portfolio contains an at-market fixed vs. float cross currency swap that is included
in the curve building. The NPV of this swap should be zero when the example is run.

Run with <code>python run_fixedfloatccs.py</code>


## USD-Prime Curve Building via Prime-LIBOR Basis Swaps

A USD-Prime Curve is built and a Prime-LIBOR basis swap is processed.

The Example confirms that the bootstrapped curves USD-FedFunds and USD-Prime follow the
3% rule observed on the market:
U.S. Prime Rate = (The Fed Funds Target Rate + 3)
http://www.fedprimerate.com/

The portfolio consists of two Basis Swaps
- a US Dollar Prime Rate vs 3 Month LIBOR Basis Swap
- a US Dollar 3 Month LIBOR vs Fed Funds Basis Swap

Run with <code>python run_prime.py</code>

Check that NPVs = 0.


## Bond Yield Shifted Curves

Run with <code>python run_bondyieldshifted.py</code>


## Central Bank Meeting Dates

This example demonstrates how to use OIS instruments at the short end of the yield curve
which have fixed start and end dates (MPC Swaps) that cover central meeting dates.

Run with <code>python run_centralbank.py</code>


## SABR Swaption and Cap/Floor Volatilities

Run with <code>python run_sabr.py</code>

