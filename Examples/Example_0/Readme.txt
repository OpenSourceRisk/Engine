Compare ORE vs QRE exposure evolution:

Single swap in EUR, vanilla fixed 1% vs 6M Euribor, maturity 2034

Single curve (6M Euribor) pricing, calibration and simulation

Calibration to 1Y tail ATM Swaptions with fixed mean reversion speed at 0.01

Realistic pseudo market as of 2016-03-01

QRE version: roland's csa branch as of 19/07/2016

Call QRE from App directory with

     ./qre -envFile ../CS/FMS/Input/env_20160301.txt \
           -asof 20160301 -sgen 8 -grid 230,1M -samples 5000 -mode xva -discounted
     ./qreAnalytics -envFile ../CS/FMS/Input/env_20160301.txt \
           -asof 20160301 -dir ../CS/FMS/Output -profiles

     QRE result 20160301_qre_exposure_trade627291AI.csv copied to this directory

     QRE input qre_input.tar.gz copied to Input

Call ORE with

     ./run.sh

Manual comparison in ore_qre_exposure.xlsx

