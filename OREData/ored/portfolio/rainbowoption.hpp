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

/*! \file ored/portfolio/rainbowoption.hpp
    \brief rainbow option wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/portfolio/underlying.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

class RainbowOption : public ScriptedTrade {
public:
    explicit RainbowOption(const QuantLib::ext::shared_ptr<Conventions>& conventions = nullptr,
                           const std::string& tradeType = "RainbowOption")
        : ScriptedTrade(tradeType) {}
    RainbowOption(const std::string& currency, const std::string& notional, const std::string& strike,
                  const std::vector<QuantLib::ext::shared_ptr<Underlying>>& underlyings, const OptionData& optionData,
                  const std::string& settlement)
        : currency_(currency), notional_(notional), strike_(strike), underlyings_(underlyings), optionData_(optionData),
          settlement_(settlement) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields() override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    void initIndices();
    std::string currency_, notional_, strike_;
    std::vector<QuantLib::ext::shared_ptr<Underlying>> underlyings_;
    OptionData optionData_;
    std::string settlement_;
};

class EquityRainbowOption : public RainbowOption {
public:
    explicit EquityRainbowOption(const QuantLib::ext::shared_ptr<Conventions>& conventions = nullptr)
        : RainbowOption(conventions, "EquityRainbowOption") {}
};

class FxRainbowOption : public RainbowOption {
public:
    explicit FxRainbowOption(const QuantLib::ext::shared_ptr<Conventions>& conventions = nullptr)
        : RainbowOption(conventions, "FxRainbowOption") {}
};

class CommodityRainbowOption : public RainbowOption {
public:
    explicit CommodityRainbowOption(const QuantLib::ext::shared_ptr<Conventions>& conventions = nullptr)
        : RainbowOption(conventions, "CommodityRainbowOption") {}
};

} // namespace data
} // namespace ore
