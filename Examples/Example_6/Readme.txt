1) Three portfolios demonstrating cap, floor and collar setup
   
   portfolio_1:
   -----------
   1) 20Y EUR swap, notional = 1M, receive 3% semi-annual and pay EURIBOR 6M semi-annual. Netting set = CPTY_A.
   2) Long a 20Y EUR 1M collar, notional = 1M, strike on both cap and floor 2.5%. Netting set = CPTY_A.
   Same netting set => netted exposure of receive fixed leg at 50bps.

   portfolio_2:
   -----------
   1) Short 15Y USD cap on USD Libor 3M @ 3%. Netting set = CPTY_B.
   2) Long 15Y USD floor on USD Libor 3M @ 0.5%. Netting set = CPTY_B.
   2) Long 15Y USD collar on USD Libor 3M @ [0.5%, 3%]. Netting set = CPTY_A.
   Show exposure of cap, floor and collar separately.
   Netted exposure of cap & floor = exposure of collar.

   portfolio_3:
   -----------
   1) Long 15Y GBP, notional = 3M, cap on GBP Libor 6M @ 2.5%. Netting set = CPTY_B.
   2) Short 15Y GBP, notional = 3M linearly amortizing, cap on GBP Libor 6M @ 2.5%. Netting set = CPTY_B.
   EPE on 1) > ENE on 2)

   Two portfolios demonstrating capped floored ibor swaps setup

   portfolio_4:
   -----------
   1) 20Y vanilla swap, receive fixed, pay floating EUR-EURIBOR-6M. Netting set = CPTY_A.
   2) Long 20Y collar on EUR-EURIBOR-6M with cap of 3% and floor of 1%. Netting set = CPTY_A.
   3) 20Y swap, receive fixed, pay floating EUR-EURIBOR-6M capped at 3% and floored at 1%. Netting set = CPTY_B.
   NPV and netted exposures should be the same
   
   portfolio_5:
   -----------
   1) 15Y swap, receive fixed, pay floating GBP-LIBOR-6M. Netting set = CPTY_B.
   2) Long 15Y cap on GBP-LIBOR-6M. Netting set = CPTY_B.
   3) 15Y swap, receive fixed, pay floating GBP-LIBOR-6M capped. Netting set = CPTY_A.
   Swap, cap and capped swap each have amortising notionals, varying spreads and gearings
   NPV and netted exposures should be the same

2) Market
   
   Yield curves 20/05/2016 and normal cap volatility surfaces in EUR, GBP and USD
		
3) Pricing

   Multi-curve pricing (EUR collateralised) with normal (Bachelier) cap pricing

4) Analytics

   EPE and ENE compared for different trade combinations 

5) Run Example

   python run.py
