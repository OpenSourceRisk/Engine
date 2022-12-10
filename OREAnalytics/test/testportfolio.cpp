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

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/commodityforward.hpp>
#include <ored/portfolio/builders/commodityoption.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/time/calendars/all.hpp>
#include <test/testportfolio.hpp>

namespace testsuite {

boost::shared_ptr<Trade> buildSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term, Real rate,
                                   Real spread, string fixedFreq, string fixedDC, string floatFreq, string floatDC,
                                   string index) {
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
    LegData fixedLeg(boost::make_shared<FixedLegData>(rates), isPayer, ccy, fixedSchedule, fixedDC, notionals);
    // float leg
    LegData floatingLeg(boost::make_shared<FloatingLegData>(index, days, false, spreads), !isPayer, ccy, floatSchedule,
                        floatDC, notionals);
    // trade
    boost::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, fixedLeg));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildEuropeanSwaption(string id, string longShort, string ccy, bool isPayer, Real notional,
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
    LegData fixedLeg(boost::make_shared<FixedLegData>(rates), isPayer, ccy, fixedSchedule, fixedDC, notionals);
    // float leg
    LegData floatingLeg(boost::make_shared<FloatingLegData>(index, days, false, spreads), !isPayer, ccy, floatSchedule,
                        floatDC, notionals);
    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);
    // option data
    OptionData option(longShort, "Call", "European", false, vector<string>(1, startDate), cashPhysical, "",
                      premiumDate.empty() ? PremiumData() : PremiumData(premium, premiumCcy, parseDate(premiumDate)));
    // trade
    boost::shared_ptr<Trade> trade(new ore::data::Swaption(env, option, legs));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildBermudanSwaption(string id, string longShort, string ccy, bool isPayer, Real notional,
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
    LegData fixedLeg(boost::make_shared<FixedLegData>(rates), isPayer, ccy, fixedSchedule, fixedDC, notionals);
    // float leg
    LegData floatingLeg(boost::make_shared<FloatingLegData>(index, days, false, spreads), !isPayer, ccy, floatSchedule,
                        floatDC, notionals);
    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);
    // option data
    OptionData option(longShort, "Call", "Bermudan", false, exerciseDates, cashPhysical, "",
                      premiumDate.empty() ? PremiumData() : PremiumData(premium, premiumCcy, parseDate(premiumDate)));
    // trade
    boost::shared_ptr<Trade> trade(new ore::data::Swaption(env, option, legs));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildFxOption(string id, string longShort, string putCall, Size expiry, string boughtCcy,
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
    boost::shared_ptr<Trade> trade(new ore::data::FxOption(env, option, boughtCcy, boughtAmount, soldCcy, soldAmount));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildEquityOption(string id, string longShort, string putCall, Size expiry, string equityName,
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
    boost::shared_ptr<Trade> trade(
        new ore::data::EquityOption(env, option, EquityUnderlying(equityName), currency, quantity, tradeStrike));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildEquityForward(string id, string longShort, Size expiry, string equityName,
                                            string currency, Real strike, Real quantity) {
    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();

    Date qlExpiry = calendar.adjust(today + expiry * Years);
    string expiryDate = ore::data::to_string(qlExpiry);

    // envelope
    Envelope env("CP");
    // trade
    boost::shared_ptr<Trade> trade(new ore::data::EquityForward(env, longShort, EquityUnderlying(equityName), currency,
                                                                quantity, expiryDate, strike));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildCap(string id, string ccy, string longShort, Real capRate, Real notional, int start,
                                  Size term, string floatFreq, string floatDC, string index) {
    return buildCapFloor(id, ccy, longShort, vector<Real>(1, capRate), vector<Real>(), notional, start, term, floatFreq,
                         floatDC, index);
}

boost::shared_ptr<Trade> buildFloor(string id, string ccy, string longShort, Real floorRate, Real notional, int start,
                                    Size term, string floatFreq, string floatDC, string index) {
    return buildCapFloor(id, ccy, longShort, vector<Real>(), vector<Real>(1, floorRate), notional, start, term,
                         floatFreq, floatDC, index);
}

boost::shared_ptr<Trade> buildCapFloor(string id, string ccy, string longShort, vector<Real> capRates,
                                       vector<Real> floorRates, Real notional, int start, Size term, string floatFreq,
                                       string floatDC, string index) {
    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    int days = 2;
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> spreads(1, 0.0);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate = ore::data::to_string(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    // float leg
    FloatingLegData floatingLegData;
    LegData floatingLeg(boost::make_shared<FloatingLegData>(index, days, false, spreads), false, ccy, floatSchedule,
                        floatDC, notionals);
    // trade
    boost::shared_ptr<Trade> trade(new ore::data::CapFloor(env, longShort, floatingLeg, capRates, floorRates));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildZeroBond(string id, string ccy, Real notional, Size term) {
    Date today = Settings::instance().evaluationDate();
    Date qlEndDate = today + term * Years;
    string maturityDate = ore::data::to_string(qlEndDate);
    string issueDate = ore::data::to_string(today);

    string settlementDays = "2";
    string calendar = "TARGET";
    string issuerId = "BondIssuer1";
    string creditCurveId = "BondIssuer1";
    string securityId = "Bond1";
    string referenceCurveId = "BondCurve1";
    // envelope
    Envelope env("CP");
    boost::shared_ptr<Trade> trade(
        new ore::data::Bond(env, BondData(issuerId, creditCurveId, securityId, referenceCurveId, settlementDays,
                                          calendar, notional, maturityDate, ccy, issueDate)));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildCPIInflationSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term,
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
    LegData floatingLeg(boost::make_shared<FloatingLegData>(index, days, false, spreads), !isPayer, ccy, floatSchedule,
                        floatDC, notionals);
    // fixed leg
    LegData cpiLeg(boost::make_shared<CPILegData>(cpiIndex, startDate, baseRate, observationLag,
                                                  (interpolated ? "Linear" : "Flat"), cpiRates),
                   isPayer, ccy, cpiSchedule, cpiDC, notionals, vector<string>(), "F", false, true);

    // trade
    boost::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, cpiLeg));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildYYInflationSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term,
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
    LegData floatingLeg(boost::make_shared<FloatingLegData>(index, days, false, spreads), !isPayer, ccy, floatSchedule,
                        floatDC, notionals);
    // fixed leg
    LegData yyLeg(boost::make_shared<YoYLegData>(yyIndex, observationLag, fixDays), isPayer, ccy, yySchedule, yyDC,
                  notionals);

    // trade
    boost::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, yyLeg));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildCommodityForward(const std::string& id, const std::string& position, Size term,
                                               const std::string& commodityName, const std::string& currency,
                                               Real strike, Real quantity) {

    Date today = Settings::instance().evaluationDate();
    string maturity = ore::data::to_string(today + term * Years);

    Envelope env("CP");
    boost::shared_ptr<Trade> trade = boost::make_shared<ore::data::CommodityForward>(
        env, position, commodityName, currency, quantity, maturity, strike);
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildCommodityOption(const string& id, const string& longShort, const string& putCall,
                                              Size term, const string& commodityName, const string& currency,
                                              Real strike, Real quantity, Real premium, const string& premiumCcy,
                                              const string& premiumDate) {

    Date today = Settings::instance().evaluationDate();
    vector<string> expiryDate{ore::data::to_string(today + term * Years)};

    Envelope env("CP");
    OptionData option(longShort, putCall, "European", false, expiryDate, "Cash", "",
                      premiumDate.empty() ? PremiumData() : PremiumData(premium, premiumCcy, parseDate(premiumDate)));
    TradeStrike trStrike(TradeStrike::Type::Price, strike);
    boost::shared_ptr<Trade> trade =
        boost::make_shared<ore::data::CommodityOption>(env, option, commodityName, currency, quantity, trStrike);
    trade->id() = id;

    return trade;
}

} // namespace testsuite
