import sys
#add path to compiled version of ORE.py, _OREP.pyd
sys.path.append(r"..\..\..\..\build\ore\ORE-SWIG")
sys.path.append(r"..\..\..\..\build\ore\ORE-SWIG\RelWithDebInfo")
sys.path.append(r"..\..\input")

from ORE import *
ip = InputParameters()
ip.setCurrencyConfigFromFile(r"..\..\input\currencies.xml")
ip.setConventionsFromFile(r"..\..\input\conventions.xml")
#ip.setRefDataManagerFromFile(r"..\..\input\referencedata.xml");
ip.setCurveConfigsFromFile(r"..\..\input\curveconfig.xml");
#ip.setCalendarAdjustmentFromFile(r"..\..\input\calendar_adjustments.xml")
refData = ip.refDataManager()
curveConfigs = ip.curveConfigs().curveConfigurations()
curveConfig = curveConfigs[curveConfigs.keys()[0]]

tg = TradeGenerator(curveConfig, refData, "CP", "NS")
tg.buildSwap("GBP-SONIA", 1000, "1Y", 0.01, True, "", "Swappy-Swap")
tg.buildInflationSwap("UKRPI", 1000, "5Y", "GBP-SONIA", 200, 0.02, True, "inflation swap!")
tg.buildFxForward("USD", 1000, "EUR", 1100, "5Y", True)
tg.buildSwap("USD-FedFunds", 1000, "5Y", "USD-LIBOR-3M", 0.01, True, "1W", "1W Start Lag Swap")
tg.buildSwap("USD-LIBOR-3M", 1000, "5Y", 0.01, False)
tg.buildCapFloor("USD-LIBOR-3M", 0.01, 1000, "2Y", True, True)
tg.buildEquitySwap("SP5", "Total", 1000, "USD-FedFunds", 1000, "10Y", True)
tg.buildEquitySwap("SP5", "Total", 1000, 0.01, 1000, "6M", True)
tg.buildEquityForward("SP5", 1000, "7Y", 2020)
#tg.buildCommoditySwap("GOLD_USD", "5Y", 100, 50, "USD-FedFunds", "spot", True)
#tg.buildCommodityForward("GOLD_USD", 100, "2Y", 50, True)
#tg.buildCommodityOption("GOLD_USD", 100, "3Y", 50, "spot", True, True)

tg.toFile(r"input\portfolio.xml")

