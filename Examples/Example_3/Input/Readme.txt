
Portfolio: Vanilla Swap, EUR, 10k notional, 20Y maturity, rec. fixed 0.99851% (ATM), pay 6m Euribor

Market: 2016-02-26

Pricing: Single curve

Analytics: EPE and ENE, compared to European payer and receiver swaption prices 

Run:

	cd App
	mkdir ../Output/Example_3
	./ore ../Input/Example_3/ore_payer_swaption.xml
	./ore ../Input/Example_3/ore_receiver_swaption.xml
	./ore ../Input/Example_3/ore.xml
	cp ../Input/Example_3/plot.gp ../Output/Example_3
	cd ../Output/Example_3
	gnuplot plot.gp
