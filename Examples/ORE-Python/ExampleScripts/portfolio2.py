
# The scope of file...
#   ore/ORE-SWIG/OREData-SWIG/SWIG/ored_portfolio2.i
# ...is: "Export to SWIG all of the constructors from
#   ore/OREData/ored/portfolio/*.hpp
# that have not already been exported in other SWIG interface files".
# This script invokes all of those constructors.
# There are 300+ classes, the number of supported constructors is higher
# because of overloads.
# This script constructs trades but does not price them and there is no output.


import ORE as ql


def main():
    # some values

    qlEvalDate = ql.Date(6, ql.November, 2001)
    ql.Settings.instance().evaluationDate = qlEvalDate
    qlEvalDateStr = ql.to_string(qlEvalDate)
    calendar = ql.TARGET()
    period_1Y = ql.Period(1, ql.Years)
    qlEndDate = calendar.adjust(qlEvalDate + period_1Y)
    qlEndDateStr = ql.to_string(qlEndDate)

    env0 = ql.Envelope("CP")
    bond0 = ql.Bond(0, calendar)
    trade_strike0 = ql.TradeStrike(0., "EUR")
    equity_underlying0 = ql.EquityUnderlying("", "", "EUR", "", 1.)
    barrier_data0 = ql.BarrierData("KnockIn", [0.], 0., [])
    bond_data0 = ql.BondData("BondIssuer", "BondIssuer", "Bond", "BondCurve", "2",
        "TARGET", 1., qlEvalDateStr, "EUR", qlEndDateStr)
    option_data0 = ql.OptionData("Long", "", "European", False, [qlEvalDateStr])
    leg_data0 = ql.LegData(ql.FixedLegData([0.]), True, "EUR")
    basket_data0 = ql.BasketData([ql.BasketConstituent("dc2", "dc2", 0., "EUR", "Tranch1")])
    swap_data0 = ql.IndexCreditDefaultSwapData("dc2", basket_data0, leg_data0)
    schedule_data0 = ql.ScheduleData(ql.ScheduleRules(qlEvalDateStr, qlEvalDateStr,
        "1Y", "TARGET", "MF", "MF", "Forward"))
    schedule0 = ql.Schedule(qlEvalDate, qlEndDate, period_1Y, calendar,
        ql.Unadjusted, ql.Unadjusted, ql.DateGeneration.Forward, False)

    # ore/OREData/ored/portfolio/scriptedtrade.hpp

    ql.ScriptedTradeEventData()
    ql.ScriptedTradeEventData("", qlEvalDateStr)
    ql.ScriptedTradeEventData("", schedule_data0)
    ql.ScriptedTradeEventData("", "", "", "TARGET", "MF")
    ql.ScriptedTradeValueTypeData("")
    ql.ScriptedTradeValueTypeData("", "", "")
    ql.ScriptedTradeValueTypeData("", "", [""])
    if hasattr(ql, "NewScheduleData"):
        ql.NewScheduleData()
        ql.NewScheduleData("", "", [""])
    if hasattr(ql, "CalibrationData"):
        ql.CalibrationData()
        ql.CalibrationData("", [""])
    ql.ScriptedTradeScriptData()
    ql.ScriptedTradeScriptData("", "", ql.VectorPairString([tuple(["", ""])]), [""])
    ql.ScriptLibraryData()
    ql.ScriptLibraryData(ql.ScriptLibraryData())
    ql.ScriptedTrade()
    x0 = [ql.ScriptedTradeEventData()]
    x1 = [ql.ScriptedTradeValueTypeData("")]
    # FIXME If this overload is required then we must implement support for map<string, ScriptedTradeScriptData>
    #ql.ScriptedTrade(env0, x0, x1, x1, x1, x1, script, "")
    ql.ScriptedTrade(env0, x0, x1, x1, x1, x1, "")

    # ore/OREData/ored/portfolio/underlying.hpp

    ql.BasicUnderlying()
    ql.BasicUnderlying("")
    #ql.EquityUnderlying()
    ql.EquityUnderlying("")
    ql.EquityUnderlying("", "", "EUR", "", 0.)
    ql.CommodityUnderlying()
    ql.CommodityUnderlying("", 0., "", 0, 0, "TARGET")
    ql.FXUnderlying()
    ql.FXUnderlying("", "", 0.)
    ql.InterestRateUnderlying()
    ql.InterestRateUnderlying("", "", 0.)
    ql.InflationUnderlying()
    ql.InflationUnderlying("", "", 0.)
    ql.CreditUnderlying()
    ql.CreditUnderlying("", "", 0.)
    ql.BondUnderlying()
    ql.BondUnderlying("")
    ql.BondUnderlying("", "", 0.)
    ql.UnderlyingBuilder()

    # ore/OREData/ored/portfolio/accumulator.hpp

    ql.EquityAccumulator()
    ql.FxAccumulator()
    ql.CommodityAccumulator()

    # ore/OREData/ored/portfolio/additionalfieldgetter.hpp

    ql.PortfolioFieldGetter(ql.Portfolio())

    # ore/OREData/ored/portfolio/convertiblebond.hpp

    ql.ConvertibleBond()

    # ore/OREData/ored/portfolio/ascot.hpp

    ql.Ascot()
    ql.Ascot(env0, ql.ConvertibleBond(), option_data0, leg_data0)

    # ore/OREData/ored/portfolio/asianoption.hpp

    ql.AsianOption("")
    ql.AsianOption(env0, "", 0., trade_strike0, option_data0,
        schedule_data0, equity_underlying0, qlEvalDate, "EUR")
    ql.EquityAsianOption()
    ql.FxAsianOption()
    ql.CommodityAsianOption()

    # ore/OREData/ored/portfolio/autocallable_01.hpp

    ql.Autocallable_01()
    ql.Autocallable_01(env0, "", "", "", equity_underlying0,
        "", "", schedule_data0, schedule_data0, [""], "")

    # ore/OREData/ored/portfolio/balanceguaranteedswap.hpp

    ql.BGSTrancheData()
    ql.BGSTrancheData("", "", 0, [0.], [""])
    ql.BalanceGuaranteedSwap()
    ql.BalanceGuaranteedSwap(env0, "", [ql.BGSTrancheData()], schedule0, [leg_data0])

    # ore/OREData/ored/portfolio/optionwrapper.hpp

    ql.EuropeanOptionWrapper(bond0, False, qlEvalDate, qlEndDate, False, bond0)
    ql.AmericanOptionWrapper(bond0, False, qlEvalDate, qlEndDate, False, bond0)
    ql.BermudanOptionWrapper(bond0, False, [qlEvalDate], [qlEndDate], False, [bond0])

    # ore/OREData/ored/portfolio/basketoption.hpp

    ql.OREBasketOption()
    ql.OREBasketOption("EUR", "", trade_strike0,
        [equity_underlying0], option_data0, "", schedule_data0)
    ql.EquityBasketOption()
    ql.FxBasketOption()
    ql.CommodityBasketOption()

    # ore/OREData/ored/portfolio/basketvarianceswap.hpp

    ql.BasketVarianceSwap()
    ql.BasketVarianceSwap(
        env0, "Long", "", "", "EUR", "", "", "",
        schedule_data0, False,
        [equity_underlying0])
    ql.EquityBasketVarianceSwap()
    ql.FxBasketVarianceSwap()
    ql.CommodityBasketVarianceSwap()

    # ore/OREData/ored/portfolio/bestentryoption.hpp

    ql.BestEntryOption()
    ql.BestEntryOption(env0, "Long", "", "", "", "", "", "",
        equity_underlying0, "EUR", schedule_data0, "")
    ql.EquityBestEntryOption()
    ql.FxBestEntryOption()
    ql.CommodityBestEntryOption()

    # ore/OREData/ored/portfolio/bondbasket.hpp

    ql.BondBasket()

    # ore/OREData/ored/portfolio/bondfuture.hpp

    ql.BondFuture()
    ql.BondFuture("", 0.)

    # ore/OREData/ored/portfolio/bondoption.hpp

    ql.BondOption()
    ql.BondOption(env0, bond_data0, option_data0, trade_strike0, False)

    # ore/OREData/ored/portfolio/bondposition.hpp

    ql.BondPositionData()
    ql.BondPositionData(0., [ql.BondUnderlying()])
    ql.BondPosition()
    ql.BondPosition(env0, ql.BondPositionData())
    bond_pos_inst_wrap0 = ql.BondPositionInstrumentWrapper(0., [bond0], [0.], [0.])

    # ore/OREData/ored/portfolio/bondrepo.hpp

    ql.BondRepo()

    # ore/OREData/ored/portfolio/bondtotalreturnswap.hpp

    ql.BondTRS()
    ql.BondTRS(env0, bond_data0)

    # ore/OREData/ored/portfolio/callablebond.hpp

    ql.CallableBondData()
    ql.CallableBondData(bond_data0)
    ql.ORECallableBond()
    ql.ORECallableBond(env0, ql.CallableBondData())

    # ore/OREData/ored/portfolio/callableswap.hpp

    ql.CallableSwap()
    # The constructor below is invoked in script portfolio.py:
    #ql.CallableSwap(Envelope, ORESwap, ORESwaption)

    # ore/OREData/ored/portfolio/capfloor.hpp

    ql.ORECapFloor()
    ql.ORECapFloor(env0, "Long", leg_data0, [0.], [0.])

    # ore/OREData/ored/portfolio/cashposition.hpp

    ql.CashPosition()
    ql.CashPosition(env0, "EUR", 0.)

    # ore/OREData/ored/portfolio/cbo.hpp

    ql.CBO()

    # ore/OREData/ored/portfolio/cliquetoption.hpp

    ql.ORECliquetOption("")
    ql.ORECliquetOption("", env0, equity_underlying0, "EUR", 0., "Long", "Call", schedule_data0)
    ql.EquityCliquetOption()
    ql.EquityCliquetOption("", env0, equity_underlying0, "EUR", 0., "Long", "Call", schedule_data0)

    # ore/OREData/ored/portfolio/collateralbalance.hpp

    ql.CollateralBalance()
    # FIXME need to export XMLNode
    #ql.CollateralBalance(XMLNode)
    # FIXME need to export NettingSetDetails from ore/OREData/ored/portfolio/nettingsetdetails.hpp
    #ql.CollateralBalance(NettingSetDetails, ...)
    ql.CollateralBalance("", "EUR", 0.)
    ql.CollateralBalances()

    # ore/OREData/ored/portfolio/commodityapo.hpp

    ql.CommodityAveragePriceOption()
    ql.CommodityAveragePriceOption(env0, option_data0, 0., 0., "EUR", "", ql.CommodityPriceType_Spot,
        qlEvalDateStr, qlEndDateStr, "TARGET", "0", "MF", "TARGET")

    # ore/OREData/ored/portfolio/commoditydigitalapo.hpp

    ql.CommodityDigitalAveragePriceOption()
    ql.CommodityDigitalAveragePriceOption(env0, option_data0, 0., 0., "EUR", "", ql.CommodityPriceType_Spot,
        qlEvalDateStr, qlEndDateStr, "TARGET", "0", "MF", "TARGET")

    # ore/OREData/ored/portfolio/commoditydigitaloption.hpp

    ql.CommodityDigitalOption()
    ql.CommodityDigitalOption(env0, option_data0, "", "EUR", 0., 0.)

    # ore/OREData/ored/portfolio/commodityforward.hpp

    ql.ORECommodityForward()
    ql.ORECommodityForward(env0, "", "", "EUR", 0., qlEndDateStr, 0.)
    ql.ORECommodityForward(env0, "", "", "EUR", 0., qlEndDateStr, 0., qlEndDate)
    ql.ORECommodityForward(env0, "", "", "EUR", 0., qlEndDateStr, 0., period_1Y, calendar)

    # ore/OREData/ored/portfolio/commoditylegbuilder.hpp

    ql.CommodityFixedLegBuilder()
    ql.CommodityFloatingLegBuilder()

    # ore/OREData/ored/portfolio/commodityoption.hpp

    ql.CommodityOption()
    ql.CommodityOption(env0, option_data0, "", "EUR", 0., trade_strike0)

    # ore/OREData/ored/portfolio/commodityoptionstrip.hpp

    ql.CommodityOptionStrip()
    v0 = ql.PositionTypeVector()
    v0.push_back(ql.Position.Long)
    ql.CommodityOptionStrip(env0, leg_data0, v0, [0.], v0, [0.])

    # ore/OREData/ored/portfolio/commodityposition.hpp

    ql.CommodityPositionData()
    ql.CommodityPositionData(0., [ql.CommodityUnderlying()])
    ql.CommodityPosition()
    ql.CommodityPosition(env0, ql.CommodityPositionData())
    ql.CommodityPositionInstrumentWrapper(0., [ql.CommoditySpotIndex("", calendar)], [0.])

    # ore/OREData/ored/portfolio/commodityspreadoption.hpp

    commodity_leg_data = ql.LegDataVector()
    commodity_leg_data.append(leg_data0)

    ql.CommoditySpreadOptionData()
    ql.CommoditySpreadOptionData(commodity_leg_data, option_data0, 0.)

    # ore/OREData/ored/portfolio/commodityswap.hpp

    ql.ORECommoditySwap(env0, commodity_leg_data)

    # ore/OREData/ored/portfolio/commodityswaption.hpp

    ql.CommoditySwaption()
    ql.CommoditySwaption(env0, option_data0, commodity_leg_data)

    # ore/OREData/ored/portfolio/compositeinstrumentwrapper.hpp

    ql.CompositeInstrumentWrapper([bond_pos_inst_wrap0])

    # ore/OREData/ored/portfolio/compositetrade.hpp

    trade_vector0 = ql.TradeVector()
    trade_vector0.append(ql.OREBond())

    ql.CompositeTrade(env0)
    ql.CompositeTrade("EUR", trade_vector0)

    # ore/OREData/ored/portfolio/convertiblebond.hpp

    ql.ConvertibleBond()
    ql.ConvertibleBond(env0, ql.ConvertibleBondData())

    # ore/OREData/ored/portfolio/convertiblebonddata.hpp

    ql.ConvertibleBondData()
    ql.ConvertibleBondData(bond_data0)

    # ore/OREData/ored/portfolio/convertiblebondreferencedata.hpp

    ql.ConvertibleBondReferenceDatum()
    ql.ConvertibleBondReferenceDatum("")

    # ore/OREData/ored/portfolio/counterpartycorrelationmatrix.hpp

    # FIXME need to export XMLNode
    #ql.CounterpartyCorrelationMatrix(XMLNode)
    ql.CounterpartyCorrelationMatrix()

    # ore/OREData/ored/portfolio/counterpartyinformation.hpp

    # FIXME need to export XMLNode
    #ql.CounterpartyInformation(XMLNode)
    ql.CounterpartyInformation("")

    # ore/OREData/ored/portfolio/counterpartymanager.hpp

    ql.CounterpartyManager()

    # ore/OREData/ored/portfolio/creditdefaultswapdata.hpp

    ql.CdsReferenceInformation()
    ql.CdsReferenceInformation("", ql.CdsTier_SNRFOR, ql.EURCurrency())
    ql.CreditDefaultSwapData("dc", "dc", leg_data0)
    ql.CreditDefaultSwapData("dc", ql.CdsReferenceInformation(), leg_data0)

    # ore/OREData/ored/portfolio/creditdefaultswapoption.hpp

    if hasattr(ql, "AuctionSettlementInformation"):
        ql.AuctionSettlementInformation()
        ql.AuctionSettlementInformation(qlEvalDate, 0.)
    ql.CreditDefaultSwapOption()
    ql.CreditDefaultSwapOption(env0, option_data0, ql.CreditDefaultSwapData("dc", "dc", leg_data0))

    # ore/OREData/ored/portfolio/creditlinkedswap.hpp

    ql.CreditLinkedSwap()
    ql.CreditLinkedSwap("", False, 0., "atDefault",
        [leg_data0], [leg_data0], [leg_data0], [leg_data0])

    # ore/OREData/ored/portfolio/crosscurrencyswap.hpp

    ql.CrossCurrencySwap()
    ql.CrossCurrencySwap(env0, [leg_data0])
    ql.CrossCurrencySwap(env0, leg_data0, leg_data0)

    # ore/OREData/ored/portfolio/doubledigitaloption.hpp

    ql.DoubleDigitalOption()
    ql.DoubleDigitalOption(env0, "", "", "", "", "", "", "", "",
        equity_underlying0, equity_underlying0, equity_underlying0, equity_underlying0, "EUR")

    # ore/OREData/ored/portfolio/durationadjustedcmslegbuilder.hpp

    ql.DurationAdjustedCmsLegBuilder()

    # ore/OREData/ored/portfolio/durationadjustedcmslegdata.hpp

    ql.DurationAdjustedCmsLegData()
    ql.DurationAdjustedCmsLegData("", 0, 0, False, [0.])

    # ore/OREData/ored/portfolio/equitybarrieroption.hpp

    ql.EquityBarrierOption()
    ql.EquityBarrierOption(env0, option_data0, barrier_data0, qlEvalDate, "TARGET",
        equity_underlying0, ql.EURCurrency(), 0., trade_strike0)

    # ore/OREData/ored/portfolio/equitydigitaloption.hpp

    ql.EquityDigitalOption()
    ql.EquityDigitalOption(env0, option_data0, 0., "EUR", 0., equity_underlying0, 0.)

    # ore/OREData/ored/portfolio/equitydoublebarrieroption.hpp

    ql.EquityDoubleBarrierOption()
    ql.EquityDoubleBarrierOption(env0, option_data0, barrier_data0, qlEvalDate, "TARGET",
        equity_underlying0, ql.EURCurrency(), 0., trade_strike0)

    # ore/OREData/ored/portfolio/equitydoubletouchoption.hpp

    ql.EquityDoubleTouchOption()
    ql.EquityDoubleTouchOption(env0, option_data0, barrier_data0, equity_underlying0, "EUR", 0.)

    # ore/OREData/ored/portfolio/equityeuropeanbarrieroption.hpp

    ql.EquityEuropeanBarrierOption()
    ql.EquityEuropeanBarrierOption(env0, option_data0, barrier_data0, equity_underlying0, "EUR", trade_strike0, 0.)

    # ore/OREData/ored/portfolio/equityforward.hpp

    ql.OREEquityForward()
    ql.OREEquityForward(env0, "Long", equity_underlying0, "EUR", 0., qlEndDateStr, 0.)

    # ore/OREData/ored/portfolio/equityfuturesoption.hpp

    ql.EquityFutureOption()
    ql.EquityFutureOption(env0, option_data0, "EUR", 0., equity_underlying0, trade_strike0, qlEndDate)

    # ore/OREData/ored/portfolio/equityfxlegbuilder.hpp

    ql.EquityMarginLegBuilder()

    # ore/OREData/ored/portfolio/equityfxlegdata.hpp

    ql.EquityMarginLegData()
    # FIXME need to export class EquityLegData in file ore/OREData/ored/legdata.hpp
    #ql.EquityMarginLegData(EquityLegData, [0.])

    # ore/OREData/ored/portfolio/equityoptionposition.hpp

    ql.EquityOptionUnderlyingData()
    x0 = ql.EquityOptionUnderlyingData(equity_underlying0, option_data0, 0.)
    v0 = ql.EquityOptionUnderlyingDataVector()
    v0.push_back(x0)
    ql.EquityOptionPositionData()
    x1 = ql.EquityOptionPositionData(0., v0)
    ql.EquityOptionPosition()
    ql.EquityOptionPosition(env0, x1)
    x2 = ql.PlainVanillaPayoff(ql.Option.Put, 0.)
    x3 = ql.AmericanExercise(qlEvalDate, qlEndDate)
    vanilla_option0 = ql.VanillaOption(x2, x3)
    ql.EquityOptionPositionInstrumentWrapper(0., [vanilla_option0], [0.], [0.])

    # ore/OREData/ored/portfolio/equityoutperformanceoption.hpp

    ql.EquityOutperformanceOption()
    ql.EquityOutperformanceOption(env0, option_data0, "EUR", 0.,
        equity_underlying0, equity_underlying0, 0., 0., 0.)

    # ore/OREData/ored/portfolio/equityposition.hpp

    ql.EquityPositionData()
    ql.EquityPositionData(0., [equity_underlying0])
    ql.EquityPosition()
    ql.EquityPosition(env0, ql.EquityPositionData())
    equity_index_vector0 = ql.EquityIndex2Vector()
    equity_index_vector0.append(ql.EquityIndex2("", calendar, ql.EURCurrency()))
    ql.EquityPositionInstrumentWrapper(0., equity_index_vector0, [0.])
    ql.EquityOptionPositionInstrumentWrapper(0., [vanilla_option0], [0.], [0.])

    # ore/OREData/ored/portfolio/equityswap.hpp

    ql.FlexiSwap()
    ql.FlexiSwap(env0, [leg_data0], ql.DoubleVector([0]), ql.StrVector([""]), "Long")

    ql.EquitySwap()
    ql.EquitySwap(env0, [leg_data0])
    ql.EquitySwap(env0, leg_data0, leg_data0)
    ql.EquityTouchOption()
    x0 = ql.BarrierData("DownAndIn", [0.], 0., [])
    ql.EquityTouchOption(env0, option_data0, x0, equity_underlying0, "EUR", 0.)
    ql.EuropeanOptionBarrier()
    ql.EuropeanOptionBarrier(env0, "", "Put", "Long", "", "", "EUR", qlEvalDateStr, qlEndDateStr,
        equity_underlying0, equity_underlying0, "", "", "", qlEndDateStr, "EUR", schedule_data0)
    ql.FailedTrade()
    ql.FailedTrade(env0)

    # ore/OREData/ored/portfolio/flexiswap.hpp

    #ql.FlexiSwap()
    #ql.FlexiSwap(env0, [leg_data0], [""], "Long")

    # ore/OREData/ored/portfolio/formulabasedlegbuilder.hpp

    ql.FormulaBasedLegBuilder()

    # ore/OREData/ored/portfolio/formulabasedlegdata.hpp

    ql.FormulaBasedLegData()
    ql.FormulaBasedLegData("+1", 0, False)

    # ore/OREData/ored/portfolio/forwardbond.hpp

    ql.ForwardBond()
    ql.ForwardBond(env0, bond_data0, qlEvalDateStr, qlEndDateStr, "", "", "", "", "", "", "", "")

    # ore/OREData/ored/portfolio/forwardrateagreement.hpp

    ql.OREForwardRateAgreement()
    ql.OREForwardRateAgreement(env0, "Long", "EUR", qlEvalDateStr, qlEndDateStr, "", 0., 0.)

    # ore/OREData/ored/portfolio/fxaverageforward.hpp

    ql.FxAverageForward()
    ql.FxAverageForward(env0, schedule_data0, qlEvalDateStr, False, "EUR", 0., "EUR", 0., "")

    # ore/OREData/ored/portfolio/fxdigitalbarrieroption.hpp

    ql.FxDigitalBarrierOption()
    ql.FxDigitalBarrierOption(env0, option_data0, barrier_data0, 0., 0., "EUR", "EUR")

    # ore/OREData/ored/portfolio/fxdigitaloption.hpp

    ql.FxDigitalOption()
    ql.FxDigitalOption(env0, option_data0, 0., "EUR", 0., "EUR", "EUR")
    ql.FxDigitalOption(env0, option_data0, 0., 0., "EUR", "EUR")

    # ore/OREData/ored/portfolio/fxdoublebarrieroption.hpp

    ql.FxDoubleBarrierOption()
    ql.FxDoubleBarrierOption(env0, option_data0, barrier_data0, qlEvalDate, "TARGET", "EUR", 0., "EUR", 0.)

    # ore/OREData/ored/portfolio/fxdoubletouchoption.hpp

    ql.FxDoubleTouchOption()
    ql.FxDoubleTouchOption(env0, option_data0, barrier_data0, "EUR", "EUR", "EUR", 0.)

    # ore/OREData/ored/portfolio/fxeuropeanbarrieroption.hpp

    ql.FxEuropeanBarrierOption()
    ql.FxEuropeanBarrierOption(env0, option_data0, barrier_data0, "EUR", 0., "EUR", 0.)

    # ore/OREData/ored/portfolio/fxforward.hpp

    ql.OREFxForward()
    ql.OREFxForward(env0, qlEvalDateStr, "EUR", 0., "EUR", 0.)

    # ore/OREData/ored/portfolio/fxkikobarrieroption.hpp

    ql.FxKIKOBarrierOption()
    ql.FxKIKOBarrierOption(env0, option_data0, [barrier_data0], "EUR", 0., "EUR", 0.)

    # ore/OREData/ored/portfolio/fxkikobarrieroption.hpp

    ql.FxKIKOBarrierOption()
    ql.FxKIKOBarrierOption(env0, option_data0, [barrier_data0], "EUR", 0., "EUR", 0.)

    # ore/OREData/ored/portfolio/fxswap.hpp

    ql.FxSwap()
    ql.FxSwap(env0, qlEvalDateStr, qlEndDateStr, "EUR", 0., "EUR", 0., 0., 0.)

    # ore/OREData/ored/portfolio/genericbarrieroption.hpp

    ql.GenericBarrierOption()
    ql.GenericBarrierOption(equity_underlying0, option_data0, [barrier_data0],
        schedule_data0, barrier_data0, "EUR", qlEvalDateStr, "", "", "", "")
    ql.GenericBarrierOption([equity_underlying0], option_data0, [barrier_data0],
        schedule_data0, [barrier_data0], "EUR", qlEvalDateStr, "", "", "", "")
    ql.EquityGenericBarrierOption()
    ql.FxGenericBarrierOption()
    ql.CommodityGenericBarrierOption()

    # ore/OREData/ored/portfolio/indexcreditdefaultswap.hpp

    ql.IndexCreditDefaultSwap()
    ql.IndexCreditDefaultSwap(env0, swap_data0, basket_data0)

    # ore/OREData/ored/portfolio/indexcreditdefaultswapoption.hpp

    ql.IndexCreditDefaultSwapOption()
    ql.IndexCreditDefaultSwapOption(env0, swap_data0, option_data0, 0.)

    # ore/OREData/ored/portfolio/indexing.hpp

    ql.Indexing()
    ql.Indexing("")

    # ore/OREData/ored/portfolio/inflationswap.hpp

    ql.InflationSwap()
    ql.InflationSwap(env0, [leg_data0])
    ql.InflationSwap(env0, leg_data0, leg_data0)

    # ore/OREData/ored/portfolio/instrumentwrapper.hpp

    ql.VanillaInstrument(bond0)

    # ore/OREData/ored/portfolio/knockoutswap.hpp

    ql.KnockOutSwap()
    ql.KnockOutSwap([leg_data0], barrier_data0, qlEvalDateStr)

    # ore/OREData/ored/portfolio/legbuilders.hpp

    ql.FixedLegBuilder()
    ql.ZeroCouponFixedLegBuilder()
    ql.FloatingLegBuilder()
    ql.CashflowLegBuilder()
    ql.CPILegBuilder()
    ql.YYLegBuilder()
    ql.CMSLegBuilder()
    ql.CMBLegBuilder()
    ql.DigitalCMSLegBuilder()
    ql.CMSSpreadLegBuilder()
    ql.DigitalCMSSpreadLegBuilder()
    ql.EquityLegBuilder()

    # ore/OREData/ored/portfolio/legdata.hpp

    ql.CashflowData()
    ql.CashflowData([0.], [""])
    ql.FixedLegData()
    ql.FixedLegData([0.])
    ql.ZeroCouponFixedLegData()
    ql.ZeroCouponFixedLegData([0.])
    ql.FloatingLegData()
    ql.FloatingLegData("EUR-EURIBOR-6M", 0, False, [0.])
    ql.CPILegData()
    ql.CPILegData()
    ql.YoYLegData()
    ql.YoYLegData("", "", 0)
    ql.CMSLegData()
    ql.CMSLegData("", 0, False, [0.])
    ql.DigitalCMSLegData()
    ql.DigitalCMSLegData(ql.CMSLegData())
    ql.CMSSpreadLegData()
    ql.CMSSpreadLegData("", "", 0, False, [0.])
    ql.DigitalCMSSpreadLegData()
    ql.DigitalCMSSpreadLegData(ql.CMSSpreadLegData())
    ql.CMBLegData()
    ql.CMBLegData("", False, 0, False, [0.])
    ql.EquityLegData()
    #ql.EquityLegData(ql.EquityReturnType_Price, 0., equity_underlying0, 0., False)
    ql.AmortizationData()
    ql.AmortizationData("", 0., qlEvalDateStr, qlEndDateStr, "", False)
    ql.LegData()
    ql.LegData(ql.CashflowData(), False, "EUR")

    # ore/OREData/ored/portfolio/multilegoption.hpp

    ql.MultiLegOption()
    ql.MultiLegOption(env0, [leg_data0])
    ql.MultiLegOption(env0, option_data0, [leg_data0])

    # ore/OREData/ored/portfolio/nettingsetdetails.hpp

    ql.NettingSetDetails()
    ql.NettingSetDetails("")

    # ore/OREData/ored/portfolio/nettingsetdefinition.hpp

    ql.CSA(ql.CSA.Bilateral, "EUR", "", 0., 0., 0., 0., 0., "", period_1Y, period_1Y, period_1Y,
        0., 0., [""], False, ql.CSA.Bilateral, False, False, "")
    ql.NettingSetDefinition()
    # FIXME need to export XMLNode
    #ql.NettingSetDefinition(XMLNode)
    ql.NettingSetDefinition(ql.NettingSetDetails("x"))
    ql.NettingSetDefinition("x")
    ql.NettingSetDefinition(ql.NettingSetDetails("x"), "Bilateral", "EUR", "", 0., 0., 0., 0., 0.,
        "FIXED", "1D", "1D", "1D", 0., 0., ["EUR"])
    ql.NettingSetDefinition("x", "Bilateral", "EUR", "", 0., 0., 0., 0., 0., "FIXED", "1D", "1D", "1D", 0., 0., ["EUR"])

    # ore/OREData/ored/portfolio/nettingsetmanager.hpp

    ql.NettingSetManager()

    # ore/OREData/ored/portfolio/optiondata.hpp

    ql.ExerciseBuilder(option_data0, ql.LegVector())

    # ore/OREData/ored/portfolio/optionexercisedata.hpp

    ql.OptionExerciseData()
    ql.OptionExerciseData(qlEvalDateStr, "")

    # ore/OREData/ored/portfolio/optionpaymentdata.hpp

    ql.OptionPaymentData()
    ql.OptionPaymentData([qlEvalDateStr])
    ql.OptionPaymentData("0", "TARGET", "F")

    # ore/OREData/ored/portfolio/pairwisevarianceswap.hpp

    ql.EqPairwiseVarSwap()
    ql.EqPairwiseVarSwap(env0, "Long", [equity_underlying0], [0.], [0.], 0., 0., schedule_data0, "EUR", qlEvalDateStr, schedule_data0)
    ql.FxPairwiseVarSwap()
    ql.FxPairwiseVarSwap(env0, "Long", [equity_underlying0], [0.], [0.], 0., 0., schedule_data0, "EUR", qlEvalDateStr, schedule_data0)

    # ore/OREData/ored/portfolio/performanceoption_01.hpp

    ql.PerformanceOption_01()
    ql.PerformanceOption_01(env0, "", "", qlEvalDateStr, qlEndDateStr, [equity_underlying0], [""], "", False, "", "EUR")

    # ore/OREData/ored/portfolio/rainbowoption.hpp

    ql.RainbowOption()
    ql.RainbowOption("EUR", "", "", [equity_underlying0], option_data0, "")
    # FIXME why does this trigger a run time error?
    #ql.EquityRainbowOption()
    ql.EquityRainbowOption(ql.Conventions())
    ql.FxRainbowOption(ql.Conventions())
    ql.CommodityRainbowOption(ql.Conventions())

    # ore/OREData/ored/portfolio/rangebound.hpp

    ql.RangeBound()
    ql.RangeBound(0., 0., 0., 0., 0.)

    # ore/OREData/ored/portfolio/referencedata.hpp

    ql.BondReferenceDatum()
    ql.BondReferenceDatum("")
    ql.BondReferenceDatum("", qlEvalDate)
    ql.BondReferenceDatum("", ql.BondReferenceDatum_BondData())
    ql.BondReferenceDatum("", qlEvalDate, ql.BondReferenceDatum_BondData())
    ql.BondFutureReferenceDatum()
    ql.BondFutureReferenceDatum("")
    ql.BondFutureReferenceDatum("", ql.BondFutureData())
    ql.BondFutureReferenceDatum("", qlEvalDate)
    ql.BondFutureReferenceDatum("", qlEvalDate, ql.BondFutureData())
    ql.CreditIndexConstituent()
    ql.CreditIndexConstituent("", 0.)
    ql.CreditIndexReferenceDatum()
    ql.CreditIndexReferenceDatum("")
    ql.CreditIndexReferenceDatum("", qlEvalDate)
    ql.EquityIndexReferenceDatum()
    ql.EquityIndexReferenceDatum("")
    ql.EquityIndexReferenceDatum("", qlEvalDate)
    ql.CommodityIndexReferenceDatum()
    ql.CommodityIndexReferenceDatum("")
    ql.CommodityIndexReferenceDatum("", qlEvalDate)
    ql.CurrencyHedgedEquityIndexReferenceDatum()
    ql.CurrencyHedgedEquityIndexReferenceDatum("")
    ql.CurrencyHedgedEquityIndexReferenceDatum("", qlEvalDate)
    ql.PortfolioBasketReferenceDatum()
    ql.PortfolioBasketReferenceDatum("")
    ql.PortfolioBasketReferenceDatum("", qlEvalDate)
    ql.CreditReferenceDatum()
    ql.CreditReferenceDatum("")
    ql.CreditReferenceDatum("", qlEvalDate)
    ql.CreditReferenceDatum("", ql.CreditData())
    ql.CreditReferenceDatum("", qlEvalDate, ql.CreditData())
    ql.EquityReferenceDatum()
    ql.EquityReferenceDatum("")
    ql.EquityReferenceDatum("", qlEvalDate)
    ql.EquityReferenceDatum("", ql.EquityData())
    ql.EquityReferenceDatum("", qlEvalDate, ql.EquityData())
    ql.BondBasketReferenceDatum()
    ql.BondBasketReferenceDatum("")
    ql.BondBasketReferenceDatum("", qlEvalDate)
    ql.BondBasketReferenceDatum("", [ql.BondUnderlying()])
    ql.BondBasketReferenceDatum("", qlEvalDate, [ql.BondUnderlying()])
    ql.BasicReferenceDataManager()

    # ore/OREData/ored/portfolio/riskparticipationagreement.hpp

    ql.RiskParticipationAgreement()
    ql.RiskParticipationAgreement(env0, [leg_data0], [leg_data0], 0., qlEvalDate, qlEndDate, "")
    ql.RiskParticipationAgreement(env0, ql.TreasuryLockData(), [leg_data0], 0., qlEvalDate, qlEndDate, "")

    # ore/OREData/ored/portfolio/schedule.hpp

    ql.ScheduleDates()
    ql.ScheduleDates("TARGET", "MF", "1Y", [qlEvalDateStr])
    ql.ScheduleDerived()
    ql.ScheduleDerived("", "TARGET", "MF", "")

    # ore/OREData/ored/portfolio/simmcreditqualifiermapping.hpp

    ql.SimmCreditQualifierMapping()
    ql.SimmCreditQualifierMapping("", "", False)

    # ore/OREData/ored/portfolio/strikeresettableoption.hpp

    ql.StrikeResettableOption()
    ql.StrikeResettableOption(env0, "Long", "Put", "EUR", "", "", "", "", "",
        equity_underlying0, schedule_data0, qlEvalDateStr, qlEndDateStr, "", qlEndDateStr)
    ql.EquityStrikeResettableOption()
    ql.FxStrikeResettableOption()
    ql.CommodityStrikeResettableOption()

    # ore/OREData/ored/utilities/log.hpp
    # FIXME need to export class StructuredMessage
    # ql.StructuredMessage()

    # FIXME the tests below are commented out because they generate output
    # containing a timestamp, which breaks automated testing

    # ore/OREData/ored/portfolio/structuredconfigurationerror.hpp

    #ql.StructuredConfigurationErrorMessage("", "", "", "")

    # ore/OREData/ored/portfolio/structuredconfigurationwarning.hpp

    #ql.StructuredConfigurationWarningMessage("", "", "", "")

    # ore/OREData/ored/portfolio/structuredtradeerror.hpp

    #ql.StructuredTradeErrorMessage(ql.OREBond(), "", "")
    #ql.StructuredTradeErrorMessage("", "", "", "")

    # ore/OREData/ored/portfolio/structuredtradewarning.hpp

    #ql.StructuredTradeWarningMessage(ql.OREBond(), "", "")
    #ql.StructuredTradeWarningMessage("", "", "", "")

    # ore/OREData/ored/portfolio/swaptionstraddle.hpp

    ql.SwaptionStraddle()
    ql.SwaptionStraddle(env0, option_data0, [leg_data0])

    # ore/OREData/ored/portfolio/tarf.hpp

    ql.TaRF()
    ql.TRS(env0, trade_vector0, [""], ql.ReturnData(), ql.FundingData(), ql.AdditionalCashflowData())
    #ql.TaRF("EUR", "", "", "", [""], [""], equity_underlying0, schedule_data0, "", "TARGET", "",
    ql.CFD(env0, trade_vector0, [""], ql.ReturnData(), ql.FundingData(), ql.AdditionalCashflowData())
    ql.EquityTaRF()
    ql.FxTaRF()
    ql.CommodityTaRF()

    # ore/OREData/ored/portfolio/tlockdata.hpp

    ql.TreasuryLockData()
    ql.TreasuryLockData(False, bond_data0, 0., "", qlEvalDateStr, 0, "TARGET")

    # ore/OREData/ored/portfolio/tradeactions.hpp

    ql.TradeAction()
    ql.TradeAction("", "", schedule_data0)
    ql.TradeActions()

    # ore/OREData/ored/portfolio/trademonetary.hpp

    ql.TradeMonetary()
    ql.TradeMonetary(0.)
    ql.TradeMonetary("0")

    # ore/OREData/ored/portfolio/tranche.hpp

    ql.TrancheData()
    ql.TrancheData("", 0., 0., ql.CashflowData())

    # ore/OREData/ored/portfolio/trs.hpp

    ql.ReturnData()
    # FIXME This triggers a run time error, I think because
    # optional<FXConversion> is not supported correctly
    #ql.ReturnData(False, "EUR", schedule_data0, "", "", "TARGET", "", "", "TARGET",
    #    [qlEvalDateStr], 0., "EUR", [""], False, ql.TRS.FXConversion_Start)
    ql.FundingData()
    # FIXME this overload not exported yet
    #ql.FundingData([leg_data0])
    ql.AdditionalCashflowData()
    ql.AdditionalCashflowData(leg_data0)
    ql.TRS()
    ql.TRS(env0, [ql.OREBond()], [""], ql.ReturnData(), ql.FundingData(), ql.AdditionalCashflowData())
    ql.CFD()
    ql.CFD(env0, [ql.OREBond()], [""], ql.ReturnData(), ql.FundingData(), ql.AdditionalCashflowData())

    # ore/OREData/ored/portfolio/trswrapper.hpp

    # WIP

    # ore/OREData/ored/portfolio/varianceswap.hpp

    ql.EqVarSwap()
    ql.EqVarSwap(env0, "Long", equity_underlying0, "EUR", 0., 0., qlEvalDateStr, qlEndDateStr, "", False)
    ql.FxVarSwap()
    ql.FxVarSwap(env0, "Long", equity_underlying0, "EUR", 0., 0., qlEvalDateStr, qlEndDateStr, "", False)
    ql.ComVarSwap()
    ql.ComVarSwap(env0, "Long", equity_underlying0, "EUR", 0., 0., qlEvalDateStr, qlEndDateStr, "", False)

    # ore/OREData/ored/portfolio/windowbarrieroption.hpp

    ql.WindowBarrierOption()
    ql.WindowBarrierOption("EUR", "", trade_strike0, equity_underlying0, qlEvalDateStr, qlEndDateStr, option_data0, barrier_data0)
    ql.EquityWindowBarrierOption()
    ql.FxWindowBarrierOption()
    ql.CommodityWindowBarrierOption()

    # ore/OREData/ored/portfolio/worstofbasketswap.hpp

    ql.WorstOfBasketSwap()
    x0 = ql.ScriptedTradeEventData()
    x1 = ql.Thirty360(ql.Thirty360.BondBasis)
    ql.WorstOfBasketSwap(env0, "Long", "", "", "", [""], "", x0, x0, x0, x0, x0, x0, x0,
        qlEvalDateStr, qlEvalDateStr, False, False, False, False, "", "", "",
        x1, period_1Y, False, "EUR", [equity_underlying0], "", [""], [""], x0)
    ql.EquityWorstOfBasketSwap()
    ql.FxWorstOfBasketSwap()
    ql.CommodityWorstOfBasketSwap()


if __name__ == "__main__":
    main()
