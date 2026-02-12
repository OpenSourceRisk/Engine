
# This script ports the code below from C++ to python:
# ore/OREAnalytics/test/testportfolio.cpp
# This script constructs trades but does not price them and there is no output.

import ORE as ql

todaysDate = ql.Date(6, ql.November, 2001)
ql.Settings.instance().evaluationDate = todaysDate

def buildSwap(trade_id, ccy, isPayer, notional, start, term, rate, spread,
              fixedFreq, fixedDC, floatFreq, floatDC, index, calendar = ql.TARGET(),
              spotDays = 2, spotStartLag = False):

    today = ql.Settings.instance().evaluationDate
    cal = calendar.name()
    conv = "MF"
    rule = "Forward"

    notionals = [notional]
    rates = [rate]
    spreads = [spread]

    spotStartLagTenor = ql.Period(spotDays, ql.Days) if spotStartLag else ql.Period(0, ql.Days)

    qlStartDate = calendar.adjust(today + spotStartLagTenor + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    fixedSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule))
    floatSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule))
    fixedLeg = ql.LegData(ql.FixedLegData(rates), isPayer, ccy, fixedSchedule, fixedDC, notionals)
    floatingLeg = ql.LegData(ql.FloatingLegData(index, spotDays, False, spreads),
        not isPayer, ccy, floatSchedule, floatDC, notionals)

    env = ql.Envelope("CP")
    trade = ql.ORESwap(env, floatingLeg, fixedLeg)
    trade.setId(trade_id)
    return trade;

buildSwap("1_Swap_EUR", "EUR", True, 10000000.0, 0, 10, 0.03, 0.00, "1Y",
          "30/360", "6M", "A360", "EUR-EURIBOR-6M")

def buildEuropeanSwaption(trade_id, longShort, ccy, isPayer, notional, start,
                          term, rate, spread, fixedFreq, fixedDC, floatFreq,
                          floatDC, index, cashPhysical = "Cash", premium = 0.0,
                          premiumCcy = "", premiumDate = ""):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    days = 2
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    notionals = [notional]
    rates = [rate]
    spreads = [spread]

    qlStartDate = calendar.adjust(today + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    fixedSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule))
    floatSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule))
    fixedLeg = ql.LegData(ql.FixedLegData(rates), isPayer, ccy, fixedSchedule, fixedDC, notionals)
    floatingLeg = ql.LegData(ql.FloatingLegData(index, days, False, spreads),
        not isPayer, ccy, floatSchedule, floatDC, notionals)
    legData = ql.LegDataVector()
    legData.push_back(fixedLeg)
    legData.push_back(floatingLeg)

    option = ql.OptionData(longShort, "Call", "European", False, [startDate], cashPhysical, "",
        ql.PremiumData(premium, premiumCcy, ql.parseDate(premiumDate)) if premiumDate else ql.PremiumData())

    env = ql.Envelope("CP")
    trade = ql.ORESwaption(env, option, legData)
    trade.setId(trade_id)
    return trade;

buildEuropeanSwaption("5_Swaption_EUR", "Long", "EUR", True, 1000000.0, 10, 10,
                      0.03, 0.00, "1Y", "30/360", "6M", "A360", "EUR-EURIBOR-6M")

def buildBermudanSwaption(trade_id, longShort, ccy, isPayer, notional,
                          exercises, start, term, rate, spread, fixedFreq,
                          fixedDC, floatFreq, floatDC, index, cashPhysical =
                          "Cash", premium = 0.0, premiumCcy = "", premiumDate = ""):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    days = 2
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    notionals = [notional]
    rates = [rate]
    spreads = [spread]

    qlStartDate = calendar.adjust(today + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    exerciseDates = []
    for i in range(0, exercises):
        exerciseDate = qlStartDate + ql.Period(i, ql.Years)
        exerciseDates.append(ql.to_string(exerciseDate))

    fixedSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule))
    floatSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule))
    fixedLeg = ql.LegData(ql.FixedLegData(rates), isPayer, ccy, fixedSchedule, fixedDC, notionals)
    floatingLeg = ql.LegData(ql.FloatingLegData(index, days, False, spreads),
        not isPayer, ccy, floatSchedule, floatDC, notionals)
    legData = ql.LegDataVector()
    legData.push_back(fixedLeg)
    legData.push_back(floatingLeg)

    option = ql.OptionData(longShort, "Call", "Bermudan", False, exerciseDates, cashPhysical, "",
        ql.PremiumData(premium, premiumCcy, ql.parseDate(premiumDate)) if premiumDate else ql.PremiumData())

    env = ql.Envelope("CP")
    trade = ql.ORESwaption(env, option, legData)
    trade.setId(trade_id)
    return trade;

buildBermudanSwaption("13_Swaption_EUR", "Long", "EUR", True, 1000000.0, 5, 2, 10, 0.02, 0.00, "1Y",
                      "30/360", "6M", "A360", "EUR-EURIBOR-6M")

def buildFxOption(trade_id, longShort, putCall, expiry, boughtCcy,
                           boughtAmount, soldCcy, soldAmount, premium = 0.0,
                           premiumCcy = "", premiumDate = "", nettingSet = ""):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    qlExpiry = calendar.adjust(today + ql.Period(expiry, ql.Years))
    expiryDate = ql.to_string(qlExpiry)

    option = ql.OptionData(longShort, putCall, "European", False, [expiryDate], "Cash", "",
        ql.PremiumData(premium, premiumCcy, ql.parseDate(premiumDate)) if premiumDate else ql.PremiumData())

    env = ql.Envelope("CP", nettingSet)
    trade = ql.FxOption(env, option, boughtCcy, boughtAmount, soldCcy, soldAmount)
    trade.setId(trade_id)
    return trade;

buildFxOption("7_FxOption_EUR_USD", "Long", "Call", 3, "EUR", 10000000.0, "USD", 11000000.0)

def buildFxForward(trade_id, expiry, boughtCcy, boughtAmount, soldCcy,
                   soldAmount, nettingSet = ""):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    qlExpiry = calendar.adjust(today + ql.Period(expiry, ql.Years))
    expiryDate = ql.to_string(qlExpiry)

    env = ql.Envelope("CP", nettingSet)
    trade = ql.OREFxForward(env, expiryDate, boughtCcy, boughtAmount, soldCcy, soldAmount)
    trade.setId(trade_id)
    return trade;

buildFxForward("1_FxForward", 15, "GBP", 1000, "USD", 1200, "NS")

def buildEquityOption(trade_id, longShort, putCall, expiry, equityName,
                      currency, strike, quantity, premium = 0.0, premiumCcy =
                      "", premiumDate = ""):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    qlExpiry = calendar.adjust(today + ql.Period(expiry, ql.Years))
    expiryDate = ql.to_string(qlExpiry)

    tradeStrike = ql.TradeStrike(strike, currency)

    option = ql.OptionData(longShort, putCall, "European", False, [expiryDate], "Cash", "",
        ql.PremiumData(premium, premiumCcy, ql.parseDate(premiumDate)) if premiumDate else ql.PremiumData())

    env = ql.Envelope("CP")
    trade = ql.EquityOption(env, option, ql.EquityUnderlying(equityName), currency, quantity, tradeStrike)
    trade.setId(trade_id)
    return trade;

buildEquityOption("14_EquityOption_SP5", "Long", "Call", 2, "SP5", "USD", 2147.56, 775)

def buildEquityForward(trade_id, longShort, expiry, equityName, currency,
                       strike, quantity):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()

    qlExpiry = calendar.adjust(today + ql.Period(expiry, ql.Years))
    expiryDate = ql.to_string(qlExpiry)

    env = ql.Envelope("CP")
    trade = ql.OREEquityForward(env, longShort, ql.EquityUnderlying(equityName),
        currency, quantity, expiryDate, strike)
    trade.setId(trade_id)
    return trade;

buildEquityForward("Fwd_SP5", "Long", 2, "SP5", "USD", 2147.56, 1000)

def buildCapFloor(trade_id, ccy, longShort, capRates, floorRates, notional,
                  start, term, floatFreq, floatDC, index, calendar = ql.TARGET(),
                  fixingDays = 2, spotStartLag = False):

    today = ql.Settings.instance().evaluationDate
    cal = calendar.name()
    conv = "MF"
    rule = "Forward"

    notionals = [notional]
    spreads = [0.0]

    spotStartLagTenor = ql.Period(spotDays, ql.Days) if spotStartLag else ql.Period(0, ql.Days)

    qlStartDate = calendar.adjust(today + spotStartLagTenor + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    floatSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule))
    floatingLeg = ql.LegData(ql.FloatingLegData(index, fixingDays, False, spreads), False, ccy,
        floatSchedule, floatDC, notionals)

    env = ql.Envelope("CP")
    trade = ql.ORECapFloor(env, longShort, floatingLeg, capRates, floorRates)
    trade.setId(trade_id)
    return trade;

def buildCap(trade_id, ccy, longShort, capRate, notional, start, term,
             floatFreq, floatDC, index, calendar = ql.TARGET(), spotDays = 2,
             spotStartLag = False):

    return buildCapFloor(trade_id, ccy, longShort, [capRate], [], notional,
                         start, term, floatFreq, floatDC, index, calendar,
                         spotDays, spotStartLag);

def buildFloor(trade_id, ccy, longShort, floorRate, notional, start,
                                    term, floatFreq, floatDC, index,
                                    calendar = ql.TARGET(), spotDays = 2,
                                    spotStartLag = False):

    return buildCapFloor(trade_id, ccy, longShort, [], [floorRate], notional,
                         start, term, floatFreq, floatDC, index, calendar,
                         spotDays, spotStartLag)

buildCap("9_Cap_EUR", "EUR", "Long", 0.05, 1000000.0, 0, 10, "6M", "A360", "EUR-EURIBOR-6M")
buildFloor("10_Floor_USD", "USD", "Long", 0.01, 1000000.0, 0, 10, "3M", "A360", "USD-LIBOR-3M")

def buildCrossCcyBasisSwap(trade_id, recCcy, recNotional, payCcy, payNotional,
                           start, term, recLegSpread, payLegSpread, recFreq,
                           recDC, recIndex, recCalendar, payFreq, payDC,
                           payIndex, payCalendar, spotDays = 2, spotStartLag =
                           False, notionalInitialExchange = False,
                           notionalFinalExchange = False,
                           notionalAmortizingExchange = False,
                           isRecLegFXResettable = False, isPayLegFXResettable =
                           False, nettingSet = "", amortizingNotional = False,
                           amortizingTerm = ""):

    today = ql.Settings.instance().evaluationDate
    payCal = payCalendar.name()
    recCal = recCalendar.name()
    conv = "MF"
    rule = "Forward"

    recNotionals = [recNotional]
    recSpreads = [recLegSpread]
    payNotionals = [payNotional]
    paySpreads = [payLegSpread]

    spotStartLagTenor = ql.Period(spotDays, ql.Days) if spotStartLag else ql.Period(0, ql.Days)

    qlStartDate = recCalendar.adjust(today + spotStartLagTenor + ql.Period(start, ql.Years))
    qlEndDate = recCalendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    recSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, recFreq, recCal, conv, conv, rule))
    paySchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, payFreq, payCal, conv, conv, rule))

    amortizationData = ql.AmortizationDataVector()
    if (amortizingNotional):
        ad = ql.AmortizationData("FixedAmount", 10000, startDate, endDate, amortizingTerm, False)
        amortizationData.push_back(ad)

    recFloatingLegData = ql.FloatingLegData(recIndex, spotDays, False, recSpreads)
    if (isRecLegFXResettable):
        fxIndex = "FX-ECB-" + recCcy + "-" + payCcy
        recFloatingLeg = ql.LegData(recFloatingLegData, False, recCcy, recSchedule, recDC, recNotionals, [],
            conv, notionalInitialExchange, notionalFinalExchange, notionalAmortizingExchange,
            isRecLegFXResettable, payCcy, payNotional, "", fxIndex, amortizationData)
    else:
        recFloatingLeg = ql.LegData(recFloatingLegData, False, recCcy, recSchedule, recDC, recNotionals, [],
            conv, notionalInitialExchange, notionalFinalExchange, notionalAmortizingExchange, True,
            "", 0, "", "", amortizationData)

    payFloatingLegData = ql.FloatingLegData(payIndex, spotDays, False, recSpreads)
    if (isPayLegFXResettable):
        fxIndex = "FX-ECB-" + payCcy + "-" + recCcy
        payFloatingLeg = ql.LegData(payFloatingLegData, True, payCcy, paySchedule, payDC, payNotionals, [],
            conv, notionalInitialExchange, notionalFinalExchange, notionalAmortizingExchange,
            not isPayLegFXResettable, recCcy, recNotional, "", fxIndex)
    else:
        payFloatingLeg = ql.LegData(payFloatingLegData, True, payCcy, paySchedule, payDC, payNotionals, [],
            conv, notionalInitialExchange, notionalFinalExchange, notionalAmortizingExchange)

    env = ql.Envelope("CP", nettingSet)
    trade = ql.ORESwap(env, recFloatingLeg, payFloatingLeg)
    trade.setId(trade_id)
    return trade

buildCrossCcyBasisSwap("5_XCCY_Basis_Swap", "EUR", 10000000, "USD", 10000000,
                       0, 20, 0.0000, 0.0000, "3M", "A360", "EUR-EURIBOR-3M",
                       ql.TARGET(), "3M", "A360", "USD-LIBOR-3M", ql.TARGET(), 2, True,
                       False, False, False, False, False, "NS", False)

def buildZeroBond(trade_id, ccy, notional, term, suffix = "1"):

    today = ql.Settings.instance().evaluationDate
    qlEndDate = today + ql.Period(term, ql.Years)
    issueDate = ql.to_string(today)
    maturityDate = ql.to_string(qlEndDate)

    settlementDays = "2"
    calendar = "TARGET"
    issuerId = "BondIssuer" + suffix
    creditCurveId = "BondIssuer" + suffix
    securityId = "Bond" + suffix
    referenceCurveId = "BondCurve" + suffix

    env = ql.Envelope("CP")
    trade = ql.OREBond(env, ql.BondData(issuerId, creditCurveId, securityId, referenceCurveId, settlementDays,
        calendar, notional, maturityDate, ccy, issueDate))
    trade.setId(trade_id)
    return trade;

buildZeroBond("11_ZeroBond_EUR", "EUR", 1.0, 10, "0")

def buildCreditDefaultSwap(trade_id, ccy, issuerId, creditCurveId, isPayer, notional,
    start, term, rate, spread, fixedFreq, fixedDC):

    today = ql.Settings.instance().evaluationDate

    settlementDays = "1"
    calendar = ql.WeekendsOnly()
    cal = "WeekendsOnly"
    conv = "F"
    convEnd = "U"
    rule = "CDS2015"

    notionals = [notional]
    rates = [rate]
    spreads = [spread]

    qlStartDate = calendar.adjust(today + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    fixedSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, fixedFreq, cal, conv, convEnd, rule))
    fixedLeg = ql.LegData(ql.FixedLegData(spreads), isPayer, ccy, fixedSchedule, fixedDC, notionals)

    env = ql.Envelope("CP")
    swap = ql.CreditDefaultSwapData(issuerId, creditCurveId, fixedLeg, True,
        ql.CreditDefaultSwap.atDefault, today + ql.Period(1, ql.Days))
    trade = ql.ORECreditDefaultSwap(env, swap)
    trade.setId(trade_id)
    return trade;

buildCreditDefaultSwap("9_CDS_USD", "USD", "dc", "dc", True, 10000000, 0, 15, 0.4, 0.009, "1Y", "30/360")

def buildSyntheticCDO(trade_id, name, names, longShort, ccy, ccys, isPayer,
    notionals, notional, start, term, rate, spread, fixedFreq, fixedDC):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.WeekendsOnly()
    cal = "WeekendsOnly"
    conv = "F"
    rule = "CDS2015"
    issuerId = name
    creditCurveId = name
    qualifier = "Tranch1"

    attachmentPoint = 0.0;
    detachmentPoint = 0.1;
    settlesAccrual = True;
    protectionPaymentTime = ql.CreditDefaultSwap.atDefault

    upfrontFee = 0.0;

    notionalTotal = [notional] * len(names)
    rates = [rate] * len(names)
    spreads = [spread] * len(names)

    qlStartDate = calendar.adjust(today + ql.Period(start, ql.Years))
    qlProtectionStartDate = calendar.advance(qlStartDate, ql.Period(1, ql.Days))
    qlUpfrontDate = calendar.advance(qlStartDate, ql.Period(3, ql.Days))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    protectionStart = ql.to_string(qlProtectionStartDate)
    upfrontDate = ql.to_string(qlUpfrontDate)
    endDate = ql.to_string(qlEndDate)

    fixedSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule))
    fixedLeg = ql.LegData(ql.FixedLegData(rates), isPayer, ccy, fixedSchedule, fixedDC, notionalTotal)
    constituents = []
    for i in range(0, len(names)):
        constituents.append(ql.BasketConstituent(names[i], names[i], notionals[i], ccys[i], qualifier))
    basket = ql.BasketData(constituents)
    swap = ql.IndexCreditDefaultSwapData(creditCurveId, basket, fixedLeg)

    env = ql.Envelope("CP")
    trade = ql.SyntheticCDO(env, fixedLeg, qualifier, basket, attachmentPoint, detachmentPoint,
        settlesAccrual, protectionPaymentTime, protectionStart, upfrontDate, upfrontFee)
    trade.setId(trade_id)
    return trade;

buildSyntheticCDO("16_SyntheticCDO_EUR", "dc2", ["dc2"], "Long", "EUR", ["EUR"],
    True, [10000000.0], 1000000.0, 0, 5, 0.03, 0.01, "1Y", "30/360")


def buildCmsCapFloor(trade_id, ccy, indexId, isPayer, notional, start, term, capRate,
    floorRate, spread, freq, dc):

    today = ql.Settings.instance().evaluationDate

    settlementDays = "2";
    calendar = ql.TARGET()
    cal = "TARGET";
    qualifier = "";
    conv = "MF";
    rule = "Forward";

    qlStartDate = calendar.adjust(today + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    longShort = "Long"

    notionals = [notional]
    caps = [capRate]
    floors = [floorRate]
    spreads = [spread]
    isInArrears = False

    schedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, freq, cal, conv, conv, rule))

    cmsLeg = ql.LegData(ql.CMSLegData(indexId, 0, isInArrears, spreads, [startDate]),
        isPayer, ccy, schedule, dc, notionals, [startDate])

    env = ql.Envelope("CP")
    trade = ql.ORECapFloor(env, longShort, cmsLeg, [], floors)
    trade.setId(trade_id)
    return trade;

buildCmsCapFloor("17_CMS_EUR", "EUR", "EUR-CMS-30Y", True, 2000000, 0, 5, 0.0,
    1, 0.0, "1Y", "30/360")

def buildCPIInflationSwap(trade_id, ccy, isPayer, notional, start, term, spread,
    floatFreq, floatDC, index, cpiFreq, cpiDC, cpiIndex, baseRate, observationLag,
    interpolated, cpiRate):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    days = 2
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    notionals = [notional]
    cpiRates = [cpiRate]
    spreads = [spread]

    qlStartDate = calendar.adjust(today + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    cpiSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, cpiFreq, cal, conv, conv, rule))
    floatSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule))
    cpiLeg = ql.LegData(ql.CPILegData(cpiIndex, startDate, baseRate, observationLag,
        "Linear" if interpolated else "Flat", cpiRates),
        isPayer, ccy, cpiSchedule, cpiDC, notionals, [], "F", False, True)
    floatingLeg = ql.LegData(ql.FloatingLegData(index, days, False, spreads),
        not isPayer, ccy, floatSchedule, floatDC, notionals)

    env = ql.Envelope("CP")
    trade = ql.ORESwap(env, floatingLeg, cpiLeg)
    trade.setId(trade_id)
    return trade;

buildCPIInflationSwap("19_CPIInflationSwap_UKRPI", "GBP", True, 100000.0, 0, 10,
    0.0, "6M", "ACT/ACT", "GBP-LIBOR-6M", "1Y", "ACT/ACT",
    "UKRPI", 201.0, "2M", False, 0.005)

def buildYYInflationSwap(trade_id, ccy, isPayer, notional, start, term, spread,
                         floatFreq, floatDC, index, yyFreq, yyDC, yyIndex,
                         observationLag, fixDays):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    days = 2
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    notionals = [notional]
    spreads = [spread]

    qlStartDate = calendar.adjust(today + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    yySchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, yyFreq, cal, conv, conv, rule))
    floatSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule))
    yyLeg = ql.LegData(ql.YoYLegData(yyIndex, observationLag, fixDays),
        isPayer, ccy, yySchedule, yyDC, notionals)
    floatingLeg = ql.LegData(ql.FloatingLegData(index, days, False, spreads),
        not isPayer, ccy, floatSchedule, floatDC, notionals)

    env = ql.Envelope("CP")
    trade = ql.ORESwap(env, floatingLeg, yyLeg)
    trade.setId(trade_id)
    return trade;

buildYYInflationSwap("16_YoYInflationSwap_UKRPI", "GBP", True, 100000.0, 0, 10,
                     0.0, "1Y", "ACT/ACT", "GBP-LIBOR-6M", "1Y", "ACT/ACT",
                     "UKRPI", "2M", 2)

def buildYYInflationCapFloor(trade_id, ccy, notional, isCap, isLong,
                             capFloorRate, start, term, yyFreq, yyDC, yyIndex,
                             observationLag, fixDays):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    days = 2
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    notionals = [notional]
    if isCap:
        caps = [capFloorRate]
        floors = []
    else:
        caps = []
        floors = [capFloorRate]

    qlStartDate = calendar.adjust(today + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    yySchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, yyFreq, cal, conv, conv, rule))
    yyLeg = ql.LegData(ql.YoYLegData(yyIndex, observationLag, fixDays),
        True, ccy, yySchedule, yyDC, notionals)

    env = ql.Envelope("CP")
    trade = ql.ORECapFloor(env, "Long" if isLong else "Short", yyLeg, caps, floors)
    trade.setId(trade_id)
    return trade;

buildYYInflationCapFloor("14_YoYInflationCap_UKRPI", "GBP", 100000.0, True,
                         True, 0.02, 0, 10, "1Y", "ACT/ACT", "UKRP1", "2M", 2)


def buildCommodityForward(trade_id, position, term, commodityName, currency,
                          strike, quantity):

    today = ql.Settings.instance().evaluationDate
    maturity = ql.to_string(today + ql.Period(term, ql.Years))

    env = ql.Envelope("CP")
    trade = ql.ORECommodityForward(env, position, commodityName, currency,
                                   quantity, maturity, strike)
    trade.setId(trade_id)
    return trade;

buildCommodityForward("17_CommodityForward_GOLD", "Long", 1, "COMDTY_GOLD_USD", "USD", 1170.0, 100)

def buildCommodityForward2(trade_id, ccy, commodityName, quantity, term, strike,
                          position, nettingSet, calendar):

    today = ql.Settings.instance().evaluationDate
    qlEndDate = calendar.adjust(today + ql.Period(term, ql.Years))
    endDate = ql.to_string(qlEndDate)

    env = ql.Envelope("CP", nettingSet)
    trade = ql.ORECommodityForward(env, position, commodityName, ccy, quantity, endDate, strike)
    trade.setId(trade_id)
    return trade;

buildCommodityForward2("18_Commodity_Forward", "USD", "COMDTY_WTI_USD", 3000, 15, 100, "Short", "NS", ql.TARGET())

def buildCommoditySwap(trade_id, ccy, isPayer, quantity, start, term, price,
                       fixedFreq, fixedDC, floatFreq, floatDC, index, calendar,
                       spotDays, spotStartLag, nettingSet, fixedPrice):

    today = ql.Settings.instance().evaluationDate
    cal = calendar.name()
    conv = "MF"
    rule = "Forward"

    priceType = ql.CommodityPriceType_Spot

    quantities = [quantity]
    quantityDates = []
    prices = [price]
    fixedPrices = [fixedPrice]
    priceDates = []

    commodityPayRelativeTo = ql.CommodityPayRelativeTo_CalculationPeriodEndDate

    spotStartLagTenor = ql.Period(spotDays, ql.Days) if spotStartLag else ql.Period(0, ql.Days)

    qlStartDate = calendar.adjust(today + spotStartLagTenor + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    fixedSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule))
    floatSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule))
    fixedLeg = ql.LegData(
        ql.CommodityFixedLegData(quantities, quantityDates, fixedPrices, priceDates, commodityPayRelativeTo),
        isPayer, ccy, fixedSchedule, fixedDC)
    floatingLeg = ql.LegData(
        ql.CommodityFloatingLegData(index, priceType, quantities, quantityDates),
        not isPayer, ccy, floatSchedule, floatDC) 
    legs = ql.LegDataVector()
    legs.push_back(fixedLeg)
    legs.push_back(floatingLeg)

    env = ql.Envelope("CP", nettingSet)
    trade = ql.ORECommoditySwap(env, legs)
    trade.setId(trade_id)
    return trade;

buildCommoditySwap("22_Commodity_Swap", "USD", False, 3000, 0, 15, 52.51,
                   "3M", "A360", "3M", "A360", "COMDTY_HOG_USD", ql.TARGET(), 2,
                   True, "NS", 1000)

def buildCommodityBasisSwap(trade_id, ccy, isPayer, quantity, start, term,
                            floatFreq, floatDC, index_1, index_2, calendar,
                            spotDays, spotStartLag, nettingSet):

    today = ql.Settings.instance().evaluationDate
    cal = calendar.name()
    conv = "MF"
    rule = "Forward"
    priceType = ql.CommodityPriceType_Spot

    quantities = [quantity]
    quantityDates = []
    spotStartLagTenor = ql.Period(spotDays, ql.Days) if spotStartLag else ql.Period(0, ql.Days)

    qlStartDate = calendar.adjust(today + spotStartLagTenor + ql.Period(start, ql.Years))
    qlEndDate = calendar.adjust(qlStartDate + ql.Period(term, ql.Years))
    startDate = ql.to_string(qlStartDate)
    endDate = ql.to_string(qlEndDate)

    floatSchedule = ql.ScheduleData(ql.ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule))
    floatingLeg_1 = ql.LegData(
        ql.CommodityFloatingLegData(index_1, priceType, quantities, quantityDates),
        isPayer, ccy, floatSchedule, floatDC) 
    floatingLeg_2 = ql.LegData(
        ql.CommodityFloatingLegData(index_2, priceType, quantities, quantityDates),
        not isPayer, ccy, floatSchedule, floatDC) 
    legs = ql.LegDataVector()
    legs.push_back(floatingLeg_1)
    legs.push_back(floatingLeg_2)

    env = ql.Envelope("CP", nettingSet)
    trade = ql.ORECommoditySwap(env, legs)
    trade.setId(trade_id)
    return trade;

buildCommodityBasisSwap( "15_Commodity_Swap", "USD", False, 3000, 0, 15, "3M",
                        "A360", "COMDTY_WTI_USD", "COMDTY_GOLD_USD",
                        ql.TARGET(), 2, True, "NS")

def buildCommodityOption(trade_id, longShort, putCall, term, commodityName,
                         currency, strike, quantity, premium = 0.0, premiumCcy
                         = "", premiumDate = ""):

    today = ql.Settings.instance().evaluationDate
    expiryDate = ql.to_string(today + ql.Period(term, ql.Years))

    option = ql.OptionData(longShort, putCall, "European", False, [expiryDate], "Cash", "",
        ql.PremiumData(premium, premiumCcy, ql.parseDate(premiumDate)) if premiumDate else ql.PremiumData())
    trStrike = ql.TradeStrike(ql.TradeStrike.Type_Price, strike)
    env = ql.Envelope("CP")
    trade = ql.ORECommodityOption(env, option, commodityName, currency, quantity, trStrike)

    trade.setId(trade_id)
    return trade;

buildCommodityOption("19_CommodityOption_GOLD", "Long", "Call", 1, "COMDTY_GOLD_USD", "USD", 1170.0, 100)

def buildFxBarrierOption(trade_id, longShort, putCall, expiry, boughtCcy,
                         boughtAmount, soldCcy, soldAmount, nettingSet,
                         barrierType, barrierLevel):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    qlExpiry = calendar.adjust(today + ql.Period(expiry, ql.Years))
    expiryDate = ql.to_string(qlExpiry)

    option = ql.OptionData(longShort, putCall, "European", False, [expiryDate], "Cash")
    rebate = 0.0
    tradeBarrier = ql.TradeBarrier(barrierLevel, "")
    tradeBarriers = ql.TradeBarrierVector()
    tradeBarriers.push_back(tradeBarrier)
    barrier = ql.BarrierData(barrierType, [barrierLevel], rebate, tradeBarriers)

    env = ql.Envelope("CP", nettingSet)
    trade = ql.FxBarrierOption(env, option, barrier, ql.Date(), "", boughtCcy,
                               boughtAmount, soldCcy, soldAmount)
    trade.setId(trade_id)
    return trade;

buildFxBarrierOption("27_FX_Barrier_Option", "Long", "Call", 10, "EUR", 1000,
                     "USD", 1200, "NS_2", "UpAndOut", 1.3)

def buildFxTouchOption(trade_id, longShort, expiry, boughtCcy, soldCcy,
                       payoffAmount, nettingSet, barrierType, barrierLevel):

    today = ql.Settings.instance().evaluationDate
    calendar = ql.TARGET()
    cal = "TARGET"
    conv = "MF"
    rule = "Forward"

    qlExpiry = calendar.adjust(today + ql.Period(expiry, ql.Years))
    expiryDate = ql.to_string(qlExpiry)

    option = ql.OptionData(longShort, "", "European", False, [expiryDate], "Cash")
    rebate = 0.0
    tradeBarrier = ql.TradeBarrier(barrierLevel, "")
    tradeBarriers = ql.TradeBarrierVector()
    tradeBarriers.push_back(tradeBarrier)
    barrier = ql.BarrierData(barrierType, [barrierLevel], rebate, tradeBarriers)
    env = ql.Envelope("CP", nettingSet)
    trade = ql.FxTouchOption(env, option, barrier, boughtCcy, soldCcy,
                             boughtCcy, payoffAmount)
    trade.setId(trade_id)
    return trade;

buildFxTouchOption("9_FXTouchOption", "Long", 10, "EUR", "USD", 1000, "NS",
                   "UpAndIn", 1.3)

