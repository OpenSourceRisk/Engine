/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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
    class SimpleMcParameters {
        public:
        SimpleMcParameters() = default;
        SimpleMcParameters(const Size timeStepsPerYear, const Real calibrationMoneynessMin,
                           const Real calibrationMoneynessMax, const Size nStrikes, const Size samples,
                           const Real d2CdK2Threshold, const Size nPasses)
            : timeStepsPerYear_(timeStepsPerYear), calibrationMoneynessMin_(calibrationMoneynessMin),
              calibrationMoneynessMax_(calibrationMoneynessMax), nStrikes_(nStrikes), samples_(samples),
              d2CdK2Threshold_(d2CdK2Threshold), nPasses_(nPasses) {}

        Size timeStepsPerYear() const { return timeStepsPerYear_; }
        Real calibrationMoneynessMin() const { return calibrationMoneynessMin_; }
        Real calibrationMoneynessMax() const { return calibrationMoneynessMax_; }
        Size nStrikes() const { return nStrikes_; }
        Size samples() const { return samples_; }
        Real d2CdK2Threshold() const { return d2CdK2Threshold_; }
        Size nPasses() const { return nPasses_; }

        void fromXML(XMLNode* node);
        XMLNode* toXML(XMLDocument& doc) const;

        bool operator==(const SimpleMcParameters& rhs) const = default;
        bool operator!=(const SimpleMcParameters& rhs) const = default;

        private:
        Size timeStepsPerYear_;
        Real calibrationMoneynessMin_;
        Real calibrationMoneynessMax_;
        Size nStrikes_;
        Size samples_;
        Real d2CdK2Threshold_;
        Size nPasses_;
    };

    FxLvData() : FxData({}, {}, "LocalVol") {}
    FxLvData(std::string foreignCcy, std::string domesticCcy, std::string model, std::string stochasticRatesCorrection,
             std::string calibrationMoneyness, std::string calibrationGrid, SimpleMcParameters simpleMcParameters)
        : FxData(foreignCcy, domesticCcy, "LocalVol"), model_(std::move(model)),
          stochasticRatesCorrection_(std::move(stochasticRatesCorrection)),
          calibrationMoneyness_(std::move(calibrationMoneyness)), calibrationGrid_(std::move(calibrationGrid)),
          simpleMcParameters_(std::move(simpleMcParameters)) {}

    //! \name Setters/Getters
    //@{
    const std::string& model() const { return model_; }
    const std::string& stochasticRatesCorrection() const { return stochasticRatesCorrection_; }
    const std::string& calibrationMoneyness() const { return calibrationMoneyness_; }
    const std::string& calibrationGrid() const { return calibrationGrid_; }
    const SimpleMcParameters& simpleMcParameters() const { return simpleMcParameters_; }
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
    SimpleMcParameters simpleMcParameters_;
};

} // namespace data
} // namespace ore
