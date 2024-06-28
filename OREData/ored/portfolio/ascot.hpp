/*
 Copyright (C) 2020 Quaternion Risk Management Ltd

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

/*! \file ored/portfolio/ascot.hpp
 \brief Ascot (or Convertible Bond Option) trade data model and serialization
 \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/convertiblebond.hpp>
#include <ored/portfolio/convertiblebonddata.hpp>

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable Convertible Bond
class Ascot : public Trade {
public:
    //! Default constructor
    Ascot() : Trade("Ascot") {}

    //! Constructor for coupon bonds
    Ascot(const Envelope& env, const ConvertibleBond& bond, const OptionData& optionData,
          const ore::data::LegData& fundingLegData)
        : Trade("Ascot", env), bond_(bond), optionData_(optionData), fundingLegData_(fundingLegData) {}

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    const ConvertibleBond& bond() const { return bond_; }
    const OptionData& optionData() const { return optionData_; }
    const ore::data::LegData& fundingLegData() const { return fundingLegData_; }

    // FIXE remove? this is only needed for the simm product class determination.
    const string& creditCurveId() const { return bond_.data().bondData().creditCurveId(); }

private:
    ConvertibleBond bond_;
    OptionData optionData_;
    ore::data::LegData fundingLegData_;
};

} // namespace data
} // namespace ore
