/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/windowbarrieroption.hpp
    \brief window barrier option - wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradestrike.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {

class WindowBarrierOption : public ScriptedTrade {
public:
    explicit WindowBarrierOption(const std::string& tradeType = "WindowBarrierOption") : ScriptedTrade(tradeType) {}
    WindowBarrierOption(const std::string& currency, const std::string& fixingAmount, const TradeStrike& strike,
                        const QuantLib::ext::shared_ptr<Underlying>& underlying, const std::string& startDate,
                        const std::string& endDate, const OptionData& optionData, const BarrierData& barrier)
        : currency_(currency), fixingAmount_(fixingAmount), strike_(strike), underlying_(underlying),
          startDate_(startDate), endDate_(endDate), optionData_(optionData), barrier_(barrier) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields() override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    //! \name Inspectors
    //@{
    const std::string& name() const { return underlying_->name(); }
    //@}

private:
    void initIndices();
    std::string currency_, fixingAmount_;
    TradeStrike strike_;
    QuantLib::ext::shared_ptr<Underlying> underlying_;
    std::string startDate_, endDate_;
    OptionData optionData_;
    BarrierData barrier_;
};

class EquityWindowBarrierOption : public WindowBarrierOption {
public:
    EquityWindowBarrierOption() : WindowBarrierOption("EquityWindowBarrierOption") {}
};

class FxWindowBarrierOption : public WindowBarrierOption {
public:
    FxWindowBarrierOption() : WindowBarrierOption("FxWindowBarrierOption") {}
};

class CommodityWindowBarrierOption : public WindowBarrierOption {
public:
    CommodityWindowBarrierOption() : WindowBarrierOption("CommodityWindowBarrierOption") {}
};

} // namespace data
} // namespace ore
