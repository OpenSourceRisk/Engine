1) Portfolio

   Vanilla Swap, EUR, 10k notional, 20Y maturity, rec. fixed 0.99851% (ATM), pay 6m Euribor
   Vanilla Swap, EUR, 10k notional, 10Y maturity, rec. fixed 0.99851% (ATM), pay 6m Euribor

   There are two cases: Normal and reversed normal (pay/rec switched). The analysis with flipViewXVA set to Y (run with normal) should be exactly the result of reversed normal.

2) Market

   Pseudo market data as of 2016-02-05

3) Pricing

   Dual curve, Eonia Discounting, Euribor Forwards 

4) Analytics

   EPE and ENE, compared to European payer and receiver swaption prices 

5) Run Example

   python run.py
