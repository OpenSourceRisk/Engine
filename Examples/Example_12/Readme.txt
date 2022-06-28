1) Portfolio

   Vanilla Swap, EUR, 10m notional, 50Y maturity, receive
   fixed 2% anually, pay 6m Euribor semi-annually

2) Market

   Flat 6m Euribor Swap curve at 2%

   Note that today's fixings in the fixing file need to be commented
   out to ensure that today's fixings are implied.
   If today's fixings are provided, they are taken into
   account for the first floating coupon.
		
3) Pricing

   Single curve

4) Analytics

   EPE and ENE, compared to European swaption prices, one simulation is
   run in the original LGM measure, the other in the T-forward measure
   (T=50), i.e. a horizon shift of 50 is set in the simulation
   configuration.

5) Run Example

   python run.py

         
