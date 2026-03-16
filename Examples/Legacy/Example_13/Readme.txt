1) Portfolio

   A) atm vanilla swap in EUR, 10y maturity
   B) atm european swaption in EUR, 10y expiry, underlying 10y, physical delivery
   C) vanilla swap in USD, 10y maturity
   D) EUR-USD XCcy swap, float-float, 10y maturity

2) Market

   Pseudo market data as of 2016-02-05
   EUR 6M and 1D curve flat
   EUR Swp Vol 0.0050 flat

3) Pricing

   Libor Discounting

4) Analytics

   A), B), C), D)
   DIM evolution, Zero-, First- Second-Order Regression
   DIM regression, Zero-, First-, Second-Order Regression

   B) (precomputed) Direct estimation of T0 VaR as a reference value for Delta / Delta-Gamma VaR

5) Run Example

   python run_A.py
   python run_B.py
   python run_C.py
   python run_D.py

   or combine all cases with

   python run.py
