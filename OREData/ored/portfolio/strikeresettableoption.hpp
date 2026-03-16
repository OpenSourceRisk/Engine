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

class StrikeResettableOption : public ScriptedTrade {
public:
    explicit StrikeResettableOption(const string& tradeType = "StrikeResettableOption") : ScriptedTrade(tradeType) {}
    StrikeResettableOption(const Envelope& env, const string& longShort, const string& optionType, const string& currency, const string& quantity,
                           const string& strike, const string& resetStrike, const string& triggerType, const string& triggerPrice,
                           const QuantLib::ext::shared_ptr<Underlying> underlying, const ScheduleData& observationDates,
                           const string& expiryDate, const string& settlementDate, const string& premium,
                           const string& premiumDate)
        : longShort_(longShort), optionType_(optionType), currency_(currency), quantity_(quantity), strike_(strike), resetStrike_(resetStrike),
          triggerType_(triggerType), triggerPrice_(triggerPrice), underlying_(underlying), observationDates_(observationDates),
          expiryDate_(expiryDate), settlementDate_(settlementDate), premium_(premium), premiumDate_(premiumDate) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    // This is called within ScriptedTrade::build()
    void setIsdaTaxonomyFields() override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    void initIndices();

    std::string longShort_, optionType_, currency_, quantity_, strike_, resetStrike_;
    std::string triggerType_, triggerPrice_;
    QuantLib::ext::shared_ptr<Underlying> underlying_;
    ScheduleData observationDates_;
    std::string expiryDate_, settlementDate_;
    std::string premium_, premiumDate_;
};

class EquityStrikeResettableOption : public StrikeResettableOption {
public:
    EquityStrikeResettableOption() : StrikeResettableOption("EquityStrikeResettableOption") {}
};

class FxStrikeResettableOption : public StrikeResettableOption {
public:
    FxStrikeResettableOption() : StrikeResettableOption("FxStrikeResettableOption") {}
};

class CommodityStrikeResettableOption : public StrikeResettableOption {
public:
    CommodityStrikeResettableOption() : StrikeResettableOption("CommodityStrikeResettableOption") {}
};

} // namespace data
} // namespace ore
