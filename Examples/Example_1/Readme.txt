1) Portfolio

Vanilla Swap, EUR, 10m notional, 20Y maturity, receive
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

6) Run Example via LibreOffice Calc

   a) Install LibreOffice 5.0.6.3 (latest stable)

   b) Make sure the soffice executable is in your search path
      (on Mac it is in /Applications/LibreOffice.app/Contents/MacOS).
      
   c) In this Example_1 directory launch:
      soffice \
      --calc \
      --accept="socket,host=localhost,port=2002;urp;StarOffice.ServiceManager" \
      run.ods
         
