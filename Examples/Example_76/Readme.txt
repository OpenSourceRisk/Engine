Demonstrate the effect of Settings::instance().includeTodaysCashFlows()

1) Portfolios consisting of 

   Vanilla Swap in EUR
   EUR/USD Cross Currency Swaps, with and without reset
   European Swaption
   Cap and Floor
   FX Forward
   FX Call and Put Options
   Equity Forward
   European Equity Calls and Puts
   Forward Bond
   CDS

   (a) with maturity = as of date = 05/02/2016
   (b) with maturuty = as of date + 4 weeks = 04/03/2016
   
2) Market

   Pseudo market as of 05/02/2016
   
3) Pricing

   Multi curve pricing with separate discount and forward curves

4) Analytics

   Three ORE runs
   - Pricing for portfolio (a)
   - Classic exposure simulation for portfolio (b)
   - AMC exposure simulation for portfolio (b)

5) Run Example

   python run.py

