/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

using namespace ore::data;

class BasketVarianceSwap : public ScriptedTrade {
public:
    explicit BasketVarianceSwap(const string& tradeType = "BasketVarianceSwap") : ScriptedTrade(tradeType) {}
    BasketVarianceSwap(const Envelope& env, const string& longShort, const string& notional, const string& strike,
                       const string& currency, const string& cap, const string& floor, const string& settlementDate,
                       const ScheduleData& valuationSchedule, bool squaredPayoff,
                       const vector<QuantLib::ext::shared_ptr<Underlying>> underlyings)
        : longShort_(longShort), notional_(notional), strike_(strike), currency_(currency), cap_(cap), floor_(floor),
          settlementDate_(settlementDate), valuationSchedule_(valuationSchedule), squaredPayoff_(squaredPayoff),
          underlyings_(underlyings) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    // This is called within ScriptedTrade::build()
    void setIsdaTaxonomyFields() override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    void initIndices();

    string longShort_, notional_, strike_, currency_, cap_, floor_;
    string settlementDate_;
    ScheduleData valuationSchedule_;
    bool squaredPayoff_;
    vector<QuantLib::ext::shared_ptr<Underlying>> underlyings_;
};

class EquityBasketVarianceSwap : public BasketVarianceSwap {
public:
    EquityBasketVarianceSwap() : BasketVarianceSwap("EquityBasketVarianceSwap") {}
};

class FxBasketVarianceSwap : public BasketVarianceSwap {
public:
    FxBasketVarianceSwap() : BasketVarianceSwap("FxBasketVarianceSwap") {}
};

class CommodityBasketVarianceSwap : public BasketVarianceSwap {
public:
    CommodityBasketVarianceSwap() : BasketVarianceSwap("CommodityBasketVarianceSwap") {}
};

} // namespace data
} // namespace ore
