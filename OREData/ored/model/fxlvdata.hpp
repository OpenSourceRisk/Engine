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

/*! \file model/fxbsdata.hpp
    \brief FX component data for the cross asset model
    \ingroup models
*/

#pragma once

#include <ored/model/fxdata.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! FX Model Parameters
/*! Specification for a FX model local vol component in the cross asset model.
    \ingroup models */
class FxLvData : public FxData {
public:
    FxLvData() : FxData({}, {}, "LocalVol") {}
    FxLvData(std::string foreignCcy, std::string domesticCcy, std::string model, std::string stochasticRatesCorrection,
             std::string calibrationMoneyness, std::string calibrationGrid)
        : FxData(foreignCcy, domesticCcy, "LocalVol"), model_(model),
          stochasticRatesCorrection_(stochasticRatesCorrection), calibrationMoneyness_(calibrationMoneyness),
          calibrationGrid_(calibrationGrid) {}

    //! \name Setters/Getters
    //@{
    const std::string& model() const { return model_; }
    const std::string& stochasticRatesCorrection() const { return stochasticRatesCorrection_; }
    const std::string& calibrationMoneyness() const { return calibrationMoneyness_; }
    const std::string& calibrationGrid() const { return calibrationGrid_; }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    bool operator==(const FxLvData& rhs) const = default;
    bool operator!=(const FxLvData& rhs) const = default;

    QuantLib::ext::shared_ptr<FxData> clone(std::string foreignCcy) const override;

private:
    std::string model_;
    std::string stochasticRatesCorrection_;
    std::string calibrationMoneyness_;
    std::string calibrationGrid_;
};

} // namespace data
} // namespace ore
