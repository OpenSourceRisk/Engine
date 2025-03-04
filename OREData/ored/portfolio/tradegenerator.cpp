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
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/curveconfig.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/utilities/parsers.hpp>
#include <stdexcept>
#include <ql/errors.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>
#include "optiondata.hpp"
using namespace std;
//#include "../../../OREAnalytics/orea/app/inputparameters.hpp"

using ore::data::XMLUtils;


namespace ore {
namespace data {
using namespace ore::data;
// using std::runtime_error;
using std::map;
using std::string;




TradeGenerator::TradeGenerator(std::string curveconfigFile, std::string counterpartyId, std::string nettingSetId) {
    today_ = Settings::instance().evaluationDate();
    if (curveconfigFile != "") {
        addCurveConfigs(curveconfigFile);
    }
    if (counterpartyId != "") {
        setCounterpartyId(counterpartyId);
    }
    if (nettingSetId != "") {
        setNettingSet(nettingSetId);
    }
    addConventions();
}

void TradeGenerator::addConventions() {

    conventions_ = map < string, QuantLib::ext::shared_ptr<Convention>>();
    auto swapConventions = InstrumentConventions::instance().conventions()->get(Convention::Type::Swap);
    auto oisConventions = InstrumentConventions::instance().conventions()->get(Convention::Type::OIS);
    for (auto conv : oisConventions) {
        QuantLib::ext::shared_ptr<OisConvention> oisConv = QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv);
        conventions_[to_string(oisConv->indexName())] = conv;
    }
    for (auto conv : swapConventions) {
        QuantLib::ext::shared_ptr<IRSwapConvention> swapConv = QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv);
        conventions_[to_string(swapConv->indexName())] = conv;
    }
    return;
    
}

void TradeGenerator::addCurveConfigs(std::string curveConfigFile) {

    curveConfigs_.fromFile(curveConfigFile);
    return;
}

void TradeGenerator::addReferenceData(std::string refDataFile) {
    referenceData_ = BasicReferenceDataManager(refDataFile);
    return;
}

void TradeGenerator::setNettingSet(std::string nettingSetId)
{
    nettingSetId_ = nettingSetId;
    return;
}

void TradeGenerator::setCounterpartyId(std::string counterpartyId) 
{
    counterpartyId_ = counterpartyId;
    return;
}

bool TradeGenerator::validateDate(std::string date) { 
    try {
        
        QuantLib::Date x = ore::data::parseDate(date);
        return true;
    }
    catch (QuantLib::Error e)
    {
        cout << date + " not a valid date format" << endl;
        return false;
    }
    
}

/* QuantLib::ext::shared_ptr<Trade> buildOisSwap(string ccy, Real notional, Date maturityDate) { return NULL;

}*/

void TradeGenerator::buildSwap(string indexId, Real notional, string maturity, Real rate, bool isPayer,
                               std::map<std::string, std::string> mapPairs) {
    QuantLib::ext::shared_ptr<Convention> conv = conventions_.at(indexId);
    string cal;
    string rule;
    string floatFreq;
    string startDate;
    string endDate;
    Natural spotDays;
    string floatDC;
    string convention;
    string ccy;
    LegData floatingLeg;
    string fixedFreq;
    string fixedDC;
    string type;
    switch (conv->type()){


        
    
    case Convention::Type::OIS: {
        floatingLeg = buildOisLeg(conv, notional, maturity, isPayer, mapPairs);
        fixedFreq = frequencyToTenor(QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv)->fixedFrequency());
        fixedDC = to_string(QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv)->fixedDayCounter());
        type = "OIS";
        break;
    }
     case Convention::Type::Swap:
        floatingLeg = buildIborLeg(conv, notional, maturity, isPayer, mapPairs);
         fixedFreq = frequencyToTenor(QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv)->fixedFrequency());
         fixedDC = to_string(QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv)->fixedDayCounter());
         type = "IBOR";
        break;


        //buildOisSwap(conventionId, notional, maturity, rate, QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv));
    }
     cal = floatingLeg.schedule().rules().front().calendar();
     rule = floatingLeg.schedule().rules().front().rule();
     ScheduleData fixedSchedule(ScheduleRules(startDate, endDate, fixedFreq, cal, convention, convention, rule));
     vector<Real> rates(1, rate);
     ccy = floatingLeg.currency();
     LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(rates), isPayer, ccy, fixedSchedule, fixedDC, floatingLeg.notionals());

     Envelope env = Envelope("CP");
     QuantLib::ext::shared_ptr<Trade> trade(new ore::data::Swap(env, floatingLeg, fixedLeg));
     
     trade->id() =  to_string(size() + 1) + "_" + ccy + "_" + type + "_SWAP";
     add(trade);
    return;
}

void TradeGenerator::buildSwap(string indexId, Real notional, string maturity, bool isPayer,
                               std::map<std::string, std::string> mapPairs) {
    auto conv = conventions_.at(indexId);
    
    if (spotValues_.count(indexId) > 0) {
        Real spotValue = spotValues_[indexId];
        buildSwap(indexId, notional, maturity, spotValue, isPayer, mapPairs);
    }
    else
    {
        cout << "No market data found for index " << indexId << " in convention " << indexId << endl;
    }
}


void TradeGenerator::buildFxOption(string payCcy, Real payNotional, string recCcy, Real recNotional, string expiryDate,
                                   bool isLong, bool isCall, std::map<std::string, std::string> mapPairs) {
    Envelope env = Envelope(counterpartyId_, nettingSetId_);

    string longShort = isLong ? "Long" : "Short";
    string putCall = isCall ? "Call" : "Put";
    OptionData option(longShort, putCall, "European", false, vector<string>(1, to_string(expiryDate)), "Cash", "");
    QuantLib::ext::shared_ptr<Trade> trade(
        new ore::data::FxOption(env, option, recCcy, recNotional, payCcy, payNotional));
    trade->id() = to_string(size() + 1) + "_" + payCcy + "-" + recCcy + "_FX_OPTION";
    add(trade);
    return;
}

void TradeGenerator::buildFxOption(string payCcy, Real payNotional, string recCcy, string expiryDate, bool isLong,
                                   bool isCall, std::map<std::string, std::string> mapPairs) {
    Real rate = 1.0;
    if (spotValues_.count(payCcy + "-" + recCcy) == 1)
    {
        rate = spotValues_[payCcy + "-" + recCcy];
    }
    else if (spotValues_.count(recCcy + "-" + payCcy) == 1)
    {
        rate = 1.0 / spotValues_[recCcy + "-" + payCcy];
    }
    else
    {
        cout << "No market data found for FX pair " << payCcy << "-" << recCcy << endl;
    }
    buildFxOption(payCcy, payNotional, recCcy, payNotional * rate, expiryDate, isLong, isCall, mapPairs);
    return;
    
}



void TradeGenerator::buildEquityOption(string equityCurveId, Real quantity, string maturity, Real strike,
                                       std::map<std::string, std::string> mapPairs) {
    

    map<string, string> eqData = fetchEquityRefData(equityCurveId);
    
    
    
    string rule = "Forward";
    string expiryDate = ore::data::to_string(maturity);
    string longShort = mapPairs.count("longShort") == 1 ? mapPairs["longShort"] : "Long";
    string putCall = mapPairs.count("putCall") == 1 ? mapPairs["putCall"] : "Call";
    TradeStrike tradeStrike(strike, eqData["currency"]);
    Envelope env("CP");
    OptionData option(longShort, putCall, "European", false, vector<string>(1, expiryDate), "Cash", "",
                     PremiumData());
    
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::EquityOption(env, option, EquityUnderlying(equityCurveId),
                                                                       eqData["currency"], quantity, tradeStrike));
    trade->id() = to_string(size() + 1) + "_" + equityCurveId + "_EQ_OPTION";
    add(trade);
    return ;
}

void TradeGenerator::buildEquityForward(string equityCurveId, Real quantity, string maturity, Real strike,
                                        std::map<std::string, std::string> mapPairs) {
    map<string, string> eqData = fetchEquityRefData(equityCurveId);
    string longShort = mapPairs.count("longShort") == 1 ? mapPairs["longShort"] : "Long";
    string expiryDate = ore::data::to_string(maturity);

    // envelope
    Envelope env("CP");
    // trade
    QuantLib::ext::shared_ptr<Trade> trade(new ore::data::EquityForward(env, longShort, EquityUnderlying(equityCurveId),
                                                                        eqData["currency"], quantity, expiryDate, strike));
    trade->id() = to_string(size() + 1) + "_" + equityCurveId + "_EQ_FORWARD";
    add(trade);
    return;
}

void TradeGenerator::buildCapFloor(string indexName, Real capFloorRate, Real notional, string maturity, bool isLong, bool isCap,
                                   std::map<std::string, std::string> mapPairs) {
    QuantLib::ext::shared_ptr<Convention> conv = conventions_.at(indexName);

    LegData floatingLeg;
    switch (conv->type())
    { case Convention::Type::OIS:
    {
        floatingLeg = buildOisLeg(conv, notional, maturity, isCap, mapPairs);
        break;
    }
    case Convention::Type::Swap:
        floatingLeg = buildIborLeg(conv, notional, maturity, isCap, mapPairs);
        break;
    }

    vector<double> capRates(0, 0);
    vector<double> floorRates(0, 0);
    string longShort = isLong ? "Long" : "Short";
    if (isCap)
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

void TradeGenerator::buildEquitySwap(string equityCurveId, string returnType, Real quantity, string indexId,
                                         Real notional, string maturity, bool isPayer, std::map<std::string, std::string> mapPairs) {
        auto convention = conventions_.at(indexId);
    string indexName;
        LegData floatingLeg;
        switch (convention->type()) {
        case Convention::Type::OIS:
            floatingLeg = buildOisLeg(convention, notional, maturity, isPayer, mapPairs);
            break;

        case Convention::Type::Swap:
            floatingLeg = buildIborLeg(convention, notional, maturity, isPayer, mapPairs);
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
                                         Real notional, string maturity, bool isPayer, std::map<std::string, std::string> mapPairs) {
        auto config = curveConfigs_.equityCurveConfig(equityCurveId);
        string indexName = config->forecastingCurve();
        
        
        QuantLib::ext::shared_ptr<IborIndex> iborIndex;
        Real dividendFactor = 1;
        tryParseIborIndex(indexName, iborIndex);
        string ccy = to_string(iborIndex->currency());
        string floatFreq = to_string( iborIndex->tenor());
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

void TradeGenerator::setSpotValues(std::map<std::string, QuantLib::Real> spotValues) {
    spotValues_ = spotValues;
    return;
}




LegData TradeGenerator::buildOisLeg(QuantLib::ext::shared_ptr<Convention> conv, Real notional, string maturity,
                                    bool isPayer,
    std::map<std::string, std::string> mapPairs)
{
        QuantLib::ext::shared_ptr<OisConvention> oisConv = QuantLib::ext::dynamic_pointer_cast<OisConvention>(conv);
        string indexName = to_string(oisConv->indexName());
        QuantLib::ext::shared_ptr<IborIndex> oisIndex;
        tryParseIborIndex(indexName, oisIndex);
        string cal = to_string(oisConv->fixedCalendar());
        string rule = to_string(oisConv->rule());
        string floatFreq = to_string(oisIndex->tenor());
        auto qlStartDate = today_ + oisConv->spotLag();
        string startDate = to_string(qlStartDate);
        string endDate;
        if (validateDate(maturity)) {
            endDate = to_string(maturity);
        }
        else
        {
            endDate = to_string(qlStartDate + parsePeriod(maturity));
        }
        /* auto config = curveConfigs_.get(CurveSpec::CurveType::Yield, oisConv->indexName());
        auto quotes = config->quotes();
        string neededQuote = "";
        for each (string quote in quotes)
        {
            std::vector<std::string> tokens;
            boost::split(quote, tokens, boost::is_any_of("/"));
            
        }*/
        Natural spotDays = oisConv->spotLag();
        string floatDC = to_string(oisConv->fixedDayCounter());
        string convention = to_string(oisConv->fixedConvention());
        string ccy = to_string(oisIndex->currency());
        ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, convention, convention, rule));
        vector<Real> notionals(1, notional);
        vector<Real> spreads(0, 0);
        LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(indexName, spotDays, false, spreads), !isPayer, ccy,
                            floatSchedule, floatDC, notionals);
        return floatingLeg;
    }


LegData TradeGenerator::buildIborLeg(QuantLib::ext::shared_ptr < Convention> conv, Real notional, string maturity,
                                         bool isPayer,
                                        std::map<std::string, std::string> mapPairs) {
        
        QuantLib::ext::shared_ptr<IRSwapConvention> iborConv = QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conv);
        string indexName = to_string(iborConv->indexName());
        string cal = to_string(iborConv->fixedCalendar());
        string rule = "";
        string floatFreq = to_string(iborConv->floatFrequency());
        string startDate = to_string(today_);
        string endDate = to_string(maturity);
        Natural spotDays = 2;
        
        string floatDC = to_string(iborConv->fixedDayCounter());
        string convention = to_string(iborConv->fixedConvention());
        string ccy = "";
        //to_string(iborConv->());

        ScheduleData floatSchedule(ScheduleRules(startDate, endDate, floatFreq, cal, convention, convention, rule));
        vector<Real> notionals(1, notional);

        vector<Real> spreads(0, 0);
        LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(indexName, spotDays, false, spreads), !isPayer,
                            ccy, floatSchedule, floatDC, notionals);
        return floatingLeg;
    }

std::string TradeGenerator::frequencyToTenor(const QuantLib::Frequency& freq) {
        switch (freq) {
        case QuantLib::Monthly:
            return "1M";
        case QuantLib::Quarterly:
            return "3M";
        case QuantLib::Semiannual:
            return "6M";
        case QuantLib::Annual:
            return "1Y";
        default:
            ALOG("Unexpected frequency " << to_string(freq) << "Fallback to 12M");
            return "1Y";
        }
}

map<string, string> TradeGenerator::fetchEquityRefData(string equityId) {

    string currency;
    string cal;
    string conv;
    map<string, string> retMap = {{"currency", ""}, {"cal", ""}, {"conv", ""}};
    if (curveConfigs_.hasEquityCurveConfig(equityId)) {
        auto config = curveConfigs_.equityCurveConfig(equityId);
        retMap["currency"] = config->currency();
        retMap["cal"] = config->calendar();
        retMap["conv"] = config->dayCountID();

    } else if (referenceData_.hasData("Equity", equityId)) {
        auto refDatum =
            QuantLib::ext::dynamic_pointer_cast<EquityReferenceDatum>(referenceData_.getData("Equity", equityId));
        retMap["currency"] = refDatum->equityData().currency;
        retMap["cal"] = refDatum->equityData().currency;
    
    }
    return retMap;
}
} // namespace data
} // namespace ore
