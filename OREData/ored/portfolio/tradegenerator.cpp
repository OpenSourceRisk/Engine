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

//#include <ored/portfolio/builders/swap/hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/tradegenerator.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>
#include "optiondata.hpp"

using ore::data::XMLUtils;

namespace ore {
namespace data {

using namespace ore::data;
using std::map;
using std::string;


TradeGenerator::TradeGenerator() { 
    today_ = Settings::instance().evaluationDate(); 
    auto convs = InstrumentConventions::instance().conventions();
    //conventions_;
}

void TradeGenerator::addConventions(std::string conventionsFile) {
    
    conventions_.fromFile(conventionsFile);
    return;
}


void TradeGenerator::addCurveConfigs(std::string curveConfigFile) {

    curveConfigs_.fromFile(curveConfigFile);
    return;
}

/* QuantLib::ext::shared_ptr<Trade> buildOisSwap(string ccy, Real notional, Date maturityDate) { return NULL;

}*/

void TradeGenerator::buildSwap(string conventionId, Real notional, Date maturity, Real rate,
                               std::map<std::string, std::string> mapPairs) {
    auto conv = conventions_.get(conventionId);
    
    string cal;
    string rule;
    string floatFreq;
    string startDate = to_string(today_);
    string endDate = to_string(maturity);
    string fixedDC;
    string fixedFreq;
    string convention;
    LegData floatingLeg;
    string type;
    
    switch (conv->type()) {
    
    case Convention::Type::OIS: {
        floatingLeg = buildOisLeg(conventionId, notional, maturity, mapPairs);
        fixedFreq = to_string(QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv)->fixedFrequency());
        fixedDC = to_string(QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv)->fixedDayCounter());
        type = "OIS";
        break;
    }
     case Convention::Type::Swap:
        floatingLeg = buildIborLeg(conventionId, notional, maturity, mapPairs);
         fixedFreq = to_string(QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv)->fixedFrequency());
         fixedDC = to_string(QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv)->fixedDayCounter());
         type = "IBOR";
        break;


        //buildOisSwap(conventionId, notional, maturity, rate, QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv));
    }
     cal = floatingLeg.schedule().rules().front().calendar();
     rule = floatingLeg.schedule().rules().front().rule();
     ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, fixedFreq, cal, convention, convention, rule));
     bool isPayer = true;
     vector<Real> rates(1, rate);
     string ccy = floatingLeg.currency();
     LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(rates), isPayer, ccy, fixedSchedule, fixedDC, floatingLeg.notionals());

     Envelope env = Envelope("CP");
     QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, fixedLeg));
     trade->id() =  to_string(size() + 1) + "_" + ccy + "_" + type + "_SWAP";
     add(trade);
    return;
}


void TradeGenerator::buildFxOption(string payCcy, Real payNotional, string recCcy, Real recNotional, Date expiryDate,
                                   std::map<std::string, std::string> mapPairs) {
    Envelope env = Envelope("CP");

    string longShort = mapPairs.count("longShort") == 1 ? mapPairs["longShort"] : "Long";
    string putCall = mapPairs.count("putCall") == 1 ? mapPairs["putCall"] : "Put";
    OptionData option(longShort, putCall, "European", false, vector<string>(1, to_string(expiryDate)), "Cash", "");
    QuantLib::ext::shared_ptr<Trade> trade(
        new ore::data::FxOption(env, option, recCcy, recNotional, payCcy, payNotional));
    trade->id() = to_string(size() + 1) + "_" + payCcy + "-" + recCcy + "_FX_OPTION";
    add(trade);
    return;
}

void TradeGenerator::buildEquityOption(string equityCurveId, Real quantity, Date maturity, Real strike,
                                       std::map<std::string, std::string> mapPairs) {
    auto config = curveConfigs_.equityCurveConfig(equityCurveId);
    string currency = config->currency();
    
    Calendar calendar = TARGET();
    string cal = mapPairs.count("calendar") == 1 ? mapPairs["calendar"] : config->calendar();
    string conv = mapPairs.count("convention") == 1 ? mapPairs["convention"] : config->dayCountID();
    string rule = mapPairs.count("rule") == 1 ? mapPairs["rule"] : "Forward";
    string expiryDate = ore::data::to_string(maturity);
    string longShort = mapPairs.count("longShort") == 1 ? mapPairs["longShort"] : "Long";
    string putCall = mapPairs.count("putCall") == 1 ? mapPairs["putCall"] : "Call";
    TradeStrike tradeStrike(strike, currency);
    Envelope env("CP");
    OptionData option(longShort, putCall, "European", false, vector<string>(1, expiryDate), "Cash", "",
                     PremiumData());
    
    QuantLib::ext::shared_ptr<Trade> trade(
        new ore::data::EquityOption(env, option, EquityUnderlying(equityCurveId), currency, quantity, tradeStrike));
    trade->id() = to_string(size() + 1) + "_" + equityCurveId + "_EQ_OPTION";
    add(trade);
    return ;
}

void TradeGenerator::buildEquityForward(string equityCurveId, Real quantity, Date maturity, Real strike,
                                        std::map<std::string, std::string> mapPairs) {
    auto config = curveConfigs_.equityCurveConfig(equityCurveId);
    string currency = config->currency();
    string longShort = mapPairs.count("longShort") == 1 ? mapPairs["longShort"] : "Long";
    string expiryDate = ore::data::to_string(maturity);

    // envelope
    Envelope env("CP");
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::EquityForward(env, longShort, EquityUnderlying(equityCurveId),
                                                                        currency, quantity, expiryDate, strike));
    trade->id() = to_string(size() + 1) + "_" + equityCurveId + "_EQ_FORWARD";
    add(trade);
    return;
}

void TradeGenerator::buildCapFloor(string indexName, Real capFloorRate, Real notional, Date maturity, string capFloor,
                                   std::map<std::string, std::string> mapPairs) {
    QuantLib::ext::shared_ptr<IborIndex> iborIndex;
    tryParseIborIndex(indexName, iborIndex);
    LegData floatingLeg;
    switch (conventions_.get(indexName)->type())
    { case Convention::Type::OIS:
    {
        floatingLeg = buildOisLeg(indexName, notional, maturity, mapPairs);
        break;
    }
    case Convention::Type::Swap:
        floatingLeg = buildIborLeg(indexName, notional, maturity, mapPairs);
        break;
    }

    vector<double> capRates(0, 0);
    vector<double> floorRates(0, 0);
    string longShort = mapPairs.count("longShort") == 1 ? mapPairs["longShort"] : "Long";
    if (capFloor == "Cap")
    {
        capRates.push_back(capFloorRate);
    } else {
        floorRates.push_back(capFloorRate);
    }

    Envelope env("CP");

    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::CapFloor(env, longShort, floatingLeg, capRates, floorRates));
    trade->id() = to_string(size() + 1) + "_" + floatingLeg.currency() + "_CAPFLOOR";
    add(trade);
    return;
    }

void TradeGenerator::buildEquitySwap(string equityCurveId, string returnType, Real quantity, string conventionId,
                                         Real notional, Date maturity, std::map<std::string, std::string> mapPairs) {
        auto convention = conventions_.get(conventionId);
    string indexName;
        LegData floatingLeg;
        switch (convention->type()) {
        case Convention::Type::OIS:
            floatingLeg = buildOisLeg(conventionId, notional, maturity, mapPairs);
            break;

        case Convention::Type::Swap:
            floatingLeg = buildIborLeg(conventionId, notional, maturity, mapPairs);
            break;

            // buildOisSwap(conventionId, notional, maturity, rate,
            // QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv));
        }
        QuantLib::ext::shared_ptr<IborIndex> iborIndex;
        Real dividendFactor = 1;
        tryParseIborIndex(indexName, iborIndex);
        string ccy = floatingLeg.currency();
        string floatFreq = floatingLeg.schedule().rules().front().tenor();
        string startDate = to_string(today_);
        string endDate = to_string(maturity);
        string floatDC = floatingLeg.dayCounter();
        string conv = floatingLeg.schedule().rules().front().convention();
        string cal = floatingLeg.schedule().rules().front().calendar();
        string rule = floatingLeg.schedule().rules().front().rule();
        Natural spotDays = 2;
        vector<double> notionals = floatingLeg.notionals();
        
        auto config = curveConfigs_.equityCurveConfig(equityCurveId);
        
        ScheduleData equitySchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, ""));

        LegData equityLeg(QuantLib::ext::make_shared<EquityLegData>(
            QuantExt::parseEquityReturnType(returnType), dividendFactor, EquityUnderlying(equityCurveId), 0, false, spotDays),
                          false, ccy, equitySchedule, floatingLeg.dayCounter(), notionals);

        Envelope env = Envelope("CP");
        QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, equityLeg));
        trade->id() = to_string(size() + 1) + "_" + equityCurveId + "_" + indexName + "_EQ_SWAP";
        add(trade);
        return;
    }

void TradeGenerator::buildEquitySwap(string equityCurveId, string returnType, Real quantity, Real rate,
                                         Real notional, Date maturity, std::map<std::string, std::string> mapPairs) {
        auto config = curveConfigs_.equityCurveConfig(equityCurveId);
    string indexName = config->forecastingCurve();
        
        
        QuantLib::ext::shared_ptr<IborIndex> iborIndex;
        Real dividendFactor = 1;
        tryParseIborIndex(indexName, iborIndex);
        string ccy = to_string(iborIndex->currency());
        string floatFreq = to_string(iborIndex->tenor());
        string startDate = to_string(today_);
        string endDate = to_string(maturity);
        string floatDC = to_string(iborIndex->dayCounter());
        string conv = to_string(iborIndex->businessDayConvention());
        string cal = to_string(iborIndex->fixingCalendar());
        string rule = "";
        Natural spotDays = (iborIndex->fixingDays());
        vector<double> notionals(1, notional);
        vector<double> spreads(0, 0);
        vector<Real> rates(1, rate);
        bool isPayer = false;
        ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, ""));
        ScheduleData equitySchedule(ScheduleRules(startDate, endDate, floatFreq, cal, conv, conv, ""));

        LegData equityLeg(QuantLib::ext::make_shared<EquityLegData>(QuantExt::parseEquityReturnType(returnType),
                                                                    dividendFactor, EquityUnderlying(equityCurveId), 0,
                                                                    isPayer, spotDays),
                          false, ccy, equitySchedule, floatDC, notionals);

         LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(rates), isPayer, "USD", fixedSchedule, floatDC,
                         notionals);
        Envelope env = Envelope("CP");
        QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swap(env, fixedLeg, equityLeg));
        trade->id() = to_string(size() + 1) + "_" + equityCurveId + "_FIXED_EQ_SWAP";
        add(trade);
        return;
    }




LegData TradeGenerator::buildOisLeg(string conventionId, Real notional, Date maturity,
    std::map<std::string, std::string> mapPairs)
{
        auto conv = conventions_.get(conventionId);
        auto oisConv = QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv);
        string indexName = to_string(oisConv->indexName());
        QuantLib::ext::shared_ptr<IborIndex> oisIndex;
        tryParseIborIndex(indexName, oisIndex);
        string cal = to_string(oisConv->fixedCalendar());
        string rule = to_string(oisConv->rule());
        string floatFreq = to_string(oisIndex->tenor());
        string startDate = to_string(today_);
        string endDate = to_string(maturity);
        Natural spotDays = oisConv->spotLag();
        string floatDC = to_string(oisConv->fixedDayCounter());
        string convention = to_string(oisConv->fixedConvention());
        string ccy = to_string(oisIndex->currency());
        ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, convention, convention, rule));
        vector<Real> notionals(1, notional);
        vector<Real> spreads(0, 0);
        bool isPayer = true;        
        LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(indexName, spotDays, false, spreads), !isPayer, ccy,
                            floatSchedule, floatDC, notionals);
        return floatingLeg;
    }


LegData TradeGenerator::buildIborLeg(string conventionId, Real notional, Date maturity,
                                        std::map<std::string, std::string> mapPairs) {
        auto conv = conventions_.get(conventionId);
        auto iborConv = QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv);
        string indexName = to_string(iborConv->indexName());
        QuantLib::ext::shared_ptr<IborIndex> iborIndex;
        tryParseIborIndex(indexName, iborIndex);
        string cal = to_string(iborConv->fixedCalendar());
        string rule = "";
        string floatFreq = to_string(iborIndex->tenor());
        string startDate = to_string(today_);
        string endDate = to_string(maturity);
        Natural spotDays = 2;
        
        string floatDC = to_string(iborConv->fixedDayCounter());
        string convention = to_string(iborConv->fixedConvention());
        string ccy = to_string(iborIndex->currency());

        ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, convention, convention, rule));
        vector<Real> notionals(1, notional);

        vector<Real> spreads(0, 0);
        bool isPayer = true;
        LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(indexName, spotDays, false, spreads), !isPayer,
                            ccy, floatSchedule, floatDC, notionals);
        return floatingLeg;
    }

} // namespace data
} // namespace ore
