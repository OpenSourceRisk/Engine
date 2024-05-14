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

/*! \file ored/portfolio/accumulator.hpp
    \brief accumulator wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>

#include <ored/portfolio/rangebound.hpp>
#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradestrike.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {

class Accumulator : public ScriptedTrade {
public:
    explicit Accumulator(const std::string& tradeType = "Accumulator") : ScriptedTrade(tradeType) {}
    Accumulator(const std::string& currency, const std::string& fixingAmount, const TradeStrike& strike,
                const QuantLib::ext::shared_ptr<Underlying>& underlying, const OptionData& optionData,
                const std::string& startDate, const ScheduleData& observationDates, const ScheduleData& pricingDates,
                const ScheduleData& settlementDates, const std::string& settlementLag,
                const std::string& settlementCalendar, const std::string& settlementConvention,
                const std::vector<RangeBound>& rangeBounds, const std::vector<BarrierData>& barriers)
        : currency_(currency), fixingAmount_(fixingAmount), strike_(strike), underlying_(underlying),
          optionData_(optionData), startDate_(startDate), observationDates_(observationDates),
          pricingDates_(pricingDates), settlementDates_(settlementDates), settlementLag_(settlementLag),
          settlementCalendar_(settlementCalendar), settlementConvention_(settlementConvention),
          rangeBounds_(rangeBounds), barriers_(barriers) {
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
    OptionData optionData_;
    std::string startDate_;
    ScheduleData observationDates_, pricingDates_, settlementDates_;
    std::string settlementLag_, settlementCalendar_, settlementConvention_;
    bool nakedOption_ = false;
    bool dailyFixingAmount_ = false;

    std::vector<RangeBound> rangeBounds_;
    std::vector<BarrierData> barriers_;
};

class EquityAccumulator : public Accumulator {
public:
    EquityAccumulator() : Accumulator("EquityAccumulator") {}
};

class FxAccumulator : public Accumulator {
public:
    FxAccumulator() : Accumulator("FxAccumulator") {}
};

class CommodityAccumulator : public Accumulator {
public:
    CommodityAccumulator() : Accumulator("CommodityAccumulator") {}
};

} // namespace data
} // namespace ore
