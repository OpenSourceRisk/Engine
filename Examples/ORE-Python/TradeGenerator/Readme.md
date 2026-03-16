# ORE Python Trade Generator

The trade generator allows the user to create a portfolio of simple trades - swaps, capfloors, FX options, equity derivatives and commodity derivatives. 
To achieve this, we must pass into the constructor a curve config object and optionally a reference data object. In addition, it is assumed that an instance of Conventions exists.
As a result, the example does the following things:
 - Create an instance of InputParameters
 - Set conventions, curve configs and various other configurations using those in the examples 'Input' folder
 - Extract the curve config and reference data manager objects from the InputParameters, and create a TradeGenerator object with these two variables, plus the optional counterparty and netting set parameters ('CP' and 'NS' respectively)

When the TradeGenerator object is created, the Swap, OIS, Commodity Forward and Inflation Swap conventions read in to a map, with their index attribute as the key. Subsequently, we can create a series of trades, where the identifier used is taken from the Index attribute of the convention. For example, the first swap created uses the index 'GBP-SONIA' - this is used to look up the relevant convention - an OIS convention in this case - and to look up the index from the curve config. Using the information in these two places we have enough to create a fixed-float swap, including:
 - fixed, float payment frequency
 - payment calendars
 - payment/business day conventions
 - spot lags

In addition, we specify as parameters the notional, maturity (either as an ISO formatted date string, or a QuantLib style Period string), fixed rate, payer/receiver, and optionally the trade ID. If not trade ID is specified it defaults to a combination of trade type, currency, and an integer, for example 1_GBP_OIS_SWAP.

The TradeGenerator object extends the Portfolio object, and so when we build a trade it is automatically added to the portfolio. Once all trades have been created, we can save the portfolio to XML for any desired modifications, or we can add this portfolio to the InputParameters object and, along with some market data and fixings, run calculations. In this example, we use the market data and fixings from the same input directory, run an NPV calculation, and output the results to the Output folder.