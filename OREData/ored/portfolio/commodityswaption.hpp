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

/*! \file ored/portfolio/commodityswaption.hpp
    \brief Commodity swaption data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/commodityswap.hpp>

namespace ore {
namespace data {

/*! Serializable Commodity Swap
    \ingroup tradedata
*/
class CommoditySwaption : public ore::data::Trade {
public:
    CommoditySwaption() : ore::data::Trade("CommoditySwaption") {}

    CommoditySwaption(const ore::data::Envelope& env, const ore::data::OptionData& option,
                      const std::vector<ore::data::LegData>& legData)
        : Trade("CommoditySwaption", env), legData_(legData) {}

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) override;
    QuantLib::Real notional() const override;

    //! Add underlying Commodity names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! \name Inspectors
    //@{
    const ore::data::OptionData& option() { return option_; }
    const std::vector<ore::data::LegData>& legData() const { return legData_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Trade
    //@{
    bool hasCashflows() const override { return false; }
    //@}

private:
    QuantLib::ext::shared_ptr<ore::data::LegData> createLegData() const { return QuantLib::ext::make_shared<ore::data::LegData>(); }

    QuantLib::ext::shared_ptr<QuantLib::Swap> buildSwap(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory);

    ore::data::OptionData option_;
    vector<ore::data::LegData> legData_;
    QuantLib::ext::shared_ptr<QuantLib::Exercise> exercise_;
    std::string name_;
    std::string ccy_;
    QuantLib::Date startDate_;
    QuantLib::ext::shared_ptr<CommoditySwap> commoditySwap_;
};

} // namespace data
} // namespace ore
