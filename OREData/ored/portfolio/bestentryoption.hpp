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

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/scriptedtrade.hpp>

namespace ore {
namespace data {

class BestEntryOption : public ScriptedTrade {
public:
    explicit BestEntryOption(const string& tradeType = "BestEntryOption") : ScriptedTrade(tradeType) {}
    BestEntryOption(const Envelope& env, const string& longShort, const string& notional, const string& multiplier,
                    const string& strike, const string& cap, const string& resetMinimum, const string& triggerLevel,
                    const QuantLib::ext::shared_ptr<Underlying> underlying, const string& currency,
                    const ScheduleData& observationDates, const string& premiumDate)
        : longShort_(longShort), notional_(notional), multiplier_(multiplier), strike_(strike), cap_(cap),
          resetMinimum_(resetMinimum), triggerLevel_(triggerLevel), underlying_(underlying), currency_(currency),
          observationDates_(observationDates), premiumDate_(premiumDate) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    // This is called within ScriptedTrade::build()
    void setIsdaTaxonomyFields() override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    void initIndices();

    std::string longShort_, notional_, multiplier_, strike_;
    std::string cap_, resetMinimum_, triggerLevel_;
    QuantLib::ext::shared_ptr<Underlying> underlying_;
    std::string currency_;
    ScheduleData observationDates_;
    std::string expiryDate_, premium_, settlementDate_;
    std::string strikeDate_, premiumDate_;
};

class EquityBestEntryOption : public BestEntryOption {
public:
    EquityBestEntryOption() : BestEntryOption("EquityBestEntryOption") {}
};

class FxBestEntryOption : public BestEntryOption {
public:
    FxBestEntryOption() : BestEntryOption("FxBestEntryOption") {}
};

class CommodityBestEntryOption : public BestEntryOption {
public:
    CommodityBestEntryOption() : BestEntryOption("CommodityBestEntryOption") {}
};

} // namespace data
} // namespace ore
