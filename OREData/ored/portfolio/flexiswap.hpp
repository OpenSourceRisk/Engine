/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/flexiswap.hpp
    \brief Flexi-Swap data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable Flexi-Swap
/*!
  \ingroup tradedata
*/
class FlexiSwap : public ore::data::Trade {
public:
    FlexiSwap() : Trade("FlexiSwap") {}

    /*! the optionality is described by lower notional bounds */
    FlexiSwap(
        const ore::data::Envelope& env, const std::vector<ore::data::LegData>& underlyingData,
        const std::map<std::string, std::pair<std::vector<double>, std::vector<std::string>>>& lowerNotionalBounds,
        const std::string& optionLongShort)
        : Trade("FlexiSwap", env), underlyingData_(underlyingData), lowerNotionalBounds_(lowerNotionalBounds),
          optionLongShort_(optionLongShort) {}

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;

    QuantLib::Real notional() const override;

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! Add underlying index names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

private:
    std::vector<ore::data::LegData> underlyingData_;
    // optionality given by lower notional boudns
    std::map<std::string, std::pair<std::vector<double>, std::vector<std::string>>> lowerNotionalBounds_;
    // long or short option
    std::string optionLongShort_;
    // notional info
    Size notionalTakenFromLeg_;
};

} // namespace data
} // namespace ore
