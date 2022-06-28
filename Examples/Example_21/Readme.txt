1) Portfolio

   Constant Maturity Swap, EUR, 10m notional, 20Y maturity, receive
   fixed 2.1% anually, pay EUR-CMS-30Y semi-annually

2) Market

   Minimal market data file, using single points where possible
   Flat Zero Coupon curve at 2.1%

   Note that today's fixings in the fixing file need to be commented
   out to ensure that today's fixings are implied.
   If today's fixings are provided, they are taken into
   account for the first floating coupon.
		
3) Pricing

   Single curve

4) Run Example

   python run.py
