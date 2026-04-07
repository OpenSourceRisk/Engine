/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/swaption.hpp
    \brief Swaption data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

/*!
  Serializable Swaption Straddle
  \ingroup tradedata
*/
class SwaptionStraddle : public Trade {
public:
    SwaptionStraddle() : Trade("SwaptionStraddle") {}
    SwaptionStraddle(const Envelope& env, const OptionData& optionData, const std::vector<LegData>& legData)
        : Trade("SwaptionStraddle", env), optionData_(optionData), legData_(legData) {}

    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    // Inspectors
    const OptionData& optionData() const { return optionData_; }
    const std::vector<LegData>& legData() const { return legData_; }
    const QuantLib::ext::shared_ptr<Swaption>& swaption1() const { return swaption1_; }
    const QuantLib::ext::shared_ptr<Swaption>& swaption2() const { return swaption2_; }

    // Serialization
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    const std::map<std::string, QuantLib::ext::any>& additionalData() const override;

    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

private:
    OptionData optionData_;
    std::vector<LegData> legData_;

    QuantLib::ext::shared_ptr<Swaption> swaption1_;
    QuantLib::ext::shared_ptr<Swaption> swaption2_;
    string longShort_;
};

} // namespace data
} // namespace ore
