/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ored/portfolio/basketdata.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/commodityforward.hpp>
#include <ored/portfolio/builders/commodityoption.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/cdo.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/creditdefaultswapdata.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/indexcreditdefaultswapdata.hpp>
#include <ored/portfolio/indexcreditdefaultswapoption.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/time/calendars/all.hpp>
#include <test/testportfolio.hpp>
#include <ql/instruments/creditdefaultswap.hpp>

namespace testsuite {

QuantLib::ext::shared_ptr<Trade> buildSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term, Real rate,
                                   Real spread, string fixedFreq, string fixedDC, string floatFreq, string floatDC,
                                   string index, Calendar calendar, Natural spotDays, bool spotStartLag) {
    Date today = Settings::instance().evaluationDate();
    ostringstream o;
    o << calendar;
    string cal = o.str();
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> rates(1, rate);
    vector<Real> spreads(1, spread);

    Period spotStartLagTenor = spotStartLag ? spotDays * Days : 0 * Days;

    Date qlStartDate = calendar.adjust(today + spotStartLagTenor + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule));
    // fixed leg
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(rates), isPayer, ccy, fixedSchedule, fixedDC, notionals);
    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(index, spotDays, false, spreads), !isPayer, ccy,
                        floatSchedule, floatDC, notionals);
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, fixedLeg));
    trade->id() = id;

    return trade;

}

QuantLib::ext::shared_ptr<Trade> buildEuropeanSwaption(string id, string longShort, string ccy, bool isPayer, Real notional,
                                               int start, Size term, Real rate, Real spread, string fixedFreq,
                                               string fixedDC, string floatFreq, string floatDC, string index,
                                               string cashPhysical, Real premium, string premiumCcy,
                                               string premiumDate) {

    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    int days = 2;
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> rates(1, rate);
    vector<Real> spreads(1, spread);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule));
    // fixed leg
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(rates), isPayer, ccy, fixedSchedule, fixedDC, notionals);
    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(index, days, false, spreads), !isPayer, ccy, floatSchedule,
                        floatDC, notionals);
    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);
    // option data
    OptionData option(longShort, "Call", "European", false, vector<string>(1, startDate), cashPhysical, "",
                      premiumDate.empty() ? PremiumData() : PremiumData(premium, premiumCcy, parseDate(premiumDate)));
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swaption(env, option, legs));
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildBermudanSwaption(string id, string longShort, string ccy, bool isPayer, Real notional,
                                               Size exercises, int start, Size term, Real rate, Real spread,
                                               string fixedFreq, string fixedDC, string floatFreq, string floatDC,
                                               string index, string cashPhysical, Real premium, string premiumCcy,
                                               string premiumDate) {

    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    int days = 2;
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> rates(1, rate);
    vector<Real> spreads(1, spread);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    vector<string> exerciseDates;
    for (Size i = 0; i < exercises; ++i) {
        Date exerciseDate = qlStartDate + i * Years;
        exerciseDates.push_back(ore::data::to_string(exerciseDate));
    }

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule));
    // fixed leg
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(rates), isPayer, ccy, fixedSchedule, fixedDC, notionals);
    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(index, days, false, spreads), !isPayer, ccy, floatSchedule,
                        floatDC, notionals);
    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);
    // option data
    OptionData option(longShort, "Call", "Bermudan", false, exerciseDates, cashPhysical, "",
                      premiumDate.empty() ? PremiumData() : PremiumData(premium, premiumCcy, parseDate(premiumDate)));
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swaption(env, option, legs));
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildFxOption(string id, string longShort, string putCall, Size expiry, string boughtCcy,
                                       Real boughtAmount, string soldCcy, Real soldAmount, Real premium,
                                       string premiumCcy, string premiumDate) {
    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    Date qlExpiry = calendar.adjust(today + expiry * Years);
    string expiryDate = ore::data::to_string(qlExpiry);

    // envelope
    Envelope env("CP");
    // option data
    OptionData option(longShort, putCall, "European", false, vector<string>(1, expiryDate), "Cash", "",
                      premiumDate.empty() ? PremiumData() : PremiumData(premium, premiumCcy, parseDate(premiumDate)));
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::FxOption(env, option, boughtCcy, boughtAmount, soldCcy, soldAmount));
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildEquityOption(string id, string longShort, string putCall, Size expiry, string equityName,
                                           string currency, Real strike, Real quantity, Real premium, string premiumCcy,
                                           string premiumDate) {
    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    Date qlExpiry = calendar.adjust(today + expiry * Years);
    string expiryDate = ore::data::to_string(qlExpiry);

    TradeStrike tradeStrike(strike, currency);

    // envelope
    Envelope env("CP");
    // option data
    OptionData option(longShort, putCall, "European", false, vector<string>(1, expiryDate), "Cash", "",
                      premiumDate.empty() ? PremiumData() : PremiumData(premium, premiumCcy, parseDate(premiumDate)));
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(
        new ore::data::EquityOption(env, option, EquityUnderlying(equityName), currency, quantity, tradeStrike));
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildEquityForward(string id, string longShort, Size expiry, string equityName,
                                            string currency, Real strike, Real quantity) {
    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();

    Date qlExpiry = calendar.adjust(today + expiry * Years);
    string expiryDate = ore::data::to_string(qlExpiry);

    // envelope
    Envelope env("CP");
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::EquityForward(env, longShort, EquityUnderlying(equityName), currency,
                                                                quantity, expiryDate, strike));
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildCap(string id, string ccy, string longShort, Real capRate, Real notional, int start,
                                  Size term, string floatFreq, string floatDC, string index,
                                  Calendar calendar, Natural spotDays, bool spotStartLag) {
    return buildCapFloor(id, ccy, longShort, vector<Real>(1, capRate), vector<Real>(), notional, start, term, floatFreq,
                         floatDC, index, calendar, spotDays, spotStartLag);
}

QuantLib::ext::shared_ptr<Trade> buildFloor(string id, string ccy, string longShort, Real floorRate, Real notional, int start,
                                    Size term, string floatFreq, string floatDC, string index,
                                    Calendar calendar, Natural spotDays, bool spotStartLag) {
    return buildCapFloor(id, ccy, longShort, vector<Real>(), vector<Real>(1, floorRate), notional, start, term,
                         floatFreq, floatDC, index, calendar, spotDays, spotStartLag);
}

QuantLib::ext::shared_ptr<Trade> buildCapFloor(string id, string ccy, string longShort, vector<Real> capRates,
                                       vector<Real> floorRates, Real notional, int start, Size term, string floatFreq,
                                       string floatDC, string index, Calendar calendar, Natural spotDays,
                                       bool spotStartLag) {
    Date today = Settings::instance().evaluationDate();
    // Calendar calendar = TARGET();
    // Size days = 2;
    // string cal = "TARGET";
    ostringstream o;
    o << calendar;
    string cal = o.str();
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> spreads(1, 0.0);

    Period spotStartLagTenor = spotStartLag ? spotDays * Days : 0 * Days;

    Date qlStartDate = calendar.adjust(today + spotStartLagTenor + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(index, spotDays, false, spreads), false, ccy, floatSchedule,
                        floatDC, notionals);
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::CapFloor(env, longShort, floatingLeg, capRates, floorRates));
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildCrossCcyBasisSwap(
    string id, string recCcy, Real recNotional, string payCcy, Real payNotional, int start, Size term,
    Real recLegSpread, Real payLegSpread, string recFreq, string recDC, string recIndex, Calendar recCalendar,
    string payFreq, string payDC, string payIndex, Calendar payCalendar, Natural spotDays, bool spotStartLag,
    bool notionalInitialExchange, bool notionalFinalExchange, bool notionalAmortizingExchange,
    bool isRecLegFXResettable, bool isPayLegFXResettable) {
    Date today = Settings::instance().evaluationDate();

    string payCal = to_string(payCalendar);
    string recCal = to_string(recCalendar);
    string conv = "MF";
    string rule = "Forward";

    vector<Real> recNotionals(1, recNotional);
    vector<Real> recSpreads(1, recLegSpread);
    vector<Real> payNotionals(1, payNotional);
    vector<Real> paySpreads(1, payLegSpread);

    Period spotStartLagTenor = spotStartLag ? spotDays * Days : 0 * Days;

    Date qlStartDate = recCalendar.adjust(today + spotStartLagTenor + start * Years);
    Date qlEndDate = recCalendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData recSchedule(ScheduleRules(startDate, endDate, recFreq, recCal, conv, conv, rule));
    ScheduleData paySchedule(ScheduleRules(startDate, endDate, payFreq, payCal, conv, conv, rule));
    // rec float leg
    auto recFloatingLegData = QuantLib::ext::make_shared<FloatingLegData>(recIndex, spotDays, false, recSpreads);
    LegData recFloatingLeg;
    if (isRecLegFXResettable) {
        string fxIndex = "FX-ECB-" + recCcy + "-" + payCcy;
        recFloatingLeg = LegData(recFloatingLegData, false, recCcy, recSchedule, recDC, recNotionals, vector<string>(),
                                 conv, notionalInitialExchange, notionalFinalExchange, notionalAmortizingExchange,
                                 isRecLegFXResettable, payCcy, payNotional, fxIndex);
    } else {
        recFloatingLeg = LegData(recFloatingLegData, false, recCcy, recSchedule, recDC, recNotionals, vector<string>(),
                                 conv, notionalInitialExchange, notionalFinalExchange, notionalAmortizingExchange);
    }
    // pay float leg
    auto payFloatingLegData = QuantLib::ext::make_shared<FloatingLegData>(payIndex, spotDays, false, recSpreads);
    LegData payFloatingLeg;
    if (isPayLegFXResettable) {
        string fxIndex = "FX-ECB-" + payCcy + "-" + recCcy;
        payFloatingLeg = LegData(payFloatingLegData, true, payCcy, paySchedule, payDC, payNotionals, vector<string>(),
                                 conv, notionalInitialExchange, notionalFinalExchange, notionalAmortizingExchange,
                                 !isPayLegFXResettable, recCcy, recNotional, fxIndex);
    } else {
        payFloatingLeg = LegData(payFloatingLegData, true, payCcy, paySchedule, payDC, payNotionals, vector<string>(),
                                 conv, notionalInitialExchange, notionalFinalExchange, notionalAmortizingExchange);
    }
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swap(env, recFloatingLeg, payFloatingLeg));
    trade->id() = id;

    return trade;
}
    
QuantLib::ext::shared_ptr<Trade> buildZeroBond(string id, string ccy, Real notional, Size term, string suffix) {
    Date today = Settings::instance().evaluationDate();
    Date qlEndDate = today + term * Years;
    string maturityDate = ore::data::to_string(qlEndDate);
    string issueDate = ore::data::to_string(today);

    string settlementDays = "2";
    string calendar = "TARGET";
    string issuerId = "BondIssuer" + suffix;
    string creditCurveId = "BondIssuer" + suffix;
    string securityId = "Bond" + suffix;
    string referenceCurveId = "BondCurve" + suffix;
    // envelope
    Envelope env("CP");
    QuantLib::ext::shared_ptr<Trade> trade(
        new ore::data::Bond(env, BondData(issuerId, creditCurveId, securityId, referenceCurveId, settlementDays,
                                          calendar, notional, maturityDate, ccy, issueDate)));
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildCreditDefaultSwap(string id, string ccy, string issuerId,
                                                string creditCurveId, bool isPayer, Real notional,
                                                int start, Size term, Real rate, Real spread,
                                                string fixedFreq, string fixedDC) {
    Date today = Settings::instance().evaluationDate();

    string settlementDays = "1";
    Calendar calendar = WeekendsOnly();
    string cal = "WeekendsOnly";
    string conv = "F";
    string convEnd = "U";
    string rule = "CDS2015";

    vector<Real> notionals(1, notional);
    vector<Real> rates(1, rate);
    vector<Real> spreads(1, spread);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // envelope
    Envelope env("CP");
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, fixedFreq, cal, conv, convEnd, rule));
    // fixed leg
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(spreads), isPayer, ccy, fixedSchedule, fixedDC, notionals);

    ore::data::CreditDefaultSwapData swap(issuerId, creditCurveId, fixedLeg, true,
                                          QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault, today + 1);
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::CreditDefaultSwap(env, swap));

    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildSyntheticCDO(string id, string name, vector<string> names,
                                           string longShort, string ccy, vector<string> ccys,
                                           bool isPayer, vector<Real> notionals, Real notional,
                                           int start, Size term, Real rate, Real spread,
                                           string fixedFreq, string fixedDC) {
    Date today = Settings::instance().evaluationDate();
    Calendar calendar = WeekendsOnly();
    string cal = "WeekendsOnly";
    string conv = "F";
    string rule = "CDS2015";
    string issuerId = name;
    string creditCurveId = name;
    string qualifier = "Tranch1";

    Real attachmentPoint = 0.0;
    Real detachmentPoint = 0.1;
    bool settlesAccrual = true;
    QuantExt::CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
        QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault;

    Real upfrontFee = 0.0;

    vector<Real> notionalTotal(names.size(), notional);
    vector<Real> rates(names.size(), rate);
    vector<Real> spreads(names.size(), spread);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlProtectionStartDate = calendar.advance(qlStartDate, 1 * Days);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    Date qlUpfrontDate = calendar.advance(qlStartDate, 3 * Days);
    string startDate = ore::data::to_string(qlStartDate);
    string protectionStart = ore::data::to_string(qlProtectionStartDate);
    string endDate = ore::data::to_string(qlEndDate);
    string upfrontDate = ore::data::to_string(qlUpfrontDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule));
    // fixed leg
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(rates), isPayer, ccy, fixedSchedule, fixedDC, notionalTotal);
    // basket data
    using ore::data::BasketConstituent;
    vector<BasketConstituent> constituents;
    for (Size i = 0; i < names.size(); ++i) {
        constituents.emplace_back(names[i], names[i], notionals[i], ccys[i], qualifier);
    }
    ore::data::BasketData basket(constituents);
    // swap data
    ore::data::IndexCreditDefaultSwapData swap(creditCurveId, basket, fixedLeg);

    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::SyntheticCDO(
        env, fixedLeg, qualifier, basket, attachmentPoint, detachmentPoint, settlesAccrual, protectionPaymentTime,
        protectionStart, upfrontDate, upfrontFee));
    if (trade == NULL)
        std::cout << "failed to build" << std::endl;

    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildCmsCapFloor(string id, string ccy, string indexId, bool isPayer,
                                          Real notional, int start, Size term, Real capRate,
                                          Real floorRate, Real spread, string freq, string dc) {
    Date today = Settings::instance().evaluationDate();

    string settlementDays = "2";
    Calendar calendar = TARGET();
    string cal = "TARGET";
    string qualifier = "";
    string conv = "MF";
    string rule = "Forward";

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    string longShort = "Long";

    vector<Real> notionals(1, notional);
    vector<Real> caps(1, capRate);
    vector<Real> floors(1, floorRate);
    vector<Real> spreads(1, spread);
    bool isInArrears = false;

    // envelope
    Envelope env("CP");
    ScheduleData schedule(ScheduleRules(startDate, endDate, freq, cal, conv, conv, rule));
    // fixed leg

    LegData cmsLeg(QuantLib::ext::make_shared<CMSLegData>(indexId, 0, isInArrears, spreads, vector<string>(1, startDate)),
                   isPayer, ccy, schedule, dc, notionals, vector<string>(1, startDate));

    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::CapFloor(env, longShort, cmsLeg, vector<double>(), floors));

    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildCPIInflationSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term,
                                               Real spread, string floatFreq, string floatDC, string index,
                                               string cpiFreq, string cpiDC, string cpiIndex, Real baseRate,
                                               string observationLag, bool interpolated, Real cpiRate) {

    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    int days = 2;
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> cpiRates(1, cpiRate);
    vector<Real> spreads(1, spread);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    ScheduleData cpiSchedule(ScheduleRules(startDate, endDate, cpiFreq, cal, conv, conv, rule));
    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(index, days, false, spreads), !isPayer, ccy, floatSchedule,
                        floatDC, notionals);
    // fixed leg
    LegData cpiLeg(QuantLib::ext::make_shared<CPILegData>(cpiIndex, startDate, baseRate, observationLag,
                                                  (interpolated ? "Linear" : "Flat"), cpiRates),
                   isPayer, ccy, cpiSchedule, cpiDC, notionals, vector<string>(), "F", false, true);

    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, cpiLeg));
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildYYInflationSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term,
                                              Real spread, string floatFreq, string floatDC, string index,
                                              string yyFreq, string yyDC, string yyIndex, string observationLag,
                                              Size fixDays) {

    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    int days = 2;
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> spreads(1, spread);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    ScheduleData yySchedule(ScheduleRules(startDate, endDate, yyFreq, cal, conv, conv, rule));
    // float leg
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(index, days, false, spreads), !isPayer, ccy, floatSchedule,
                        floatDC, notionals);
    // fixed leg
    LegData yyLeg(QuantLib::ext::make_shared<YoYLegData>(yyIndex, observationLag, fixDays), isPayer, ccy, yySchedule, yyDC,
                  notionals);

    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, yyLeg));
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildYYInflationCapFloor(string id, string ccy, Real notional, bool isCap,
                                                  bool isLong, Real capFloorRate, int start,
                                                  Size term, string yyFreq, string yyDC,
                                                  string yyIndex, string observationLag,
                                                  Size fixDays) {

    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> caps, floors;
    if (isCap)
        caps.resize(1, capFloorRate);
    else
        floors.resize(1, capFloorRate);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    ScheduleData yySchedule(ScheduleRules(startDate, endDate, yyFreq, cal, conv, conv, rule));

    LegData yyLeg(QuantLib::ext::make_shared<YoYLegData>(yyIndex, observationLag, fixDays), true, ccy, yySchedule, yyDC,
                  notionals);

    Envelope env("CP");

    auto trade = QuantLib::ext::make_shared<ore::data::CapFloor>(env, isLong ? "Long" : "Short", yyLeg, caps, floors);
    trade->id() = id;
    return trade;
}
    
QuantLib::ext::shared_ptr<Trade> buildCommodityForward(const std::string& id, const std::string& position, Size term,
                                               const std::string& commodityName, const std::string& currency,
                                               Real strike, Real quantity) {

    Date today = Settings::instance().evaluationDate();
    string maturity = ore::data::to_string(today + term * Years);

    Envelope env("CP");
    QuantLib::ext::shared_ptr<Trade> trade = QuantLib::ext::make_shared<ore::data::CommodityForward>(
        env, position, commodityName, currency, quantity, maturity, strike);
    trade->id() = id;

    return trade;
}

QuantLib::ext::shared_ptr<Trade> buildCommodityOption(const string& id, const string& longShort, const string& putCall,
                                              Size term, const string& commodityName, const string& currency,
                                              Real strike, Real quantity, Real premium, const string& premiumCcy,
                                              const string& premiumDate) {

    Date today = Settings::instance().evaluationDate();
    vector<string> expiryDate{ore::data::to_string(today + term * Years)};

    Envelope env("CP");
    OptionData option(longShort, putCall, "European", false, expiryDate, "Cash", "",
                      premiumDate.empty() ? PremiumData() : PremiumData(premium, premiumCcy, parseDate(premiumDate)));
    TradeStrike trStrike(TradeStrike::Type::Price, strike);
    QuantLib::ext::shared_ptr<Trade> trade =
        QuantLib::ext::make_shared<ore::data::CommodityOption>(env, option, commodityName, currency, quantity, trStrike);
    trade->id() = id;

    return trade;
}

} // namespace testsuite
