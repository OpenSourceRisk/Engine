
Portfolio: Vanilla Swap, EUR, 10k notional, 20Y maturity, rec. fixed 0.99851% (ATM), pay 6m Euribor

Market: 2016-02-26

Pricing: Dual curve, Eonia Discounting

Analytics: EPE and ENE, compared to European payer and receiver swaption prices 

Run:

	cd App
	mkdir ../Output/Example_4
	./ore ../Input/Example_4/ore_payer_swaption.xml
	./ore ../Input/Example_4/ore_receiver_swaption.xml
	./ore ../Input/Example_4/ore.xml
	cp ../Input/Example_4/plot.gp ../Output/Example_4
	cd ../Output/Example_4
	gnuplot plot.gp
