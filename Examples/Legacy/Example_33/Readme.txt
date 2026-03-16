1) Portfolio

   Credit Default Swap, EUR, 10m notional, 5Y maturity, pay
   fixed 0.59% quaterly, receive protection on CPTY A.
   Counterparty of the trade is CPTY B

2) Market

   Flat CPTY A default curve at 1% hazard rate and 40% recovery rate
   Flat CPTY B default curve at 1% hazard rate and 40% recovery rate

3) Pricing

   Single curve

4) Analytics

   EPE and ENE with dynamic credit, compared to CDS option prices
   CVA, DVA, FCA, FBA

5) Simulation

   Cross Asset Model with CR simulation
   Instantaneous Correlations between CPTY A and CPTY B is at 80%

6) Run Example

   python run.py