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

#include <ored/portfolio/commoditylegdata.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore::data {

class CommoditySpreadOptionData : public XMLSerializable {
public:

    class OptionStripData : public XMLSerializable {
    public:
        ScheduleData schedule() const { return schedule_; }
        BusinessDayConvention bdc() const { return bdc_; }
        int lag() const { return lag_; }
        Calendar calendar() const { return calendar_; }
        //! \name Serialisation
        //@{
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        //@}
    private:
        ScheduleData schedule_;
        BusinessDayConvention bdc_;
        int lag_;
        Calendar calendar_;
    };

    CommoditySpreadOptionData() {}
    CommoditySpreadOptionData(const std::vector<ore::data::LegData>& legData, const ore::data::OptionData& optionData,
                              QuantLib::Real strike)
        : legData_(legData), optionData_(optionData), strike_(strike) {}
    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    const std::vector<ore::data::LegData>& legData() const { return legData_; }
    const ore::data::OptionData& optionData() const { return optionData_; }
    QuantLib::Real strike() const { return strike_; }
    boost::optional<OptionStripData> optionStrip() { return optionStrip_; }

private:
    QuantLib::ext::shared_ptr<ore::data::LegData> createLegData() const { return QuantLib::ext::make_shared<ore::data::LegData>(); }

    std::vector<ore::data::LegData> legData_;
    ore::data::OptionData optionData_;
    QuantLib::Real strike_;

    boost::optional<OptionStripData> optionStrip_;
    
};

class CommoditySpreadOption : public ore::data::Trade {
public:
    CommoditySpreadOption() : ore::data::Trade("CommoditySpreadOption") {}
    CommoditySpreadOption(const CommoditySpreadOptionData& data)
        : ore::data::Trade("CommoditySpreadOption"), csoData_(data) {}

    //! Implement the build method
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) override;

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    std::vector<std::string> const& fxIndex() const { return fxIndex_; }
    const ore::data::OptionData& option() const { return csoData_.optionData(); }
    QuantLib::Real strike() const { return csoData_.strike(); }

    //@}

    //! Add underlying Commodity names
    std::map<ore::data::AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

private:
    CommoditySpreadOptionData csoData_;
    std::vector<std::string> fxIndex_;    
};
} // namespace ore::data
