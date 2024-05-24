/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/scriptedtrade.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/worstofbasketswap.hpp>
#include <ored/scripting/utilities.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

void WorstOfBasketSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    auto builder = QuantLib::ext::dynamic_pointer_cast<ScriptedTradeEngineBuilder>(factory->builder("ScriptedTrade"));

    // set script parameters

    clear();
    initIndices();

    // Manually defined map that specifies which schedules should be mandatory, and which ones should be optional.
    // Mandatory schedules are specified with a blank string (""). For optional schedules, the default schedule
    // that they will be derived from must be specified.
    // clang-format off
    schedules_ = {
        // schedule name                    ST event data                       fallback schedule
        // mandatory schedules
        {"FloatingPeriodSchedule",          {floatingPeriodSchedule_,           ""}},
        {"FloatingPayDates",                {floatingPayDates_,                 ""}},
        // optional schedules
        {"FloatingFixingSchedule",          {floatingFixingSchedule_,           "FloatingPeriodSchedule"}},
        {"FixedDeterminationSchedule",      {fixedDeterminationSchedule_,       "FloatingPeriodSchedule"}},
        {"KnockInDeterminationSchedule",    {knockInDeterminationSchedule_,     "FloatingPeriodSchedule"}},
        {"KnockOutDeterminationSchedule",   {knockOutDeterminationSchedule_,    "FloatingPeriodSchedule"}},
        {"FixedAccrualSchedule",            {fixedAccrualSchedule_,             "FloatingPeriodSchedule"}},
        {"FixedPayDates",                   {fixedPayDates_,                    "FloatingPayDates"}}
    };
    // clang-format on

    string name;
    string defaultName;
    ScriptedTradeEventData ed;

    if (accruingFixedCoupons_) {
        QL_REQUIRE(fixedAccrualSchedule_.hasData(),
                   "FixedAccrualSchedule must be specified for accruing fixed coupons.");
    }

    // First, we check the schedule dates that are mandatory
    for (auto s : schedules_) {
        name = s.first;
        ed = s.second.first;
        defaultName = s.second.second;
        if (defaultName.empty()) {
            QL_REQUIRE(ed.hasData(), "Could not find mandatory node " << name << ".");
        }
    }

    // Next, we ensure that each optional schedule has a valid EventData schedule
    for (auto s : schedules_) {
        name = s.first;
        ed = s.second.first;
        defaultName = s.second.second;
        if (!defaultName.empty() && !ed.hasData()) {
            auto defaultEventData = schedules_.find(defaultName);
            QL_REQUIRE(defaultEventData != schedules_.end(), "Could not find node " << defaultName);
            
            auto n = schedules_[defaultName].first;
            ScriptedTradeEventData newEventData; 

            if (n.type() == ScriptedTradeEventData::Type::Array) {
                newEventData = ScriptedTradeEventData(name, n.schedule());
            } else if (n.type() == ScriptedTradeEventData::Type::Derived) {
                newEventData = ScriptedTradeEventData(name, n.baseSchedule(), n.shift(), n.calendar(), n.convention());
            }
            schedules_[name].first = newEventData;
        }
    }

    // We build the schedules first that are defined by a ScheduleData node
    for (auto s : schedules_) {
        ed = s.second.first;
        if (ed.type() == ScriptedTradeEventData::Type::Array) {
            events_.emplace_back(ed);
        }
    }

    // And then we build the schedules that are defined by a DerivedSchedule node, i.e. are dependent on the above
    for (auto s : schedules_) {
        ed = s.second.first;
        if (ed.type() == ScriptedTradeEventData::Type::Derived) {
            events_.emplace_back(ed);
        }
    }

    // check underlying types
    string type = underlyings_.front()->type();
    for (auto u : underlyings_) {
        QL_REQUIRE(u->type() == type, "All of Underlyings must be from the same asset class.");
    }
    auto floatingIndex = *factory->market()->iborIndex(floatingIndex_, builder->configuration(MarketContext::pricing));
    auto ois = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(floatingIndex);

    if (ois) {
        DLOG("building WorstOfBasketSwap scripted trade wrapper using (internal) script \'Overnight\'")
    } else {
        DLOG("building WorstOfBasketSwap scripted trade wrapper using (internal) script \'Standard\'");
    }

    if (bermudanKnockIn_) {
        QL_REQUIRE(knockInDeterminationSchedule_.hasData(),
                   "KnockInDeterminationSchedule must be specified for a Bermudan knock-in.");
    }

    if (accruingFixedCoupons_) {
        QL_REQUIRE(fixedAccrualSchedule_.hasData(),
                   "FixedAccrualSchedule must be specified for accruing fixed coupons.");
    }

    if (knockInPayDate_.empty()) {
        QuantLib::Date newKnockInPayDate = makeSchedule(schedules_["FloatingPayDates"].first.schedule()).dates().back();
        knockInPayDate_ = ore::data::to_string(newKnockInPayDate);
    }
    events_.emplace_back("KnockInPayDate", knockInPayDate_);
    if (initialFixedPayDate_.empty()) {
        QuantLib::Date newInitialFixedPayDate = makeSchedule(schedules_["FloatingPayDates"].first.schedule()).dates().front();
        initialFixedPayDate_ = ore::data::to_string(newInitialFixedPayDate);
    }
    events_.emplace_back("InitialFixedPayDate", initialFixedPayDate_);

    // numbers
    numbers_.emplace_back("Number", "Quantity", quantity_);
    strike_ = strike_.empty() ? "1.0" : strike_;
    numbers_.emplace_back("Number", "Strike", strike_);
    initialFixedRate_ = initialFixedRate_.empty() ? "0.0" : initialFixedRate_ ;
    numbers_.emplace_back("Number", "InitialFixedRate", initialFixedRate_);
    numbers_.emplace_back("Number", "FixedRate", fixedRate_);
    numbers_.emplace_back("Number", "InitialPrices", initialPrices_);
    numbers_.emplace_back("Number", "FixedTriggerLevels", fixedTriggerLevels_);
    numbers_.emplace_back("Number", "KnockOutLevels", knockOutLevels_);
    knockInLevel_ = knockInLevel_.empty() ? "1.0" : knockInLevel_;
    numbers_.emplace_back("Number", "KnockInLevel", knockInLevel_);
    floatingSpread_ = floatingSpread_.empty() ? "0.0" : floatingSpread_;
    numbers_.emplace_back("Number", "FloatingSpread", floatingSpread_);
    floatingRateCutoff_ = floatingRateCutoff_.empty() ? "0" : floatingRateCutoff_;
    if (ois)
        QL_REQUIRE(parseInteger(floatingRateCutoff_) >= 0, "FloatingRateCutoff should be a non-negative whole number.");
    numbers_.emplace_back("Number", "FloatingRateCutoff", floatingRateCutoff_);
    if (ois)
        QL_REQUIRE(floatingLookback_.units() == TimeUnit::Days,
                   "FloatingLookback (" << floatingLookback_ << ") should be given with units days.");
    numbers_.emplace_back("Number", "FloatingLookback", std::to_string(floatingLookback_.length()));

    // booleans
    numbers_.emplace_back("Number", "BermudanKnockIn", bermudanKnockIn_ ? "1" : "-1");
    numbers_.emplace_back("Number", "AccumulatingFixedCoupons", accumulatingFixedCoupons_ ? "1" : "-1");
    numbers_.emplace_back("Number", "AccruingFixedCoupons", accruingFixedCoupons_ ? "1" : "-1");
    numbers_.emplace_back("Number", "LongShort", parsePositionType(longShort_) == Position::Long ? "1" : "-1");
    numbers_.emplace_back("Number", "IsAveraged", isAveraged_ ? "1" : "-1");
    numbers_.emplace_back("Number", "IncludeSpread", includeSpread_ ? "1" : "-1");

    //bool inArrears;
    //if (inArrears_.empty()) {
    //    // If left blank or omitted, default to true if OIS, otherwise (e.g. ibor) default to false
    //    inArrears = ois ? true : false;
    //} else {
    //    inArrears = parseBool(inArrears_);
    //}
    //numbers_.emplace_back("Number", "IsInArrears", inArrears ? "1" : "-1");
    
    // daycounters
    daycounters_.emplace_back("DayCounter", "FloatingDayCountFraction", floatingDayCountFraction_.name());

    // currencies
    currencies_.emplace_back("Currency", "Currency", currency_);

    // set product tag accordingly
    if (type == "InterestRate") {
        productTag_ = "MultiUnderlyingIrOption";
    } else {
        productTag_ = "MultiAssetOptionAD({AssetClass})";
    }
    
    LOG("ProductTag=" << productTag_);

    // set script

    // clang-format off
    const std::string fixingScriptStandard = 
                 "  fixing = FloatingIndex(FloatingFixingSchedule[d-1]) + FloatingSpread;\n";
    const std::string fixingScriptOvernight = 
	std::string("  fixing = FWD") + (isAveraged_ ? "AVG" : "COMP") + std::string("(FloatingIndex, FloatingPeriodSchedule[d-1], FloatingPeriodSchedule[d-1], FloatingPeriodSchedule[d], FloatingSpread, 1, FloatingLookback, FloatingRateCutoff, 0, IncludeSpread);\n");

    const std::string floatingFixingScript = ois ? fixingScriptOvernight : fixingScriptStandard;

    // clang-format on

    script_ = {
        // clang-format off
        {"",
         ScriptedTradeScriptData(
                 "NUMBER alive, couponAccumulation, fixing, n, indexInitial;\n"
                 "NUMBER allAssetsTriggered, indexFinal, performance, worstPerformance, d, payoff, u, knockedIn;\n"
                 "NUMBER floatingAccrualFraction, fixedAccrualFraction;\n"
                 "NUMBER lastIdx, accrualPeriodIdx, accrualFractions[SIZE(FixedTriggerLevels)], totalDays;\n"
                 "NUMBER ad, cd, dd, ed, fd;\n"
                 "\n"
                 "Option = Option + LOGPAY(LongShort * Quantity * InitialFixedRate, InitialFixedPayDate,\n"
                 "                         InitialFixedPayDate, Currency, 0, InitialFixedAmount);\n"
                 "\n"
                 "n = SIZE(FloatingPeriodSchedule);\n"
                 "REQUIRE n - SIZE(FloatingPayDates) <= 1;\n"
                 "REQUIRE n - SIZE(FloatingFixingSchedule) <= 1;\n"
                 "REQUIRE n - SIZE(FixedDeterminationSchedule) <= 1;\n"
                 "REQUIRE n - SIZE(KnockInDeterminationSchedule) <= 1;\n"
                 "REQUIRE n - SIZE(KnockOutDeterminationSchedule) <= 1;\n"
                 "REQUIRE n - SIZE(FixedPayDates) <= 1;\n"
                 "\n"
                 "IF SIZE(FloatingPayDates) == n THEN ad = 0; ELSE ad = -1; END;\n"
                 "IF SIZE(FixedDeterminationSchedule) == n THEN cd = 0; ELSE cd = -1; END;\n"
                 "IF SIZE(KnockInDeterminationSchedule) == n THEN dd = 0; ELSE dd = -1; END;\n"
                 "IF SIZE(KnockOutDeterminationSchedule) == n THEN ed = 0; ELSE ed = -1; END;\n"
                 "IF SIZE(FixedPayDates) == n THEN fd = 0; ELSE fd = -1; END;\n"
                 "\n"
                 "couponAccumulation = 1;\n"
                 "alive = 1;\n"
                 "IF BermudanKnockIn == 1 THEN\n"
                 "  FOR d IN (1, SIZE(KnockInDeterminationSchedule), 1) DO\n"
                 "    FOR u IN (1, SIZE(Underlyings), 1) DO\n"
                 "      IF Underlyings[u](KnockInDeterminationSchedule[d+dd]) < KnockInLevel * InitialPrices[u] THEN\n"
                 "        knockedIn = 1;\n"
                 "      END;\n"
                 "    END;\n"
                 "  END;\n"
                 "END;\n"
                 "\n"
                 "IF AccruingFixedCoupons == 1 THEN\n"
                 "  lastIdx = 1;\n"
                 "  FOR d IN (1, SIZE(FixedAccrualSchedule), 1) DO \n"
                 "    accrualPeriodIdx = DATEINDEX(FixedAccrualSchedule[d], FixedDeterminationSchedule, GEQ) - 1;\n"
                 "    IF accrualPeriodIdx > 0 AND accrualPeriodIdx < SIZE(FixedDeterminationSchedule) THEN\n"
                 "      IF lastIdx != accrualPeriodIdx THEN\n"
                 "        accrualFractions[lastIdx] = accrualFractions[lastIdx] / totalDays;\n"
                 "        lastIdx = accrualPeriodIdx;\n"
                 "        totalDays = 1;\n"
                 "      END;\n"
                 "\n"
                 "      allAssetsTriggered = 1;"
                 "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
                 "        IF Underlyings[u](FixedAccrualSchedule[d]) < FixedTriggerLevels[accrualPeriodIdx] * InitialPrices[u] THEN\n"
                 "          allAssetsTriggered = 0;\n"
                 "        END;\n"
                 "      END;\n"
                 "      accrualFractions[accrualPeriodIdx] = accrualFractions[accrualPeriodIdx] + allAssetsTriggered;\n"
                 "      totalDays = totalDays + 1;\n"
                 "      IF d == SIZE(FixedAccrualSchedule) THEN\n"
                 "        accrualFractions[SIZE(FixedTriggerLevels)] = accrualFractions[SIZE(FixedTriggerLevels)] / totalDays;\n"
                 "      END;\n"
                 "    END;\n"
                 "  END;\n"
                 "END;\n"
                 "\n"
                 "FOR d IN (2, n, 1) DO\n"
                 + floatingFixingScript + 
                 "  floatingAccrualFraction = dcf(FloatingDayCountFraction, FloatingPeriodSchedule[d-1], FloatingPeriodSchedule[d]);\n"
                 "  Option = Option + LOGPAY(-1 * LongShort * Quantity * alive * fixing * floatingAccrualFraction,\n"
                 "                           FloatingFixingSchedule[d-1], FloatingPayDates[d+ad], Currency, 1, FloatingLeg);\n"
                 "\n"
                 "  allAssetsTriggered = 1;\n"
                 "  FOR u IN (1, SIZE(Underlyings), 1) DO\n"
                 "    IF Underlyings[u](FixedDeterminationSchedule[d+cd]) < FixedTriggerLevels[d-1] * InitialPrices[u] THEN\n"
                 "      allAssetsTriggered = 0;\n"
                 "    END;\n"
                 "  END;\n"
                 "  IF AccruingFixedCoupons == 1 THEN\n"
                 "    fixedAccrualFraction = allAssetsTriggered * accrualFractions[d-1] + (1-allAssetsTriggered) * fixedAccrualFraction;\n"
                 "  ELSE\n"
                 "    fixedAccrualFraction = allAssetsTriggered + (1-allAssetsTriggered) * fixedAccrualFraction;\n"
                 "  END;\n"
                 "  Option = Option + LOGPAY(allAssetsTriggered * LongShort * Quantity * alive * FixedRate * couponAccumulation * fixedAccrualFraction,\n"
                 "                           FixedDeterminationSchedule[d+cd], FixedPayDates[d+fd], Currency, 2, FixedCouponLeg);\n"
                 "  couponAccumulation = allAssetsTriggered + (1-allAssetsTriggered) * couponAccumulation;\n"
                 "  IF AccumulatingFixedCoupons == 1 THEN\n"
                 "    couponAccumulation = couponAccumulation + (1 - allAssetsTriggered);\n"
                 "  END;\n"
                 "\n"
                 "  IF d == n THEN\n"
                 "    worstPerformance = 999999.9;\n"
                 "    FOR u IN (1, SIZE(Underlyings), 1) DO\n"
                 "      indexInitial = InitialPrices[u];\n"
                 "      indexFinal = Underlyings[u](FloatingPeriodSchedule[n]);\n"
                 "      performance = indexFinal / indexInitial;\n"
                 "\n"
                 "      IF performance < worstPerformance THEN\n"
                 "        worstPerformance = performance;\n"
                 "      END;\n"
                 "    END;\n"
                 "\n"
                 "    IF worstPerformance < KnockInLevel THEN\n"
                 "      knockedIn = 1;\n"
                 "    END;\n"
                 "\n"
                 "    IF worstPerformance < Strike THEN\n"
                 "      payoff = worstPerformance - Strike;\n"
                 "      Option = Option + LOGPAY(LongShort * Quantity * alive * payoff * knockedIn, FloatingPeriodSchedule[n],\n"
                 "                               KnockInPayDate, Currency, 3, EquityAmountPayoff);\n"
                 "    END;\n"
                 "  END;\n"
                 "\n"
                 "  IF d != n THEN\n"
                 "    allAssetsTriggered = 1;\n"
                 "    FOR u IN (1, SIZE(Underlyings), 1) DO\n"
                 "      IF Underlyings[u](KnockOutDeterminationSchedule[d+ed]) < KnockOutLevels[d-1] * InitialPrices[u] THEN\n"
                 "        allAssetsTriggered = 0;\n"
                 "      END;\n"
                 "    END;\n"
                 "    alive = alive * (1 - allAssetsTriggered);\n"
                 "  END;\n"
                 "END;\n",
             //
             "Option",
             {{"currentNotional", "Quantity"},
              {"notionalCurrency", "Currency"}},
             {})}};
        // clang-format on

    // build trade

    ScriptedTrade::build(factory);
}

void WorstOfBasketSwap::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // ISDA taxonomy
    // asset class set in the base class already
    std::string assetClass = boost::any_cast<std::string>(additionalData_["isdaAssetClass"]);
    if (assetClass == "Equity") {
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    } else if (assetClass == "Commodity") {
        // isda taxonomy missing for this class, using the same as equity
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    } else if (assetClass == "Foreign Exchange") {
        additionalData_["isdaBaseProduct"] = string("Exotic");
        additionalData_["isdaSubProduct"] = string("Generic");
    }
    additionalData_["isdaTransaction"] = string("Basket");
}

void WorstOfBasketSwap::initIndices() {
    indices_.emplace_back("Index", "FloatingIndex", floatingIndex_);
    
    std::vector<string> underlyings;
    for (auto const& u : underlyings_) {
        underlyings.push_back(scriptedIndexName(u));
    }
    indices_.emplace_back("Index", "Underlyings", underlyings);
}

ScriptedTradeEventData readEventData(XMLNode* node) {
    ScriptedTradeEventData eventData;

    string name = XMLUtils::getNodeName(node);
    if (XMLNode* sch = XMLUtils::getChildNode(node, "DerivedSchedule")) {
        string baseSchedule = XMLUtils::getChildValue(sch, "BaseSchedule", true);
        
        string shift = XMLUtils::getChildValue(sch, "Shift", false);
        shift = shift.empty() ? "0D" : shift;

        string calendar = XMLUtils::getChildValue(sch, "Calendar", false);
        calendar = calendar.empty() ? "NullCalendar" : calendar;

        string convention = XMLUtils::getChildValue(sch, "Convention", false);
        convention = convention.empty() ? "Unadjusted" : convention;

        eventData = ScriptedTradeEventData(name, baseSchedule, shift, calendar, convention);

    } else {
        ScheduleData scheduleData = ScheduleData();
        scheduleData.fromXML(node);
        eventData = ScriptedTradeEventData(name, scheduleData);
    }

    return eventData;
}

void WorstOfBasketSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(tradeDataNode, "WorstOfBasketSwapData node not found");
    longShort_ = XMLUtils::getChildValue(tradeDataNode, "LongShort", true);
    quantity_ = XMLUtils::getChildValue(tradeDataNode, "Quantity", true);
    strike_ = XMLUtils::getChildValue(tradeDataNode, "Strike", false);
    initialFixedRate_ = XMLUtils::getChildValue(tradeDataNode, "InitialFixedRate", false);
    fixedRate_ = XMLUtils::getChildValue(tradeDataNode, "FixedRate", true);

    XMLNode* initialPricesNode = XMLUtils::getChildNode(tradeDataNode, "InitialPrices");
    QL_REQUIRE(initialPricesNode, "Could not find an InitialPrices node.");
    auto initialPrices = XMLUtils::getChildrenNodes(initialPricesNode, "InitialPrice");
    for (auto const& p : initialPrices)
        initialPrices_.push_back(XMLUtils::getNodeValue(p));

    XMLNode* fixedTriggerLevelsNode = XMLUtils::getChildNode(tradeDataNode, "FixedTriggerLevels");
    QL_REQUIRE(fixedTriggerLevelsNode, "Could not find a FixedTriggerLevels node.");
    auto fixedTriggerLevels = XMLUtils::getChildrenNodes(fixedTriggerLevelsNode, "FixedTriggerLevel");
    for (auto const& level : fixedTriggerLevels)
        fixedTriggerLevels_.push_back(XMLUtils::getNodeValue(level));

    XMLNode* knockOutLevelsNode = XMLUtils::getChildNode(tradeDataNode, "KnockOutLevels");
    QL_REQUIRE(knockOutLevelsNode, "Could not find a KnockOutLevels node.");
    auto knockOutLevels = XMLUtils::getChildrenNodes(knockOutLevelsNode, "KnockOutLevel");
    for (auto const& level : knockOutLevels)
        knockOutLevels_.push_back(XMLUtils::getNodeValue(level));
    
    if (XMLNode* floatingPeriodSchedule = XMLUtils::getChildNode(tradeDataNode, "FloatingPeriodSchedule"))
        floatingPeriodSchedule_ = readEventData(floatingPeriodSchedule);

    if (XMLNode* floatingFixingSchedule = XMLUtils::getChildNode(tradeDataNode, "FloatingFixingSchedule"))
        floatingFixingSchedule_ = readEventData(floatingFixingSchedule);
    
    if (XMLNode* fixedDeterminationSchedule = XMLUtils::getChildNode(tradeDataNode, "FixedDeterminationSchedule"))
        fixedDeterminationSchedule_ = readEventData(fixedDeterminationSchedule);

    XMLNode* knockOutDeterminationSchedule = XMLUtils::getChildNode(tradeDataNode, "KnockOutDeterminationSchedule");
    if (knockOutDeterminationSchedule)
        knockOutDeterminationSchedule_ = readEventData(knockOutDeterminationSchedule);

    if (XMLNode* knockInDeterminationSchedule = XMLUtils::getChildNode(tradeDataNode, "KnockInDeterminationSchedule"))
        knockInDeterminationSchedule_ = readEventData(knockInDeterminationSchedule);

    if (XMLNode* fixedAccrualSchedule = XMLUtils::getChildNode(tradeDataNode, "FixedAccrualSchedule"))
        fixedAccrualSchedule_ = readEventData(fixedAccrualSchedule);

    if (XMLNode* floatingPayDates = XMLUtils::getChildNode(tradeDataNode, "FloatingPayDates"))
        floatingPayDates_ = readEventData(floatingPayDates);

    if (XMLNode* fixedPayDates = XMLUtils::getChildNode(tradeDataNode, "FixedPayDates"))
        fixedPayDates_ = readEventData(fixedPayDates);
    
    XMLNode* underlyingsNode = XMLUtils::getChildNode(tradeDataNode, "Underlyings");
    QL_REQUIRE(underlyingsNode, "Could not find an Underlyings node.");
    auto underlyings = XMLUtils::getChildrenNodes(underlyingsNode, "Underlying");
    for (auto const& u : underlyings) {
        UnderlyingBuilder underlyingBuilder;
        underlyingBuilder.fromXML(u);
        underlyings_.push_back(underlyingBuilder.underlying());
    }

    knockInPayDate_ = XMLUtils::getChildValue(tradeDataNode, "KnockInPayDate", false);
    initialFixedPayDate_ = XMLUtils::getChildValue(tradeDataNode, "InitialFixedPayDate", false);

    string bermudanKnockIn = XMLUtils::getChildValue(tradeDataNode, "BermudanKnockIn", false);
    bermudanKnockIn_ = bermudanKnockIn.empty() ? false : parseBool(bermudanKnockIn);

    string accumulatingFixedCoupons = XMLUtils::getChildValue(tradeDataNode, "AccumulatingFixedCoupons", false);
    accumulatingFixedCoupons_ = accumulatingFixedCoupons.empty() ? false : parseBool(accumulatingFixedCoupons);

    string accruingFixedCoupons = XMLUtils::getChildValue(tradeDataNode, "AccruingFixedCoupons", false);
    accruingFixedCoupons_ = accruingFixedCoupons.empty() ? false : parseBool(accruingFixedCoupons);
        
    floatingIndex_ = internalIndexName(XMLUtils::getChildValue(tradeDataNode, "FloatingIndex", true));

    floatingSpread_ = XMLUtils::getChildValue(tradeDataNode, "FloatingSpread", false);
    string floatingDayCountFraction = XMLUtils::getChildValue(tradeDataNode, "FloatingDayCountFraction", true);
    floatingDayCountFraction_ = parseDayCounter(floatingDayCountFraction);
    
    string floatingLookback = XMLUtils::getChildValue(tradeDataNode, "FloatingLookback", false);
    floatingLookback_ = floatingLookback.empty() ? 0 * Days : parsePeriod(floatingLookback);

    floatingRateCutoff_ = XMLUtils::getChildValue(tradeDataNode, "FloatingRateCutoff", false);

    isAveraged_ = XMLUtils::getChildValueAsBool(tradeDataNode, "IsAveraged", false, false);

    //inArrears_ = XMLUtils::getChildValue(tradeDataNode, "IsInArrears", false);

    includeSpread_ = XMLUtils::getChildValueAsBool(tradeDataNode, "IncludeSpread", false, false);

    knockInLevel_ = XMLUtils::getChildValue(tradeDataNode, "KnockInLevel", false);
    currency_ = XMLUtils::getChildValue(tradeDataNode, "Currency", true);

    initIndices();
}

XMLNode* writeEventData(XMLDocument& doc, ScriptedTradeEventData& eventData) {
    XMLNode* n = doc.allocNode(eventData.name());
    if (eventData.type() == ScriptedTradeEventData::Type::Derived) {
        XMLNode* derivedSchedule = doc.allocNode("DerivedSchedule");
        XMLUtils::addChild(doc, derivedSchedule, "BaseSchedule", eventData.baseSchedule());
        XMLUtils::addChild(doc, derivedSchedule, "Shift", eventData.shift());
        XMLUtils::addChild(doc, derivedSchedule, "Calendar", eventData.calendar());
        XMLUtils::addChild(doc, derivedSchedule, "Convention", eventData.convention());
        XMLUtils::appendNode(n, derivedSchedule);
    } else if (eventData.type() == ScriptedTradeEventData::Type::Array) {
        if (!eventData.schedule().dates().empty()) {
            vector<ScheduleDates> scheduleDates = eventData.schedule().dates();
            for (auto& d : scheduleDates)
                XMLUtils::appendNode(n, d.toXML(doc));
        } else {
            vector<ScheduleRules> scheduleRules = eventData.schedule().rules();
            for (auto& r : scheduleRules)
                XMLUtils::appendNode(n, r.toXML(doc));
        }
        //ScheduleData scheduleData = eventData.schedule();
        //XMLUtils::appendNode(n, scheduleData.toXML(doc));
    } else {
        QL_FAIL(":writeEventData(): unexpected ScriptedTradeEventData::Type");
    }
    return n;
}

XMLNode* WorstOfBasketSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, tradeNode);
    XMLUtils::addChild(doc, tradeNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, tradeNode, "Currency", currency_);
    XMLUtils::addChild(doc, tradeNode, "Quantity", quantity_);
    XMLUtils::addChild(doc, tradeNode, "Strike", strike_);
    XMLUtils::addChild(doc, tradeNode, "InitialFixedRate", initialFixedRate_);
    XMLUtils::addChild(doc, tradeNode, "InitialFixedPayDate", initialFixedPayDate_);
    XMLUtils::addChild(doc, tradeNode, "FixedRate", fixedRate_);

    XMLNode* underlyingsNode = doc.allocNode("Underlyings");
    for (auto const& u : underlyings_)
        XMLUtils::appendNode(underlyingsNode, u->toXML(doc));
    XMLUtils::appendNode(tradeNode, underlyingsNode);

    XMLNode* initialPricesNode = doc.allocNode("InitialPrices");
    for (auto const& p : initialPrices_)
        XMLUtils::addChild(doc, initialPricesNode, "InitialPrice", p);
    XMLUtils::appendNode(tradeNode, initialPricesNode);

    XMLUtils::addChild(doc, tradeNode, "BermudanKnockIn", bermudanKnockIn_);
    XMLUtils::addChild(doc, tradeNode, "KnockInLevel", knockInLevel_);

    XMLNode* fixedTriggerLevelsNode = doc.allocNode("FixedTriggerLevels");
    for (auto const& level : fixedTriggerLevels_)
        XMLUtils::addChild(doc, fixedTriggerLevelsNode, "FixedTriggerLevel", level);
    XMLUtils::appendNode(tradeNode, fixedTriggerLevelsNode);

    XMLNode* knockOutLevelsNode = doc.allocNode("KnockOutLevels");
    for (auto const& p : knockOutLevels_)
        XMLUtils::addChild(doc, knockOutLevelsNode, "KnockOutLevel", p);
    XMLUtils::appendNode(tradeNode, knockOutLevelsNode);

    for (auto sch : schedules_) {
        XMLNode* tmp = writeEventData(doc, sch.second.first);
        XMLUtils::appendNode(tradeNode, tmp);
    }
    
    XMLUtils::addChild(doc, tradeNode, "KnockInPayDate", knockInPayDate_);
    XMLUtils::addChild(doc, tradeNode, "AccruingFixedCoupons", accruingFixedCoupons_);
    XMLUtils::addChild(doc, tradeNode, "AccumulatingFixedCoupons", accumulatingFixedCoupons_);
    XMLUtils::addChild(doc, tradeNode, "FloatingIndex", floatingIndex_);
    if (!floatingSpread_.empty())
        XMLUtils::addChild(doc, tradeNode, "FloatingSpread", floatingSpread_);
    XMLUtils::addChild(doc, tradeNode, "FloatingDayCountFraction", floatingDayCountFraction_.name());
    
    XMLUtils::addChild(doc, tradeNode, "FloatingLookback", floatingLookback_);
    if (!floatingRateCutoff_.empty())
        XMLUtils::addChild(doc, tradeNode, "FloatingRateCutoff", floatingRateCutoff_);
    XMLUtils::addChild(doc, tradeNode, "IsAveraged", isAveraged_);
    //if (!inArrears_.empty())
    //    XMLUtils::addChild(doc, tradeNode, "IsInArrears", inArrears_);
    XMLUtils::addChild(doc, tradeNode, "IncludeSpread", includeSpread_);

    return node;
}

} // namespace data
} // namespace ore
