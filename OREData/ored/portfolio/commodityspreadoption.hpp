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

class CommoditySpreadOption : public ore::data::Trade {
public:
    CommoditySpreadOption() : ore::data::Trade("CommoditySpreadOption"){}

    //! Implement the build method
    void build(const boost::shared_ptr<ore::data::EngineFactory>& engineFactory) override;

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    std::vector<std::string> const& fxIndex() const {return fxIndex_; }
    const ore::data::OptionData& option() const { return optionData_; }
    QuantLib::Real strike() const { return strike_; }

    //@}

private:

    std::vector<ore::data::LegData> legData_;
    boost::shared_ptr<QuantExt::CommodityCashFlow> longAssetCashFlow_;
    boost::shared_ptr<QuantExt::CommodityCashFlow> shortAssetCashFlow_;
    std::vector<std::string> fxIndex_;
    ore::data::OptionData optionData_;
    QuantLib::Date expiryDate_;
    QuantLib::Real strike_;
    boost::shared_ptr<ore::data::LegData> createLegData() const { return boost::make_shared<ore::data::LegData>(); }

};
}
