
1) Portfolio

   Small portfolio consisting of three vanilla swaps in EUR, USD and GBP,
   with final maturity 2027

2) Market

   Pseudo market data as of 2016-02-05

3) Pricing

   Dual curve, Eonia Discounting

4) Analytics

   a) no collateral:
      EPE and ENE for individual trades and netting set, 
      allocated trade EPEs and ENEs (marginal)
      
   b) collateral with THR > 0, MTA > 0, MPOR = 2W:
      EPE and ENE for the netting set
      allocated trade EPEs and ENEs (marginal)
      
   c) collateral with THR = 0, MTA > 0, MPOR = 2W:
      EPE and ENE for the netting set
      allocated trade EPEs and ENEs (relative CVA)

   d) collateral with THR = 0, MTA = 0, MPOR = 2W:
      EPE and ENE for the netting set
      allocated trade EPEs and ENEs (relative CVA)

5) Run Example

   python run.py
   
