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

#include <ored/portfolio/tarf.hpp>
#include <ored/scripting/utilities.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

using namespace QuantExt;
    
// clang-format off

    static const std::string tarf_script_regular =
        "REQUIRE FixingAmount > 0;\n"
        "NUMBER Payoff, d, r, ri, PnL, tmpPnL, wasTriggered, AccProfit, Hits, currentNotional;\n"
        "NUMBER Fixing[SIZE(FixingDates)], Triggered[SIZE(FixingDates)];\n"
        "FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
        "  REQUIRE RangeLowerBounds[r] <= RangeUpperBounds[r];\n"
        "  REQUIRE RangeStrikes[r] >= 0;\n"
        "END;\n"
        "FOR d IN (1, SIZE(FixingDates), 1) DO\n"
        "  Fixing[d] = Underlying(FixingDates[d]);\n"
        "  tmpPnL = 0;\n"
        "  FOR r IN (1, NumberOfRangeBounds, 1) DO\n"
        "    ri = (d - 1) * NumberOfRangeBounds + r;\n"
        "    IF Fixing[d] > RangeLowerBounds[ri] AND Fixing[d] <= RangeUpperBounds[ri] THEN\n"
        "      tmpPnL = tmpPnL + RangeLeverages[ri] * FixingAmount * (Fixing[d] - RangeStrikes[ri]);\n"
        "    END;\n"
        "  END;\n"
        "  IF wasTriggered != 1 THEN\n"
        "    PnL = tmpPnL;\n"
        "    IF PnL >= 0 THEN\n"
        "      AccProfit = AccProfit + PnL;\n"
        "      Hits = Hits + 1;\n"
        "    END;\n"
        "    IF {KnockOutProfitEvents > 0 AND Hits >= KnockOutProfitEvents} OR\n"
        "       {KnockOutProfitAmount > 0 AND AccProfit >= KnockOutProfitAmount} THEN\n"
        "      wasTriggered = 1;\n"
        "      Triggered[d] = 1;\n"
        "      IF TargetType == 0 THEN\n"
        "        Payoff = Payoff + LOGPAY(TargetAmount - (AccProfit - PnL), FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "      END;\n"
        "      IF TargetType == 1 THEN\n"
        "        Payoff = Payoff + LOGPAY(PnL, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "      END;\n"
        "    ELSE\n"
        "      Payoff = Payoff + LOGPAY(PnL, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "    END;\n"
        "  END;\n"
        "END;\n"
        "value = LongShort * Payoff;\n"
        "currentNotional = FixingAmount * RangeStrikes[1];";

    static const std::string tarf_script_regular_amc =
        "REQUIRE FixingAmount > 0;\n"
        "NUMBER Payoff, d, r, ri, PnL, tmpPnL, wasTriggered, AccProfit[SIZE(FixingDates)], Hits[SIZE(FixingDates)], currentNotional;\n"
        "NUMBER Fixing[SIZE(FixingDates)], Triggered[SIZE(FixingDates)];\n"
        "NUMBER a, s, nthPayoff[SIZE(FixingDates)], bwdPayoff, _AMC_NPV[SIZE(_AMC_SimDates)];\n"
        "FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
        "  REQUIRE RangeLowerBounds[r] <= RangeUpperBounds[r];\n"
        "  REQUIRE RangeStrikes[r] >= 0;\n"
        "END;\n"
        "FOR d IN (1, SIZE(FixingDates), 1) DO\n"
        "  Fixing[d] = Underlying(FixingDates[d]);\n"
        "  tmpPnL = 0;\n"
        "  FOR r IN (1, NumberOfRangeBounds, 1) DO\n"
        "    ri = (d - 1) * NumberOfRangeBounds + r;\n"
        "    IF Fixing[d] > RangeLowerBounds[ri] AND Fixing[d] <= RangeUpperBounds[ri] THEN\n"
        "      tmpPnL = tmpPnL + RangeLeverages[ri] * FixingAmount * (Fixing[d] - RangeStrikes[ri]);\n"
        "    END;\n"
        "  END;\n"
        "  IF wasTriggered != 1 THEN\n"
        "    PnL = tmpPnL;\n"
        "    IF PnL >= 0 THEN\n"
        "      AccProfit[d] = AccProfit[d] + PnL;\n"
        "      Hits[d] = Hits[d] + 1;\n"
        "    END;\n"
        "    IF {KnockOutProfitEvents > 0 AND Hits[d] >= KnockOutProfitEvents} OR\n"
        "       {KnockOutProfitAmount > 0 AND AccProfit[d] >= KnockOutProfitAmount} THEN\n"
        "      wasTriggered = 1;\n"
        "      Triggered[d] = 1;\n"
        "      IF TargetType == 0 THEN\n"
        "        Payoff = Payoff + LOGPAY(TargetAmount - (AccProfit[d] - PnL), FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "        nthPayoff[d] = PAY(TargetAmount - (AccProfit[d] - PnL), FixingDates[d], SettlementDates[d], PayCcy);\n"
        "        AccProfit[d] = TargetAmount;\n"
        "      END;\n"
        "      IF TargetType == 1 THEN\n"
        "        Payoff = Payoff + LOGPAY(PnL, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "        nthPayoff[d] = PAY(PnL, FixingDates[d], SettlementDates[d], PayCcy);\n"
        "      END;\n"
        "    ELSE\n"
        "      Payoff = Payoff + LOGPAY(PnL, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "      nthPayoff[d] = PAY(PnL, FixingDates[d], SettlementDates[d], PayCcy);\n"
        "    END;\n"
        "  END;\n"
        "  IF d < SIZE(FixingDates) THEN\n"
        "    AccProfit[d + 1] = AccProfit[d];\n"
        "    Hits[d + 1] = Hits[d];\n"
        "  END;\n"
        "END;\n"
        "FOR a IN (SIZE(FixingAndSimDates), 1, -1) DO\n"
        "  s = DATEINDEX(FixingAndSimDates[a], _AMC_SimDates, EQ);\n"
        "  d = DATEINDEX(FixingAndSimDates[a], FixingDates, GT);\n"
        "  IF s > 0 THEN\n"
        "    IF d > 1 THEN\n"
        // including Hits[d-1] or AccProfit[d-1] in the regression is not stable
        "      _AMC_NPV[s] = LongShort * NPVMEM( bwdPayoff, _AMC_SimDates[s], a);\n"
        "    ELSE\n"
        "      _AMC_NPV[s] = LongShort * NPVMEM( bwdPayoff, _AMC_SimDates[s], a);\n"
        "    END;\n"
        "  END;\n"
        "  d = DATEINDEX(FixingAndSimDates[a], FixingDates, EQ);\n"
        "  IF d > 0 THEN\n"
        "    bwdPayoff = bwdPayoff + nthPayoff[d];\n"
        "  END;\n"
        "END;\n"
        "value = LongShort * Payoff;\n"
        "currentNotional = FixingAmount * RangeStrikes[1];";

    static const std::string tarf_script_points =
        "REQUIRE FixingAmount > 0;\n"
        "NUMBER Payoff, d, r, ri, PnL, tmpPnL, PnLPoints, tmpPnLPoints, wasTriggered, AccProfitPoints, currentNotional;\n"
        "NUMBER Fixing[SIZE(FixingDates)], Triggered[SIZE(FixingDates)];\n"
        "FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
        "  REQUIRE RangeLowerBounds[r] <= RangeUpperBounds[r];\n"
        "  REQUIRE RangeStrikes[r] >= 0;\n"
        "END;\n"
        "FOR d IN (1, SIZE(FixingDates), 1) DO\n"
        "  Fixing[d] = Underlying(FixingDates[d]);\n"
        "  tmpPnL = 0;\n"
        "  tmpPnLPoints = 0;\n"
        "  FOR r IN (1, NumberOfRangeBounds, 1) DO\n"
        "    ri = (d - 1) * NumberOfRangeBounds + r;\n"
        "    IF Fixing[d] > RangeLowerBounds[ri] AND Fixing[d] <= RangeUpperBounds[ri] THEN\n"
        "      tmpPnL = tmpPnL + RangeLeverages[ri] * FixingAmount * (Fixing[d] - RangeStrikes[ri]);\n"
        "      tmpPnLPoints = tmpPnLPoints + RangeLeverages[ri] / abs(RangeLeverages[ri]) * (Fixing[d] - RangeStrikes[ri]);\n"
        "    END;\n"
        "  END;\n"
        "  IF wasTriggered != 1 THEN\n"
        "    PnL = tmpPnL;\n"
        "    PnLPoints = tmpPnLPoints;\n"
        "    IF PnLPoints >= 0 THEN\n"
        "      AccProfitPoints = AccProfitPoints + PnLPoints;\n"
        "    END;\n"
        "    IF KnockOutProfitAmountPoints > 0 AND AccProfitPoints >= KnockOutProfitAmountPoints THEN\n"
        "      wasTriggered = 1;\n"
        "      Triggered[d] = 1;\n"
        "      IF TargetType == 0 THEN\n"
        "        Payoff = Payoff + LOGPAY((TargetPoints - (AccProfitPoints - PnLPoints)) * PnL / PnLPoints, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "      END;\n"
        "      IF TargetType == 1 THEN\n"
        "        Payoff = Payoff + LOGPAY(PnL, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "      END;\n"
        "    ELSE\n"
        "      Payoff = Payoff + LOGPAY(PnL, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "    END;\n"
        "  END;\n"
        "END;\n"
        "value = LongShort * Payoff;\n"
        "currentNotional = FixingAmount * RangeStrikes[1];";

    static const std::string tarf_script_points_amc =
        "REQUIRE FixingAmount > 0;\n"
        "NUMBER Payoff, d, r, ri, PnL, tmpPnL, PnLPoints, tmpPnLPoints, wasTriggered, AccProfitPoints[SIZE(FixingDates)], currentNotional;\n"
        "NUMBER Fixing[SIZE(FixingDates)], Triggered[SIZE(FixingDates)];\n"
        "NUMBER a, s, nthPayoff[SIZE(FixingDates)], bwdPayoff, _AMC_NPV[SIZE(_AMC_SimDates)];\n"
        "FOR r IN (1, SIZE(RangeUpperBounds), 1) DO\n"
        "  REQUIRE RangeLowerBounds[r] <= RangeUpperBounds[r];\n"
        "  REQUIRE RangeStrikes[r] >= 0;\n"
        "END;\n"
        "FOR d IN (1, SIZE(FixingDates), 1) DO\n"
        "  Fixing[d] = Underlying(FixingDates[d]);\n"
        "  tmpPnL = 0;\n"
        "  tmpPnLPoints = 0;\n"
        "  FOR r IN (1, NumberOfRangeBounds, 1) DO\n"
        "    ri = (d - 1) * NumberOfRangeBounds + r;\n"
        "    IF Fixing[d] > RangeLowerBounds[ri] AND Fixing[d] <= RangeUpperBounds[ri] THEN\n"
        "      tmpPnL = tmpPnL + RangeLeverages[ri] * FixingAmount * (Fixing[d] - RangeStrikes[ri]);\n"
        "      tmpPnLPoints = tmpPnLPoints + RangeLeverages[ri] / abs(RangeLeverages[ri]) * (Fixing[d] - RangeStrikes[ri]);\n"
        "    END;\n"
        "  END;\n"
        "  IF wasTriggered != 1 THEN\n"
        "    PnL = tmpPnL;\n"
        "    PnLPoints = tmpPnLPoints;\n"
        "    IF PnLPoints >= 0 THEN\n"
        "      AccProfitPoints[d] = AccProfitPoints[d] + PnLPoints;\n"
        "    END;\n"
        "    IF KnockOutProfitAmountPoints > 0 AND AccProfitPoints[d] >= KnockOutProfitAmountPoints THEN\n"
        "      wasTriggered = 1;\n"
        "      Triggered[d] = 1;\n"
        "      IF TargetType == 0 THEN\n"
        "        Payoff = Payoff + LOGPAY((TargetPoints - (AccProfitPoints[d] - PnLPoints)) * PnL / PnLPoints, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "        nthPayoff[d] = PAY((TargetPoints - (AccProfitPoints[d] - PnLPoints)) * PnL / PnLPoints, FixingDates[d], SettlementDates[d], PayCcy);\n"
        "        AccProfitPoints[d] = TargetPoints;\n"
        "      END;\n"
        "      IF TargetType == 1 THEN\n"
        "        Payoff = Payoff + LOGPAY(PnL, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "        nthPayoff[d] = PAY(PnL, FixingDates[d], SettlementDates[d], PayCcy);\n"
        "      END;\n"
        "    ELSE\n"
        "      Payoff = Payoff + LOGPAY(PnL, FixingDates[d], SettlementDates[d], PayCcy, 0, Cashflow);\n"
        "        nthPayoff[d] = PAY(PnL, FixingDates[d], SettlementDates[d], PayCcy);\n"
        "    END;\n"
        "  END;\n"
        "  IF d < SIZE(FixingDates) THEN\n"
        "    AccProfitPoints[d + 1] = AccProfitPoints[d];\n"
        "  END;\n"
        "END;\n"
        "FOR a IN (SIZE(FixingAndSimDates), 1, -1) DO\n"
        "  s = DATEINDEX(FixingAndSimDates[a], _AMC_SimDates, EQ);\n"
        "  d = DATEINDEX(FixingAndSimDates[a], FixingDates, GT);\n"
        "  IF s > 0 THEN\n"
        "    IF d > 1 THEN\n"
        "      _AMC_NPV[s] = LongShort * NPVMEM( bwdPayoff, _AMC_SimDates[s], a);\n"
        "    ELSE\n"
        "      _AMC_NPV[s] = LongShort * NPVMEM( bwdPayoff, _AMC_SimDates[s], a);\n"
        "    END;\n"
        "  END;\n"
        "  d = DATEINDEX(FixingAndSimDates[a], FixingDates, EQ);\n"
        "  IF d > 0 THEN\n"
        "    bwdPayoff = bwdPayoff + nthPayoff[d];\n"
        "  END;\n"
        "END;\n"
        "value = LongShort * Payoff;\n"
        "currentNotional = FixingAmount * RangeStrikes[1];";
// clang-format on

TaRF::TaRF(const std::string& currency, const std::string& fixingAmount, const std::string& targetAmount,
           const std::string& targetPoints, const std::vector<std::string>& strikes,
           const std::vector<std::string>& strikeDates, const QuantLib::ext::shared_ptr<Underlying>& underlying,
           const ScheduleData& fixingDates, const std::string& settlementLag, const std::string& settlementCalendar,
           const std::string& settlementConvention, OptionData& optionData,
           const std::vector<std::vector<RangeBound>>& rangeBoundSet,
           const std::vector<std::string>& rangeBoundSetDates, const std::vector<BarrierData>& barriers)
    : currency_(currency), fixingAmount_(fixingAmount), targetAmount_(targetAmount), targetPoints_(targetPoints),
      strikes_(strikes), underlying_(underlying), fixingDates_(fixingDates), settlementLag_(settlementLag),
      settlementCalendar_(settlementCalendar), settlementConvention_(settlementConvention), optionData_(optionData),
      rangeBoundSet_(rangeBoundSet), barriers_(barriers) {
    QL_REQUIRE(strikes_.size() == strikeDates_.size(), "TaRF: strike size (" << strikes_.size()
                                                                             << ") does not match strikeDates size ("
                                                                             << strikeDates_.size() << ")");
    QL_REQUIRE(rangeBoundSet_.size() == rangeBoundSetDates_.size(),
               "TaRF: rangeBoundSet size (" << rangeBoundSet_.size() << ") does not match rangeBoundSetDates size ("
                                            << rangeBoundSetDates_.size());
    QL_REQUIRE(targetAmount_.empty() || targetPoints_.empty(),
               "TaRF: both ttargetAmount, targetPoints is populated, only one is allowed");
    initIndices();
}

void TaRF::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // 1 inits

    clear();
    initIndices();

    // 2 build rangeBounds and strikes vectors according to fixing date schedule

    std::vector<QuantLib::Date> fixingSchedulePlusInf = makeSchedule(fixingDates_).dates();
    fixingSchedulePlusInf.push_back(Date::maxDate());
    std::vector<std::vector<RangeBound>> rangeBoundSet = buildScheduledVectorNormalised<std::vector<RangeBound>>(
        rangeBoundSet_, rangeBoundSetDates_, fixingSchedulePlusInf, std::vector<RangeBound>());
    std::vector<std::string> strikes =
        buildScheduledVectorNormalised<std::string>(strikes_, strikeDates_, fixingSchedulePlusInf, "");
    Size numberOfRangeBounds = Null<Size>();

    QL_REQUIRE(rangeBoundSet.size() == fixingSchedulePlusInf.size() - 1,
               "RangeBoundSet has " << rangeBoundSet.size() << " elements for " << (fixingSchedulePlusInf.size() - 1)
                                    << " fixing dates.");
    QL_REQUIRE(strikes.size() == fixingSchedulePlusInf.size() - 1, "Strikes has " << strikes.size() << " elements for "
                                                                                  << (fixingSchedulePlusInf.size() - 1)
                                                                                  << " fixing dates.");

    // 3 populate range-bound data (per fixing date)

    std::vector<std::string> rangeStrikes, rangeUpperBounds, rangeLowerBounds, rangeLeverages;
    for (Size i = 0; i < rangeBoundSet.size(); ++i) {

        for (auto const& r : rangeBoundSet[i]) {
            if (r.strike() != Null<Real>())
                rangeStrikes.push_back(boost::lexical_cast<std::string>(r.strike()));
            else if (r.strikeAdjustment() != Null<Real>() && !strikes[i].empty())
                rangeStrikes.push_back(boost::lexical_cast<std::string>(r.strikeAdjustment() + parseReal(strikes[i])));
            else if (!strikes[i].empty())
                rangeStrikes.push_back(boost::lexical_cast<std::string>(parseReal(strikes[i])));
            else {
                QL_FAIL("insufficient strike information");
            }
            rangeLowerBounds.push_back(
                boost::lexical_cast<std::string>(r.from() == Null<Real>() ? -QL_MAX_REAL : r.from()));
            rangeUpperBounds.push_back(boost::lexical_cast<std::string>(r.to() == Null<Real>() ? QL_MAX_REAL : r.to()));
            rangeLeverages.push_back(
                boost::lexical_cast<std::string>(r.leverage() == Null<Real>() ? 1.0 : r.leverage()));
        }

        if (i == 0)
            numberOfRangeBounds = rangeLowerBounds.size();
        else {
            QL_REQUIRE(
                numberOfRangeBounds * (i + 1) == rangeLowerBounds.size(),
                "Each RangeBounds subnode (under RangeBoundSets) must contain the same number of RangeBound nodes");
        }
    }

    QL_REQUIRE(numberOfRangeBounds != Null<Size>(), "internal error: numberOfRangeBounds not set.");

    // 4 set parameters

    numbers_.emplace_back("Number", "NumberOfRangeBounds", std::to_string(numberOfRangeBounds));
    numbers_.emplace_back("Number", "RangeStrikes", rangeStrikes);
    numbers_.emplace_back("Number", "RangeLowerBounds", rangeLowerBounds);
    numbers_.emplace_back("Number", "RangeUpperBounds", rangeUpperBounds);
    numbers_.emplace_back("Number", "RangeLeverages", rangeLeverages);

    numbers_.emplace_back("Number", "FixingAmount", fixingAmount_);
    numbers_.emplace_back("Number", "LongShort",
                          parsePositionType(optionData_.longShort()) == Position::Long ? "1" : "-1");

    currencies_.emplace_back("Currency", "PayCcy", currency_);

    events_.emplace_back("FixingDates", fixingDates_);
    events_.emplace_back("SettlementDates", "FixingDates", settlementLag_.empty() ? "0D" : settlementLag_,
                         settlementCalendar_.empty() ? "NullCalendar" : settlementCalendar_,
                         settlementConvention_.empty() ? "F" : settlementConvention_);

    std::string knockOutProfitAmount = "0", knockOutProfitAmountPoints = "0", knockOutProfitEvents = "0";
    for (auto const& b : barriers_) {
        QL_REQUIRE(b.style().empty() || b.style() == "European", "only european barrier style supported");
        if (b.type() == "CumulatedProfitCap" && !b.levels().empty())
            knockOutProfitAmount = boost::lexical_cast<std::string>(b.levels().front().value());
        else if (b.type() == "CumulatedProfitCapPoints" && !b.levels().empty())
            knockOutProfitAmountPoints = boost::lexical_cast<std::string>(b.levels().front().value());
        else if (b.type() == "FixingCap" && !b.levels().empty())
            knockOutProfitEvents = boost::lexical_cast<std::string>(b.levels().front().value());
        else {
            QL_FAIL("invalid barrier definition, expected CumulatedProfitCap or FixingCap with exactly one level");
        }
    }

    // 4a compute both target amount and points from given trade data, it depends on the variant which we use below

    Real targetAmount = 0.0, targetPoints = 0.0;
    if (!targetAmount_.empty()) {
        targetAmount = parseReal(targetAmount_);
        targetPoints = targetAmount / parseReal(fixingAmount_);
    } else if (!targetPoints_.empty()) {
        targetPoints = parseReal(targetPoints_);
        targetAmount = targetPoints * parseReal(fixingAmount_);
    }

    // 4b choose the variant and check barrier types, set target amount or points dependent on variant

    std::string scriptToUse, amcScriptToUse;
    if (knockOutProfitAmountPoints != "0") {
        scriptToUse = tarf_script_points;
        amcScriptToUse = tarf_script_points_amc;
        QL_REQUIRE(
            knockOutProfitAmount == "0",
            "CumulatedProfitCapPoints can not be combined with other barrier types CumulatedPorfitCap, FixingCap");
        numbers_.emplace_back("Number", "TargetPoints", boost::lexical_cast<std::string>(targetPoints));
        numbers_.emplace_back("Number", "KnockOutProfitAmountPoints", knockOutProfitAmountPoints);
    } else {
        scriptToUse = tarf_script_regular;
        amcScriptToUse = tarf_script_regular_amc;
        numbers_.emplace_back("Number", "TargetAmount", boost::lexical_cast<std::string>(targetAmount));
        numbers_.emplace_back("Number", "KnockOutProfitAmount", knockOutProfitAmount);
        numbers_.emplace_back("Number", "KnockOutProfitEvents", knockOutProfitEvents);
    }

    // 4c set target type

    std::string targetType;
    if (optionData_.payoffType() == "TargetTruncated")
        targetType = "-1";
    else if (optionData_.payoffType() == "TargetExact")
        targetType = "0";
    else if (optionData_.payoffType() == "TargetFull")
        targetType = "1";
    else {
        QL_FAIL("invalid payoffType, expected TargetTruncated, TargetExact, TargetFull");
    }
    numbers_.emplace_back("Number", "TargetType", targetType);

    // 5 set product tag

    productTag_ = "SingleAssetOptionCG({AssetClass})";

    // 6 set script

    script_.clear();

    script_[""] = ScriptedTradeScriptData(scriptToUse, "value",
                                          {{"currentNotional", "currentNotional"},
                                           {"notionalCurrency", "PayCcy"},
                                           {"FixingAmount", "FixingAmount"},
                                           {"Fixing", "Fixing"},
                                           {"Triggered", "Triggered"}},
                                          {});

    script_["AMC"] = ScriptedTradeScriptData(
        amcScriptToUse, "value",
        {{"currentNotional", "currentNotional"},
         {"notionalCurrency", "PayCcy"},
         {"FixingAmount", "FixingAmount"},
         {"Fixing", "Fixing"},
         {"Triggered", "Triggered"}},
        {}, {ScriptedTradeScriptData::NewScheduleData("FixingAndSimDates", "Join", {"_AMC_SimDates", "FixingDates"})},
        {}, {}, {"Asset"});

    // 7 build trade

    ScriptedTrade::build(factory);
}

void TaRF::initIndices() { indices_.emplace_back("Index", "Underlying", scriptedIndexName(underlying_)); }

void TaRF::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(dataNode, tradeType() + "Data node not found");
    currency_ = XMLUtils::getChildValue(dataNode, "Currency", true);
    fixingAmount_ = XMLUtils::getChildValue(dataNode, "FixingAmount", true);
    targetAmount_ = XMLUtils::getChildValue(dataNode, "TargetAmount", false);
    targetPoints_ = XMLUtils::getChildValue(dataNode, "TargetPoints", false);
    QL_REQUIRE(targetAmount_.empty() || targetPoints_.empty(),
               "both TargetAmount and TargetPoints are given, only one of these is allowed at the same time");
    strikes_ = {XMLUtils::getChildValue(dataNode, "Strike", false)};
    if (XMLUtils::getChildNode(dataNode, "Strikes")) {
        QL_REQUIRE(strikes_.front().empty(),
                   "both Strike and Strikes nodes are given, only one of these is allowed at the same time.");
        strikes_ = XMLUtils::getChildrenValuesWithAttributes(dataNode, "Strikes", "Strike", "startDate", strikeDates_);
        QL_REQUIRE(!strikes_.empty(), "noch Strike nodes under Strikes given.");
    }
    strikeDates_.resize(strikes_.size());
    XMLNode* tmp = XMLUtils::getChildNode(dataNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(dataNode, "Name");
    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();

    fixingDates_.fromXML(XMLUtils::getChildNode(dataNode, "ScheduleData"));
    settlementLag_ = XMLUtils::getChildValue(dataNode, "SettlementLag", false);
    settlementCalendar_ = XMLUtils::getChildValue(dataNode, "SettlementCalendar", false);
    settlementConvention_ = XMLUtils::getChildValue(dataNode, "SettlementConvention", false);
    optionData_.fromXML(XMLUtils::getChildNode(dataNode, "OptionData"));
    std::vector<XMLNode*> rangeBounds = {XMLUtils::getChildNode(dataNode, "RangeBounds")};
    if (XMLUtils::getChildNode(dataNode, "RangeBoundSet")) {
        QL_REQUIRE(rangeBounds.front() == nullptr,
                   "both RangeBounds and RangeBoundSet nodes are given, only one allowed at the same time");
        rangeBounds = XMLUtils::getChildrenNodesWithAttributes(dataNode, "RangeBoundSet", "RangeBounds", "startDate",
                                                               rangeBoundSetDates_);
        QL_REQUIRE(!rangeBounds.empty(), "no RangeBounds subnode under RangeBoundSets given");
    }
    QL_REQUIRE(rangeBounds.front() != nullptr, "either RangeBounds or RangeBoundSet nodes required");
    rangeBoundSetDates_.resize(rangeBounds.size());
    for (auto const& r : rangeBounds) {
        rangeBoundSet_.push_back(std::vector<RangeBound>());
        auto rb = XMLUtils::getChildrenNodes(r, "RangeBound");
        for (auto const& n : rb) {
            rangeBoundSet_.back().push_back(RangeBound());
            rangeBoundSet_.back().back().fromXML(n);
        }
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

XMLNode* TaRF::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, dataNode);
    XMLUtils::addChild(doc, dataNode, "Currency", currency_);
    XMLUtils::addChild(doc, dataNode, "FixingAmount", fixingAmount_);
    if (!targetAmount_.empty())
        XMLUtils::addChild(doc, dataNode, "TargetAmount", targetAmount_);
    if (!targetPoints_.empty())
        XMLUtils::addChild(doc, dataNode, "TargetPoints", targetPoints_);
    if (!strikes_.front().empty())
        XMLUtils::addChildrenWithAttributes(doc, dataNode, "Strikes", "Strike", strikes_, "startDate", strikeDates_);
    XMLUtils::appendNode(dataNode, underlying_->toXML(doc));
    XMLUtils::appendNode(dataNode, fixingDates_.toXML(doc));
    if (!settlementLag_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementLag", settlementLag_);
    if (!settlementCalendar_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementCalendar", settlementCalendar_);
    if (!settlementConvention_.empty())
        XMLUtils::addChild(doc, dataNode, "SettlementConvention", settlementConvention_);
    XMLUtils::appendNode(dataNode, optionData_.toXML(doc));
    XMLNode* rangeBoundSet = doc.allocNode("RangeBoundSet");
    for (Size i = 0; i < rangeBoundSet_.size(); ++i) {
        XMLNode* rangeBoundsNode = doc.allocNode("RangeBounds");
        for (auto& n : rangeBoundSet_[i]) {
            XMLUtils::appendNode(rangeBoundsNode, n.toXML(doc));
        }
        if (!rangeBoundSetDates_[i].empty())
            XMLUtils::addAttribute(doc, rangeBoundsNode, "startDate", rangeBoundSetDates_[i]);
        XMLUtils::appendNode(rangeBoundSet, rangeBoundsNode);
    }
    XMLUtils::appendNode(dataNode, rangeBoundSet);
    XMLNode* barriers = doc.allocNode("Barriers");
    for (auto& n : barriers_) {
        XMLUtils::appendNode(barriers, n.toXML(doc));
    }
    XMLUtils::appendNode(dataNode, barriers);
    return node;
}

} // namespace data
} // namespace ore
