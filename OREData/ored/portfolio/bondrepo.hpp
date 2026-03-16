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

/*! \file ored/portfolio/bondrepo.hpp
 \brief Bond Repo trade data model and serialization
 \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

class BondRepo : public Trade {
public:
    BondRepo() : Trade("BondRepo") {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    const ore::data::BondData& bondData() const { return securityLegData_; }
    const ore::data::LegData& cashLegData() const { return cashLegData_; }

private:
    ore::data::BondData originalSecurityLegData_, securityLegData_;
    ore::data::LegData cashLegData_;

    QuantLib::ext::shared_ptr<ore::data::Bond> securityLeg_;
    Leg cashLeg_;
};
} // namespace data
} // namespace ore
