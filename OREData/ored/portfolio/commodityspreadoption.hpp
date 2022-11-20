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

private:
    OptionData optionData_;
    std::vector<string> underlyingName_;
    std::vector<boost::shared_ptr<CommodityIndex>> underlying_;
    std::vector<string> underlyingCcy_;
    std::vector<boost::optional<Date>> futureExpiryDate_;
    string settlementCcy_;
    Date settlementDate_;
    boost::optional<bool> isAveraged_;
    boost::shared_ptr<FxIndex> fxIndex_;
};
}
#endif // ORE_COMMODITYSPREADOPTION_HPP
