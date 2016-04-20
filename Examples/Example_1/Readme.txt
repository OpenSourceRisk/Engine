1) Portfolio

Vanilla Swap, EUR, 10k notional, 20Y maturity, receive
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

EPE and ENE, compared to European swaption prices 

5) Run Example

./run.sh
