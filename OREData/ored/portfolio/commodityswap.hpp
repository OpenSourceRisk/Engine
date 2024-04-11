/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/commodityswap.hpp
    \brief Commodity Swap data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradefactory.hpp>
#include <ored/portfolio/commoditylegdata.hpp>

namespace ore {
namespace data {

/*! Serializable Commodity Swap
    \ingroup tradedata
*/
class CommoditySwap : public ore::data::Trade {
public:
    CommoditySwap() : ore::data::Trade("CommoditySwap") {}

    CommoditySwap(const ore::data::Envelope& env, const std::vector<ore::data::LegData>& legs)
        : Trade("CommoditySwap", env), legData_(legs) {}

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    QuantLib::Real notional() const override;

    //! Add underlying Commodity names
    std::map<ore::data::AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! \name Inspectors
    //@{
    const std::vector<ore::data::LegData>& legData() const { return legData_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    const std::map<std::string,boost::any>& additionalData() const override;

private:
    QuantLib::ext::shared_ptr<ore::data::LegData> createLegData() const { return QuantLib::ext::make_shared<ore::data::LegData>(); }

    // Perform checks before attempting to build
    void check() const;

    // Build a leg
    void buildLeg(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& ef,
        const ore::data::LegData& legDatum, const std::string& configuration);

    std::vector<ore::data::LegData> legData_;
};

} // namespace data
} // namespace ore
