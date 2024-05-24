/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/accumulator.hpp>
#include <ored/scripting/utilities.hpp>

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

// clang-format off

    static const std::string accumulator01_script =
      "            REQUIRE SIZE(FixingDates) == SIZE(SettlementDates);\n"
      "            REQUIRE KnockOutType == 3 OR KnockOutType == 4;\n"
      "            NUMBER Payoff, fix, d, r, Alive, currentNotional, Factor, ThisPayout, Fixing[SIZE(FixingDates)], dailyMult;\n"
      "            Alive = 1;\n"
      "            dailyMult = 1;\n"
      "            FOR d IN (1, SIZE(FixingDates), 1) DO\n"
      "                fix = Underlying(FixingDates[d]);\n"
      "                Fixing[d] = fix;\n"
      "\n"
      "                IF DailyFixingAmount == 1 THEN\n"
      "                  IF d == 1 THEN\n"
      "                     dailyMult = days(DailyFixingAmountDayCounter, StartDate, FixingDates[d]);\n"
      "                  ELSE\n"
      "                     dailyMult = days(DailyFixingAmountDayCounter, FixingDates[d-1], FixingDates[d]);\n"
      "                  END;\n"
      "                END;\n"
      "\n"
      "                IF AmericanKO == 1 THEN\n"
      "                  IF KnockOutType == 4 THEN\n"
      "                    IF FixingDates[d] >= StartDate THEN\n"
      "                       IF d == 1 OR FixingDates[d-1] <= StartDate THEN\n"
      "                          Alive = Alive * (1 - ABOVEPROB(Underlying, StartDate, FixingDates[d], KnockOutLevel));\n"
      "		              ELSE\n"
      "                          Alive = Alive * (1 - ABOVEPROB(Underlying, FixingDates[d-1], FixingDates[d], KnockOutLevel));\n"
      "		              END;\n"
      "                    END;\n"
      "                  ELSE\n"
      "                    IF FixingDates[d] >= StartDate THEN\n"
      "                       IF d == 1 OR FixingDates[d-1] <= StartDate THEN\n"
      "                          Alive = Alive * (1 - BELOWPROB(Underlying, StartDate, FixingDates[d], KnockOutLevel));\n"
      "		              ELSE\n"
      "                          Alive = Alive * (1 - BELOWPROB(Underlying, FixingDates[d-1], FixingDates[d], KnockOutLevel));\n"
      "		              END;\n"
      "                    END;\n"
      "                  END;\n"
      "                ELSE\n"
      "                  IF {KnockOutType == 4 AND fix >= KnockOutLevel} OR\n"
      "                     {KnockOutType == 3 AND fix <= KnockOutLevel} THEN\n"
      "                    Alive = 0;\n"
      "                  END;\n"
      "                END;\n"
      "\n"
      "                IF d <= GuaranteedFixings THEN\n"
      "                  Factor = 1;\n"
      "                ELSE\n"
      "                  Factor = Alive;\n"
      "                END;\n"
      "\n"
      "                FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
      "                  IF fix > RangeLowerBounds[r] AND fix <= RangeUpperBounds[r] THEN\n"
      "                  IF NakedOption == 1 THEN\n"
      "                    ThisPayout = abs(RangeLeverages[r]) * FixingAmount * dailyMult * max(0, OptionType * (fix - Strike[r])) * Factor;\n"
      "                  ELSE\n"
      "                    ThisPayout = RangeLeverages[r] * FixingAmount * dailyMult * (fix - Strike[r]) * Factor;\n"
      "                  END;\n"
      "                    IF d > GuaranteedFixings OR ThisPayout >= 0 THEN\n"
      "                      Payoff = Payoff + LOGPAY(ThisPayout, FixingDates[d], SettlementDates[d], PayCcy);\n"
      "                    END;\n"
      "                  END;\n"
      "                END;\n"
      "            END;\n"
      "            value = LongShort * Payoff;\n"
      "            currentNotional = FixingAmount * dailyMult * Strike[1];";


    static const std::string accumulator02_script =
        "            REQUIRE SIZE(ObservationDates) == SIZE(KnockOutSettlementDates);\n"
        "            REQUIRE SIZE(ObservationPeriodEndDates) == SIZE(SettlementDates);\n"
        "            REQUIRE SIZE(RangeUpperBounds) == SIZE(RangeLowerBounds);\n"
        "            REQUIRE SIZE(RangeUpperBounds) == SIZE(RangeLeverages);\n"
        "            REQUIRE ObservationPeriodEndDates[SIZE(ObservationPeriodEndDates)] >= ObservationDates[SIZE(ObservationDates)];\n"
        "            NUMBER Payoff, fix, d, dd, KnockedOut, currentNotional, Days[SIZE(RangeUpperBounds)], knockOutDays, Fixing[SIZE(ObservationPeriodEndDates)];\n"
        "            NUMBER currentPeriod, r, ThisPayout;\n"
        "            currentPeriod = 1;\n"
        "            FOR d IN (1, SIZE(ObservationDates), 1) DO\n"
        "              fix = Underlying(ObservationDates[d]);\n"
        "\n"
        "              knockOutDays = max(DATEINDEX(GuaranteedPeriodEndDate, ObservationDates, GT) - 1 - d, 0);\n"
        "\n"
        "              IF KnockedOut == 0 THEN\n"
        "                IF {KnockOutType == 4 AND fix >= KnockOutLevel} OR\n"
        "                   {KnockOutType == 3 AND fix <= KnockOutLevel} THEN\n"
        "                   KnockedOut = 1;\n"
        "                   Days[DefaultRange] = Days[DefaultRange] + knockOutDays;\n"
        "                   FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
        "                     IF NakedOption == 1 THEN\n"
        "                       ThisPayout = LongShort * FixingAmount * abs(RangeLeverages[r]) * Days[r] * max(0, OptionType * (fix - Strike) );\n"
        "                     ELSE\n"
        "                       ThisPayout = LongShort * FixingAmount * RangeLeverages[r] * Days[r] * ( fix - Strike );\n"
        "                     END;\n"
        "                     value = value + PAY( ThisPayout, ObservationDates[d], KnockOutSettlementDates[d], PayCcy );\n"
        "                   END;\n"
        "                END;\n"
        "              END;\n"
        "\n"
        "              IF KnockedOut == 0 THEN\n"
        "                FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
        "                  IF fix > RangeLowerBounds[r] AND fix <= RangeUpperBounds[r] THEN\n"
        "                    Days[r] = Days[r] + 1;\n"
        "                  END;\n"
        "                END;\n"
        "                IF ObservationDates[d] >= ObservationPeriodEndDates[currentPeriod] THEN\n"
        "                  FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
        "                    IF NakedOption == 1 THEN\n"
        "                      ThisPayout = LongShort * FixingAmount * abs(RangeLeverages[r]) * Days[r] * max(0, OptionType * (fix - Strike) );\n"
        "                    ELSE\n"
        "                      ThisPayout = LongShort * FixingAmount * RangeLeverages[r] * Days[r] * ( fix - Strike );\n"
        "                    END;\n"
        "                    value = value + LOGPAY( ThisPayout, ObservationDates[d], SettlementDates[currentPeriod], PayCcy );\n"
        "                  END;\n"
        "                END;\n"
        "              END;\n"
        "              IF ObservationDates[d] >= ObservationPeriodEndDates[currentPeriod] THEN\n"
        "                Fixing[currentPeriod] = fix;\n"
        "                currentPeriod = currentPeriod + 1;\n"
        "                FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
        "                  Days[r] = 0;\n"
        "                END;\n"
        "              END;\n"
        "            END;\n"
        "            currentNotional = FixingAmount *  Strike;";


    static const std::string accumulator02_script_fd =
        "            REQUIRE SIZE(ObservationDates) == SIZE(KnockOutSettlementDates);\n"
        "            REQUIRE SIZE(ObservationPeriodEndDates) == SIZE(SettlementDates);\n"
        "            REQUIRE SIZE(RangeUpperBounds) == SIZE(RangeLowerBounds);\n"
        "            REQUIRE SIZE(RangeUpperBounds) == SIZE(RangeLeverages);\n"
        "            REQUIRE ObservationPeriodEndDates[SIZE(ObservationPeriodEndDates)] >= ObservationDates[SIZE(ObservationDates)];\n"
        "\n"
        "            NUMBER currentPeriod, referencePayout, fix, d, r, dd, currentNotional, Fixing[SIZE(ObservationPeriodEndDates)], ThisPayout;\n"
        "\n"
        "            currentPeriod = DATEINDEX(ObservationDates[SIZE(ObservationDates)], ObservationPeriodEndDates, GEQ);\n"
        "            IF NakedOption == 1 THEN\n"
        "              ThisPayout = max(0, OptionType * (Underlying(ObservationDates[SIZE(ObservationDates)]) - Strike) );\n"
        "            ELSE\n"
        "              ThisPayout = Underlying(ObservationDates[SIZE(ObservationDates)]) - Strike;\n"
        "            END;\n"
        "            referencePayout = PAY( LongShort * FixingAmount * ThisPayout, ObservationDates[SIZE(ObservationDates)],\n"
        "                                   SettlementDates[currentPeriod], PayCcy );\n"
        "            value = 0 * referencePayout;\n"
        "\n"
        "            FOR d IN (SIZE(ObservationDates), 1, -1) DO\n"
        "\n"
        "              IF ObservationDates[d] >= TODAY THEN\n"
        "                value = NPV(value, ObservationDates[d]);\n"
        "                referencePayout = NPV(referencePayout, ObservationDates[d]);\n"
        "              ELSE\n"
        "                value = NPV(value, TODAY);\n"
        "                referencePayout = NPV(referencePayout, TODAY);\n"
	    "              END;\n"
        "\n"
        "              fix = Underlying(ObservationDates[d]);\n"
        "              IF NakedOption == 1 THEN\n"
        "                ThisPayout = LongShort * FixingAmount * max(0, OptionType * (fix - Strike));\n"
        "              ELSE\n"
        "                ThisPayout = LongShort * FixingAmount * (fix - Strike);\n"
        "              END;\n"
        "\n"
        "              IF d > 1 AND currentPeriod > 0 AND ObservationDates[d-1] < ObservationPeriodEndDates[currentPeriod] THEN\n"
        "                referencePayout = PAY( ThisPayout, ObservationDates[d], SettlementDates[currentPeriod], PayCcy );\n"
        "                Fixing[currentPeriod] = fix;\n"
        "                currentPeriod = currentPeriod - 1;\n"
        "              END;\n"
        "\n"
        "              IF {KnockOutType == 4 AND fix >= KnockOutLevel} OR\n"
        "                 {KnockOutType == 3 AND fix <= KnockOutLevel} THEN\n"
        "                IF NakedOption == 1 THEN\n"
        "                  ThisPayout = ThisPayout * abs(RangeLeverages[DefaultRange]);\n"
        "                ELSE\n"
        "                  ThisPayout = ThisPayout * RangeLeverages[DefaultRange];\n"
        "                END;\n"
        "                referencePayout = PAY( ThisPayout, ObservationDates[d], KnockOutSettlementDates[d], PayCcy );\n"
        "                value = referencePayout * max(DATEINDEX(GuaranteedPeriodEndDate, ObservationDates, GT) - 1 - d, 0);\n"
        "              ELSE\n"
        "                FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
        "                  IF fix > RangeLowerBounds[r] AND fix <= RangeUpperBounds[r] THEN\n"
        "                    IF NakedOption == 1 THEN\n"
        "                      value = value + abs(RangeLeverages[r]) * referencePayout;\n"
        "                    ELSE\n"
        "                      value = value + RangeLeverages[r] * referencePayout;\n"
        "                    END;\n"
        "                  END;\n"
        "                END;\n"
        "              END;\n"
        "            END;\n"
        "            currentNotional = FixingAmount *  Strike;";

// clang-format on

void Accumulator::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    enum class AccumulatorScript { Accumulator01, Accumulator02 };
    AccumulatorScript scriptToUse;
    if (!pricingDates_.hasData()) {
        scriptToUse = AccumulatorScript::Accumulator01;
        DLOG("building scripted trade wrapper using (internal) script Accumulator01");
    } else {
        scriptToUse = AccumulatorScript::Accumulator02;
        DLOG("building scripted trade wrapper using (internal) script Accumulator02");
    }

    Real leverageMultiplier;
    if (optionData_.payoffType() == "Accumulator")
        leverageMultiplier = 1.0;
    else if (optionData_.payoffType() == "Decumulator")
        leverageMultiplier = -1.0;
    else {
        QL_FAIL("invalid payoff type, expected Accumulator or Decumulator");
    }

    if (strike_.currency().empty())
        strike_.setCurrency(currency_);
    Real globalStrike = strike_.value();

    QL_REQUIRE(scriptToUse == AccumulatorScript::Accumulator01 || globalStrike != Null<Real>(),
               "For accumulator type 02 a global strike must be given");

    std::vector<std::string> rangeUpperBounds, rangeLowerBounds, rangeLeverages, rangeStrikes;
    for (auto const& r : rangeBounds_) {
        rangeLowerBounds.push_back(
            boost::lexical_cast<std::string>(r.from() == Null<Real>() ? -QL_MAX_REAL : r.from()));
        rangeUpperBounds.push_back(boost::lexical_cast<std::string>(r.to() == Null<Real>() ? QL_MAX_REAL : r.to()));
        rangeLeverages.push_back(
            boost::lexical_cast<std::string>(leverageMultiplier * (r.leverage() == Null<Real>() ? 1.0 : r.leverage())));
        if (scriptToUse == AccumulatorScript::Accumulator01) {
            if (r.strike() != Null<Real>()) {
                rangeStrikes.push_back(boost::lexical_cast<std::string>(r.strike()));
            } else if (r.strikeAdjustment() != Null<Real>() && globalStrike != Null<Real>()) {
                rangeStrikes.push_back(boost::lexical_cast<std::string>(globalStrike + r.strikeAdjustment()));
            } else if (globalStrike != Null<Real>()) {
                rangeStrikes.push_back(boost::lexical_cast<std::string>(globalStrike));
            } else {
                QL_FAIL(
                    "insufficient strike information: either a global strike or a range-specific strike must be given");
            }
        }
    }

    bool initPositive = false;
    for (Size i = 0; i < rangeLeverages.size(); i++) {
        Real rl = parseReal(rangeLeverages.at(i));
        if (i == 0)
            initPositive = rl >= 0.0;
        else {
            bool nextPositive = rl >= 0.0;
            QL_REQUIRE(nextPositive == initPositive, "Range leverages must all have the same sign.");
        }
    }
    numbers_.emplace_back("Number", "NakedOption", nakedOption_ ? "1" : "-1");
    if (nakedOption_)
        numbers_.emplace_back("Number", "OptionType", initPositive ? "1" : "-1");

    numbers_.emplace_back("Number", "RangeLowerBounds", rangeLowerBounds);
    numbers_.emplace_back("Number", "RangeUpperBounds", rangeUpperBounds);
    numbers_.emplace_back("Number", "RangeLeverages", rangeLeverages);
    if (scriptToUse == AccumulatorScript::Accumulator02) {
        numbers_.emplace_back("Number", "DefaultRange", "1");
    }

    numbers_.emplace_back("Number", "FixingAmount", fixingAmount_);
    numbers_.emplace_back("Number", "LongShort",
                          parsePositionType(optionData_.longShort()) == Position::Long ? "1" : "-1");

    currencies_.emplace_back("Currency", "PayCcy", currency_);
    if (scriptToUse == AccumulatorScript::Accumulator01) {
        numbers_.emplace_back("Number", "Strike", rangeStrikes);
    } else {
        numbers_.emplace_back("Number", "Strike", boost::lexical_cast<std::string>(globalStrike));
    }

    if (scriptToUse == AccumulatorScript::Accumulator01) {
        events_.emplace_back("FixingDates", observationDates_);
        if(settlementDates_.hasData()) {
            events_.emplace_back("SettlementDates", settlementDates_);
        } else {
            events_.emplace_back("SettlementDates", "FixingDates", settlementLag_.empty() ? "0D" : settlementLag_,
                                 settlementCalendar_.empty() ? "NullCalendar" : settlementCalendar_,
                                 settlementConvention_.empty() ? "F" : settlementConvention_);
        }
    } else {
        events_.emplace_back("ObservationDates", observationDates_);
        events_.emplace_back("KnockOutSettlementDates", "ObservationDates",
                             settlementLag_.empty() ? "0D" : settlementLag_,
                             settlementCalendar_.empty() ? "NullCalendar" : settlementCalendar_,
                             settlementConvention_.empty() ? "F" : settlementConvention_);
        events_.emplace_back("ObservationPeriodEndDates", pricingDates_);
        if(settlementDates_.hasData()) {
            events_.emplace_back("SettlementDates", settlementDates_);
        } else {
            events_.emplace_back("SettlementDates", "ObservationPeriodEndDates",
                                 settlementLag_.empty() ? "0D" : settlementLag_,
                                 settlementCalendar_.empty() ? "NullCalendar" : settlementCalendar_,
                                 settlementConvention_.empty() ? "F" : settlementConvention_);
        }
    }

    std::string knockOutLevel = boost::lexical_cast<std::string>(QL_MAX_REAL), knockOutType = "4",
                guaranteedFixings = "0";
    bool barrierSet = false;
    bool americanKO = false;
    for (auto const& b : barriers_) {
        QL_REQUIRE(b.style().empty() || b.style() == "European" || b.style() == "American",
                   "expected barrier style American or European, got " << b.style());
        QL_REQUIRE(b.style() != "European" || scriptToUse == AccumulatorScript::Accumulator01,
                   "European barrier style not allowed if PricingDates are given (Accumulator02 script variant)");
        if (b.type() == "UpAndOut" && !b.levels().empty()) {
            knockOutType = "4";
            knockOutLevel = boost::lexical_cast<std::string>(b.levels().front().value());
            QL_REQUIRE(!barrierSet, "multiple barrier definitions");
            barrierSet = true;
            americanKO = !(b.style() == "European");
        } else if (b.type() == "DownAndOut" && !b.levels().empty()) {
            knockOutType = "3";
            knockOutLevel = boost::lexical_cast<std::string>(b.levels().front().value());
            QL_REQUIRE(!barrierSet, "multiple barrier definitions");
            barrierSet = true;
            americanKO = !(b.style() == "European");
        } else if (b.type() == "FixingFloor" && !b.levels().empty()) {
            guaranteedFixings = boost::lexical_cast<std::string>(b.levels().front().value());
        } else
            QL_FAIL("invalid barrier definition, expected UpAndOut, DownAndOut, FixingFloor (with exactly one level)");
    }

    numbers_.emplace_back("Number", "KnockOutLevel", knockOutLevel);
    numbers_.emplace_back("Number", "KnockOutType", knockOutType);

    if (scriptToUse == AccumulatorScript::Accumulator01) {
        QL_REQUIRE((!americanKO && !dailyFixingAmount_) || !startDate_.empty(),
                   "For american knock out or when using a daily fixing amount StartDate must be given.");
        events_.emplace_back("StartDate", startDate_);
        numbers_.emplace_back("Number", "AmericanKO", americanKO ? "1" : "-1");
        numbers_.emplace_back("Number", "GuaranteedFixings", guaranteedFixings);
        numbers_.emplace_back("Number", "DailyFixingAmount", dailyFixingAmount_ ? "1" : "-1");
        daycounters_.emplace_back("Daycounter", "DailyFixingAmountDayCounter", "ACT/ACT.ISDA");

    } else {
        Schedule pd = makeSchedule(pricingDates_);
        Size gf = parseInteger(guaranteedFixings);
        QL_REQUIRE(gf <= pd.size(),
                   "guaranteed fixings (" << gf << ") > pricing dates schedule size (" << pd.size() << ")");
        Date gpend = gf == 0 ? Date::minDate() : pd.date(gf - 1);
        events_.emplace_back("GuaranteedPeriodEndDate", ore::data::to_string(gpend));
    }

    // set product tag

    productTag_ = scriptToUse == AccumulatorScript::Accumulator01 ? "SingleAssetOptionCG({AssetClass})"
                                                                  : "SingleAssetOptionBwd({AssetClass})";

    // set script

    if (scriptToUse == AccumulatorScript::Accumulator01) {
        script_[""] = ScriptedTradeScriptData(
            accumulator01_script, "value",
            {{"currentNotional", "currentNotional"},
             {"notionalCurrency", "PayCcy"},
             {"Alive", "Alive"},
             {"Fixing", "Fixing"}},
            {}, {}, {ScriptedTradeScriptData::CalibrationData("Underlying", {"Strike", "KnockOutLevel"})});
    } else {
        script_[""] = ScriptedTradeScriptData(
            accumulator02_script, "value",
            {{"currentNotional", "currentNotional"},
             {"notionalCurrency", "PayCcy"},
             {"KnockedOut", "KnockedOut"},
             {"Fixing", "Fixing"}},
            {}, {}, {ScriptedTradeScriptData::CalibrationData("Underlying", {"Strike", "KnockOutLevel"})});
        script_["FD"] = ScriptedTradeScriptData(
            accumulator02_script_fd, "value",
            {{"currentNotional", "currentNotional"}, {"notionalCurrency", "PayCcy"}, {"Fixing", "Fixing"}}, {}, {},
            {ScriptedTradeScriptData::CalibrationData("Underlying", {"Strike", "KnockOutLevel"})});
    }

    // build trade

    ScriptedTrade::build(factory);
}

void Accumulator::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // ISDA taxonomy, asset class set in the base class build
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
        additionalData_["isdaSubProduct"] = string("Target");
    } else {
        WLOG("ISDA taxonomy incomplete for trade " << id());
    }

    additionalData_["isdaTransaction"] = string("");
}

void Accumulator::initIndices() { indices_.emplace_back("Index", "Underlying", scriptedIndexName(underlying_)); }

void Accumulator::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(dataNode, tradeType() + "Data node not found");
    fixingAmount_ = XMLUtils::getChildValue(dataNode, "FixingAmount", true);
    dailyFixingAmount_ = XMLUtils::getChildValueAsBool(dataNode, "DailyFixingAmount", false, false);

    currency_ = XMLUtils::getChildValue(dataNode, "Currency", false);
    strike_.fromXML(dataNode, false, false);

    XMLNode* tmp = XMLUtils::getChildNode(dataNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(dataNode, "Name");
    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();

    optionData_.fromXML(XMLUtils::getChildNode(dataNode, "OptionData"));
    startDate_ = XMLUtils::getChildValue(dataNode, "StartDate", false);
    observationDates_.fromXML(XMLUtils::getChildNode(dataNode, "ObservationDates"));
    if (XMLNode* n = XMLUtils::getChildNode(dataNode, "PricingDates"))
        pricingDates_.fromXML(n);
    if (XMLNode* n = XMLUtils::getChildNode(dataNode, "SettlementDates"))
        settlementDates_.fromXML(n);
    settlementLag_ = XMLUtils::getChildValue(dataNode, "SettlementLag", false);
    settlementCalendar_ = XMLUtils::getChildValue(dataNode, "SettlementCalendar", false);
    settlementConvention_ = XMLUtils::getChildValue(dataNode, "SettlementConvention", false);
    nakedOption_ = XMLUtils::getChildValueAsBool(dataNode, "NakedOption", false, false);
    auto rangeBoundsNode = XMLUtils::getChildNode(dataNode, "RangeBounds");
    QL_REQUIRE(rangeBoundsNode, "No RangeBounds node");
    auto rangeBounds = XMLUtils::getChildrenNodes(rangeBoundsNode, "RangeBound");
    for (auto const& n : rangeBounds) {
        rangeBounds_.push_back(RangeBound());
        rangeBounds_.back().fromXML(n);
    }
    auto barriersNode = XMLUtils::getChildNode(dataNode, "Barriers");
    QL_REQUIRE(barriersNode, "No Barriers node");
    auto barriers = XMLUtils::getChildrenNodes(barriersNode, "BarrierData");
    for (auto const& n : barriers) {
        barriers_.push_back(BarrierData());
        barriers_.back().fromXML(n);
    }
    initIndices();
}

XMLNode* Accumulator::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, dataNode);
    XMLUtils::addChild(doc, dataNode, "FixingAmount", fixingAmount_);
    XMLUtils::addChild(doc, dataNode, "DailyFixingAmount", dailyFixingAmount_);
    XMLUtils::addChild(doc, dataNode, "Currency", currency_);

    if (strike_.value() != Null<Real>()) {
        XMLUtils::appendNode(dataNode, strike_.toXML(doc));
    }

    XMLUtils::appendNode(dataNode, underlying_->toXML(doc));
    XMLUtils::appendNode(dataNode, optionData_.toXML(doc));
    if (!startDate_.empty())
        XMLUtils::addChild(doc, dataNode, "StartDate", startDate_);
    XMLNode* tmp = observationDates_.toXML(doc);
    XMLUtils::setNodeName(doc, tmp, "ObservationDates");
    XMLUtils::appendNode(dataNode, tmp);
    if (pricingDates_.hasData()) {
        XMLNode* tmp = pricingDates_.toXML(doc);
        XMLUtils::setNodeName(doc, tmp, "PricingDates");
        XMLUtils::appendNode(dataNode, tmp);
    }
    if (settlementDates_.hasData()) {
        XMLNode* tmp = settlementDates_.toXML(doc);
        XMLUtils::setNodeName(doc, tmp, "SettlementDates");
        XMLUtils::appendNode(dataNode, tmp);
    }
    if (!settlementLag_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementLag", settlementLag_);
    if (!settlementCalendar_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementCalendar", settlementCalendar_);
    if (!settlementConvention_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementConvention", settlementConvention_);
    XMLUtils::addChild(doc, dataNode, "NakedOption", nakedOption_);
    XMLNode* rangeBounds = doc.allocNode("RangeBounds");
    for (auto& n : rangeBounds_) {
        XMLUtils::appendNode(rangeBounds, n.toXML(doc));
    }
    XMLUtils::appendNode(dataNode, rangeBounds);
    XMLNode* barriers = doc.allocNode("Barriers");
    for (auto& n : barriers_) {
        XMLUtils::appendNode(barriers, n.toXML(doc));
    }
    XMLUtils::appendNode(dataNode, barriers);
    return node;
}

} // namespace data
} // namespace ore
