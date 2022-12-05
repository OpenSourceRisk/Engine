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
#ifndef ORE_COMMODITYSPREADOPTION_HPP
#define ORE_COMMODITYSPREADOPTION_HPP

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/commoditylegdata.hpp>

namespace ore::data {
using namespace QuantExt;

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
    std::string const& exerciseDate() const { return exerciseDate_; }
    std::vector<boost::shared_ptr<CommodityFloatingLegData>> const& commodityLegData() const { return commLegData_; }
    std::vector<std::string> const& fxIndex() const {return fxIndex_; }
    QuantLib::Real quantity() const { return quantity_; }
    QuantLib::Real strike() const { return spreadStrike_; }
    std::string const& callPut() const { return callPut_; }
    std::string const& settlementDate() const { return settlementDate_; }
    std::string const& settlementCcy() const { return settlementCcy_; }
    //@}

private:

    std::vector<ore::data::LegData> legData_;
    std::vector<boost::shared_ptr<CommodityFloatingLegData>> commLegData_;
    std::string exerciseDate_;
    boost::shared_ptr<QuantExt::CommodityCashFlow> longAssetCashFlow_;
    boost::shared_ptr<QuantExt::CommodityCashFlow> shortAssetCashFlow_;
    std::vector<std::string> fxIndex_;
    QuantLib::Real quantity_;
    QuantLib::Spread spreadStrike_;
    std::string callPut_;
    std::string settlementDate_;
    std::string settlementCcy_;

    boost::shared_ptr<ore::data::LegData> createLegData() const { return boost::make_shared<ore::data::LegData>(); }

};
}
#endif // ORE_COMMODITYSPREADOPTION_HPP
