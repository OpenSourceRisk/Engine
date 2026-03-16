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

/*! \file ored/portfolio/tarf.hpp
    \brief tarf wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>
#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/underlying.hpp>
#include <ored/portfolio/rangebound.hpp>

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

class TaRF : public ScriptedTrade {
public:
    explicit TaRF(const std::string& tradeType = "TaRF") : ScriptedTrade(tradeType) {}
    TaRF(const std::string& currency, const std::string& fixingAmount, const std::string& targetAmount,
         const std::string& targetPoints, const std::vector<std::string>& strikes,
         const std::vector<std::string>& strikeDates, const QuantLib::ext::shared_ptr<Underlying>& underlying,
         const ScheduleData& fixingDates, const std::string& settlementLag, const std::string& settlementCalendar,
         const std::string& settlementConvention, OptionData& optionData,
         const std::vector<std::vector<RangeBound>>& rangeBoundSet, const std::vector<std::string>& rangeBoundSetDates,
         const std::vector<BarrierData>& barriers);
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    //! \name Inspectors
    //@{
    const std::string& name() const { return underlying_->name(); }
    //@}

private:
    void initIndices();
    std::string currency_, fixingAmount_, targetAmount_, targetPoints_;
    std::vector<std::string> strikes_, strikeDates_;
    QuantLib::ext::shared_ptr<Underlying> underlying_;
    ScheduleData fixingDates_;
    std::string settlementLag_, settlementCalendar_, settlementConvention_;
    OptionData optionData_;
    std::vector<std::vector<RangeBound>> rangeBoundSet_;
    std::vector<std::string> rangeBoundSetDates_;
    std::vector<BarrierData> barriers_;
};

class EquityTaRF : public TaRF {
public:
    EquityTaRF() : TaRF("EquityTaRF") {}
};

class FxTaRF : public TaRF {
public:
    explicit FxTaRF() : TaRF("FxTaRF") {}
};

class CommodityTaRF : public TaRF {
public:
    explicit CommodityTaRF() : TaRF("CommodityTaRF") {}
};

} // namespace data
} // namespace ore
