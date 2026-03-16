1) Portfolio

   Vanilla Swap, EUR, 10m notional, 20Y maturity, receive
   fixed 2.1% anually, pay 6m Euribor semi-annually

2) Market

   Minimal market data file, using single points where possible
   Flat Zero Coupon curve at 2.1%

   Note that today's fixings in the fixing file need to be commented
   out to ensure that today's fixings are implied.
   If today's fixings are provided, they are taken into
   account for the first floating coupon.
		
3) Pricing

   Single curve

4) Analytics

   EPE and ENE, compared to European swaption prices 

5) Run Example

   python run.py
