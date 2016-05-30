
Portfolio: Mini portfolio consisting of three vanilla swaps in EUR, USD and GBP,
	   final maturity 2027

Market: 2016-02-26

Pricing: Dual curve, Eonia Discounting

Analytics: EPE and ENE with and without collateral

Run:

	cd App
	mkdir ../Output/Example_5
	cp ../Input/Example_5/plot.gp ../Output/Example_5

	# without collateral	
	./ore ../Input/Example_5/ore.xml

	# with collateral
	./ore ../Input/Example_5/ore_active.xml

	cd ../Output/Example_5
	# edit plot.gp
	# ...
	# then
	gnuplot plot.gp
