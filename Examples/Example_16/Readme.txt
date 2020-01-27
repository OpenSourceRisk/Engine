1) Portfolio

   "S&P" bought equity call option, denominated in USD
   "S&P" sold equity put option, denomindated in USD
   "S&P" short equity forward, denominated in USD
   "Lufthansa" bought equity call option, denominated in EUR
   "Lufthansa" bought equity put option, denominated in EUR
   "Lufthansa" long equity forward, denominated in EUR
   "S&P" equity payer swap, return type "price"
   "S&P" equity payer swap, return type "total" with dividend factor 0.8
   
2) Market

   Pseudo market data as of 2016-02-05, including 
   equity spot quotes, forward price quotes, dividend yields
   and Black-implied volatilities.

3) Pricing

   QuantLib's AnalyticEuropeanEngine (Black-Scholes) used for option pricing

4) Analytics

   EPE and ENE, at trade and portfolio level

5) Run Example

   python run.py

