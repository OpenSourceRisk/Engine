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

/*! \file ored/portfolio/basketoption.hpp
    \brief basket option wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradestrike.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {

class BasketOption : public ScriptedTrade {
public:
    explicit BasketOption(const std::string& tradeType = "BasketOption") : ScriptedTrade(tradeType) {}
    BasketOption(const std::string& currency, const std::string& notional, const TradeStrike& strike,
                 const std::vector<QuantLib::ext::shared_ptr<ore::data::Underlying>>& underlyings, const OptionData& optionData,
                 const std::string& settlement, const ScheduleData& observationDates)
        : currency_(currency), notional_(notional), tradeStrike_(strike), underlyings_(underlyings),
          optionData_(optionData), settlement_(settlement), observationDates_(observationDates) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields() override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

protected:
    void initIndices();
    std::string currency_, notional_;
    TradeStrike tradeStrike_;
    std::vector<QuantLib::ext::shared_ptr<ore::data::Underlying>> underlyings_;
    OptionData optionData_;
    std::string settlement_;
    ScheduleData observationDates_;
};

class EquityBasketOption : public BasketOption {
public:
    EquityBasketOption()
        : BasketOption("EquityBasketOption") {}
};

class FxBasketOption : public BasketOption {
public:
    FxBasketOption()
        : BasketOption("FxBasketOption") {}
};

class CommodityBasketOption : public BasketOption {
public:
    CommodityBasketOption()
        : BasketOption("CommodityBasketOption") {}
};

} // namespace data
} // namespace ore
