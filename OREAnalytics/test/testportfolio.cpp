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

#include <test/testportfolio.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ql/time/calendars/all.hpp>

namespace testsuite {

string toString(Date d) {
    ostringstream o;
    o << io::iso_date(d);
    return o.str();
}

boost::shared_ptr<Trade> buildSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term, Real rate,
                                   Real spread, string fixedFreq, string fixedDC, string floatFreq, string floatDC,
                                   string index) {
    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    Size days = 2;
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> rates(1, rate);
    vector<Real> spreads(1, spread);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = toString(qlStartDate);
    string endDate = toString(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule));
    // fixed leg
    FixedLegData fixedLegData(rates);
    LegData fixedLeg(isPayer, ccy, fixedLegData, fixedSchedule, fixedDC, notionals);
    // float leg
    FloatingLegData floatingLegData(index, days, false, spreads);
    LegData floatingLeg(!isPayer, ccy, floatingLegData, floatSchedule, floatDC, notionals);
    // trade
    boost::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, fixedLeg));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildEuropeanSwaption(string id, string longShort, string ccy, bool isPayer, Real notional,
                                               int start, Size term, Real rate, Real spread, string fixedFreq,
                                               string fixedDC, string floatFreq, string floatDC, string index,
                                               string cashPhysical) {

    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    Size days = 2;
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> rates(1, rate);
    vector<Real> spreads(1, spread);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = toString(qlStartDate);
    string endDate = toString(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, fixedFreq, cal, conv, conv, rule));
    // fixed leg
    FixedLegData fixedLegData(rates);
    LegData fixedLeg(isPayer, ccy, fixedLegData, fixedSchedule, fixedDC, notionals);
    // float leg
    FloatingLegData floatingLegData(index, days, false, spreads);
    LegData floatingLeg(!isPayer, ccy, floatingLegData, floatSchedule, floatDC, notionals);
    // leg vector
    vector<LegData> legs;
    legs.push_back(fixedLeg);
    legs.push_back(floatingLeg);
    // option data
    OptionData option(longShort, "Call", "European", false, vector<string>(1, startDate), cashPhysical);
    // trade
    boost::shared_ptr<Trade> trade(new ore::data::Swaption(env, option, legs));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildFxOption(string id, string longShort, string putCall, Size expiry, string boughtCcy,
                                       Real boughtAmount, string soldCcy, Real soldAmount) {
    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    Date qlExpiry = calendar.adjust(today + expiry * Years);
    string expiryDate = toString(qlExpiry);

    // envelope
    Envelope env("CP");
    // option data
    OptionData option(longShort, putCall, "European", false, vector<string>(1, expiryDate), "Cash");
    // trade
    boost::shared_ptr<Trade> trade(new ore::data::FxOption(env, option, boughtCcy, boughtAmount, soldCcy, soldAmount));
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
    Size days = 2;
    string cal = "TARGET";
    string conv = "MF";
    string rule = "Forward";

    vector<Real> notionals(1, notional);
    vector<Real> spreads(1, 0.0);

    Date qlStartDate = calendar.adjust(today + start * Years);
    Date qlEndDate = calendar.adjust(qlStartDate + term * Years);
    string startDate = toString(qlStartDate);
    string endDate = toString(qlEndDate);

    // envelope
    Envelope env("CP");
    // schedules
    ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, rule));
    // float leg
    FloatingLegData floatingLegData(index, days, false, spreads);
    LegData floatingLeg(false, ccy, floatingLegData, floatSchedule, floatDC, notionals);
    // trade
    boost::shared_ptr<Trade> trade(new ore::data::CapFloor(env, longShort, floatingLeg, capRates, floorRates));
    trade->id() = id;

    return trade;
}

boost::shared_ptr<Trade> buildZeroBond(string id, string ccy, Real notional, Size term) {
    Date today = Settings::instance().evaluationDate();
    Date qlEndDate = today + term * Years;
    string maturityDate = toString(qlEndDate);
    string issueDate = toString(today);

    string settlementDays = "2";
    string calendar = "TARGET";
    string issuerId = "BondIssuer1";
    string securityId = "Bond1";
    string referenceCurveId = "BondCurve1";
    // envelope
    Envelope env("CP");
    boost::shared_ptr<Trade> trade(new ore::data::Bond(env, issuerId, securityId, referenceCurveId, settlementDays,
                                                       calendar, notional, maturityDate, ccy, issueDate));
    trade->id() = id;

    return trade;
}
}
