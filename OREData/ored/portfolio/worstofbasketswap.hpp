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

/*! \file ored/portfolio/europeanoptionbarrier.hpp
    \brief European option with barrier wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/scriptedtrade.hpp>

namespace ore {
namespace data {

class WorstOfBasketSwap : public ScriptedTrade {
public:
    explicit WorstOfBasketSwap(const string& tradeType = "WorstOfBasketSwap") : ScriptedTrade(tradeType) {}
    WorstOfBasketSwap(const Envelope& env, const string& longShort, const string& quantity, const string& strike,
                      const string& initialFixedRate, const vector<string>& initialPrices, const string& fixedRate,
                      const ScriptedTradeEventData& floatingPeriodSchedule,
                      const ScriptedTradeEventData& floatingFixingSchedule,
                      const ScriptedTradeEventData& fixedDeterminationSchedule,
                      const ScriptedTradeEventData& floatingPayDates, const ScriptedTradeEventData& fixedPayDates,
                      const ScriptedTradeEventData& knockOutDeterminationSchedule,
                      const ScriptedTradeEventData& knockInDeterminationSchedule, const string& knockInPayDate,
                      const string& initialFixedPayDate, const bool bermudanKnockIn,
                      const bool accumulatingFixedCoupons, const bool accruingFixedCoupons, const bool isAveraged,
                      const string& floatingIndex, const string& floatingSpread, const string& floatingRateCutoff,
                      // const string& inArrears,
                      const DayCounter& floatingDayCountFraction, const Period& floatingLookback,
                      const bool includeSpread, const string& currency,
                      const vector<QuantLib::ext::shared_ptr<Underlying>> underlyings, const string& knockInLevel,
                      const vector<string> fixedTriggerLevels, const vector<string> knockOutLevels,
                      const ScriptedTradeEventData& fixedAccrualSchedule)
        : longShort_(longShort), quantity_(quantity), strike_(strike), initialFixedRate_(initialFixedRate),
          initialPrices_(initialPrices), fixedRate_(fixedRate), floatingPeriodSchedule_(floatingPeriodSchedule),
          floatingFixingSchedule_(floatingFixingSchedule), fixedDeterminationSchedule_(fixedDeterminationSchedule),
          knockOutDeterminationSchedule_(knockOutDeterminationSchedule), floatingPayDates_(floatingPayDates),
          knockInDeterminationSchedule_(knockInDeterminationSchedule), fixedPayDates_(fixedPayDates),
          knockInPayDate_(knockInPayDate), initialFixedPayDate_(initialFixedPayDate), bermudanKnockIn_(bermudanKnockIn),
          accumulatingFixedCoupons_(accumulatingFixedCoupons), accruingFixedCoupons_(accruingFixedCoupons),
          isAveraged_(isAveraged), floatingIndex_(floatingIndex), floatingSpread_(floatingSpread),
          floatingRateCutoff_(floatingRateCutoff), // inArrears_(inArrears),
          floatingDayCountFraction_(floatingDayCountFraction), floatingLookback_(floatingLookback),
          includeSpread_(includeSpread), currency_(currency), underlyings_(underlyings), knockInLevel_(knockInLevel),
          fixedTriggerLevels_(fixedTriggerLevels), knockOutLevels_(knockOutLevels),
          fixedAccrualSchedule_(fixedAccrualSchedule) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields() override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    void initIndices();

    string longShort_, quantity_, strike_, initialFixedRate_;
    vector<string> initialPrices_;
    string fixedRate_;
    ScriptedTradeEventData floatingPeriodSchedule_, floatingFixingSchedule_, fixedDeterminationSchedule_,
        knockOutDeterminationSchedule_, floatingPayDates_, knockInDeterminationSchedule_, fixedPayDates_;
    string knockInPayDate_, initialFixedPayDate_;
    bool bermudanKnockIn_, accumulatingFixedCoupons_, accruingFixedCoupons_, isAveraged_;
    string floatingIndex_, floatingSpread_, floatingRateCutoff_; //, inArrears_;
    DayCounter floatingDayCountFraction_;
    Period floatingLookback_;
    bool includeSpread_;
    string currency_;
    vector<QuantLib::ext::shared_ptr<Underlying>> underlyings_;
    string knockInLevel_;
    vector<string> fixedTriggerLevels_, knockOutLevels_;
    ScriptedTradeEventData fixedAccrualSchedule_;
    // <scheduleName, <stEventData, fallbackSchedule> >
    map<string, pair<ScriptedTradeEventData, string>> schedules_;
};

class EquityWorstOfBasketSwap : public WorstOfBasketSwap {
public:
    EquityWorstOfBasketSwap() : WorstOfBasketSwap("EquityWorstOfBasketSwap") {}
};

class FxWorstOfBasketSwap : public WorstOfBasketSwap {
public:
    FxWorstOfBasketSwap() : WorstOfBasketSwap("FxWorstOfBasketSwap") {}
};

class CommodityWorstOfBasketSwap : public WorstOfBasketSwap {
public:
    CommodityWorstOfBasketSwap() : WorstOfBasketSwap("CommodityWorstOfBasketSwap") {}
};

} // namespace data
} // namespace ore
