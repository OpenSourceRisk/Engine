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

/*! \file ored/portfolio/genericbarrieroption.hpp
    \brief generic barrier option wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>

#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {

class GenericBarrierOption : public ScriptedTrade {
public:
    explicit GenericBarrierOption(const std::string& tradeType = "GenericBarrierOption") : ScriptedTrade(tradeType) {}
    GenericBarrierOption(std::vector<QuantLib::ext::shared_ptr<Underlying>> underlyings, const OptionData& optionData,
                         const std::vector<BarrierData>& barriers, const ScheduleData& barrierMonitoringDates,
                         const std::vector<BarrierData>& transatlanticBarrier, const std::string& payCurrency,
                         const std::string& settlementDate, const std::string& quantity, const std::string& strike,
                         const std::string& amount, const std::string& kikoType)
        : underlyings_(underlyings), optionData_(optionData), barriers_(barriers),
          barrierMonitoringDates_(barrierMonitoringDates), transatlanticBarrier_(transatlanticBarrier),
          payCurrency_(payCurrency), settlementDate_(settlementDate), quantity_(quantity), strike_(strike),
          amount_(amount), kikoType_(kikoType) {
        initIndices();
    }

    GenericBarrierOption(QuantLib::ext::shared_ptr<Underlying>& underlying, const OptionData& optionData,
                         const std::vector<BarrierData>& barriers, const ScheduleData& barrierMonitoringDates,
                         const BarrierData& transatlanticBarrier, const std::string& payCurrency,
                         const std::string& settlementDate, const std::string& quantity, const std::string& strike,
                         const std::string& amount, const std::string& kikoType)
        : optionData_(optionData), barriers_(barriers),
          barrierMonitoringDates_(barrierMonitoringDates), 
          payCurrency_(payCurrency), settlementDate_(settlementDate), quantity_(quantity), strike_(strike),
          amount_(amount), kikoType_(kikoType) {
        underlyings_.push_back(underlying);
        transatlanticBarrier_.push_back(transatlanticBarrier);
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    //! \name Inspectors
    //@{
    std::set<std::string> names() const { 
        std::set<std::string> names;
        for (const auto& u : underlyings_)
            names.insert(u->name());
        return names;
    }
    //@}

private:
    void initIndices();
    QuantLib::Calendar getUnderlyingCalendar(const QuantLib::ext::shared_ptr<EngineFactory>& factory) const;
    std::vector<QuantLib::ext::shared_ptr<ore::data::Underlying>> underlyings_;
    OptionData optionData_;
    std::vector<BarrierData> barriers_;
    ScheduleData barrierMonitoringDates_;
    std::vector<BarrierData> transatlanticBarrier_;
    std::string barrierMonitoringStartDate_, barrierMonitoringEndDate_;
    std::string payCurrency_, settlementDate_, quantity_, strike_, amount_, kikoType_;
    std::string settlementLag_, settlementCalendar_, settlementConvention_;
};

class EquityGenericBarrierOption : public GenericBarrierOption {
public:
    EquityGenericBarrierOption() : GenericBarrierOption("EquityGenericBarrierOption") {}

    std::map<ore::data::AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const override {
        return {{ore::data::AssetClass::EQ, names()}};
    }
};

class FxGenericBarrierOption : public GenericBarrierOption {
public:
    FxGenericBarrierOption() : GenericBarrierOption("FxGenericBarrierOption") {}
};

class CommodityGenericBarrierOption : public GenericBarrierOption {
public:
    CommodityGenericBarrierOption() : GenericBarrierOption("CommodityGenericBarrierOption") {}
};

} // namespace data
} // namespace ore
