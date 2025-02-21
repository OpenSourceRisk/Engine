# Products

This directory contains 150+ sample trades in ORE XML across six asset classes, vanilla and complex, see subfolder <code>Example_Trades</code>.
The sample trades have also been concatenated into a single portfolio in <code>Input/portfolio.xml</code>.

The Input folder contains the necessary configuration and market data to support a valuation batch.

Run with: <code>python run.py</code>

In addition to the NPV and CASHFLOW analytic we have selected the PORTFOLIIO_DETAILS analytic here which reports portfolio composition in a series of extra files:
counterparties.csv, marketObjects.csv, netting_sets.csv, riskFactors.csv, swap_indices.csv, trade_types.csv, underlying_indices.csv.

See the **ORE Product Catalogue** in Docs/UserGuide/products.tex|pdf for payoff descriptions, input guide and pricing methods. 