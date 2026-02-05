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
/*!
  Specification for a FX model component in the Cross Ccy LGM (i.e. lognormal FX with
  stochastic IR differential).
  The specification applies to the volatility component (sigma) of the FX model only.

  \ingroup models
*/
class FxBsData : public FxData {
public:
    FxBsData() = default;
    FxBsData(std::string foreignCcy, std::string domesticCcy, CalibrationType calibrationType, bool calibrateSigma,
             ParamType sigmaType, const std::vector<Time>& sigmaTimes, const std::vector<Real>& sigmaValues,
             std::vector<std::string> optionExpiries = std::vector<std::string>(),
             std::vector<std::string> optionStrikes = std::vector<std::string>())
        : FxData(foreignCcy, domesticCcy, "CrossCcyLgm"), calibrationType_(calibrationType),
          calibrateSigma_(calibrateSigma), sigmaType_(sigmaType), sigmaTimes_(sigmaTimes), sigmaValues_(sigmaValues),
          optionExpiries_(optionExpiries), optionStrikes_(optionStrikes) {}

    //! \name Setters/Getters
    //@{
    CalibrationType calibrationType() const { return calibrationType_; }
    bool calibrateSigma() const { return calibrateSigma_; }
    ParamType sigmaParamType() const { return sigmaType_; }
    const std::vector<Time>& sigmaTimes() const { return sigmaTimes_; }
    const std::vector<Real>& sigmaValues() const { return sigmaValues_; }
    const std::vector<std::string>& optionExpiries() const { return optionExpiries_; }
    const std::vector<std::string>& optionStrikes() const { return optionStrikes_; }
    void setCalibrationType(const CalibrationType type) { calibrationType_ = type; }
    void setCalibrateSigma(bool b) { calibrateSigma_ = b; }
    void setSigmaParamType(const ParamType type) { sigmaType_ = type; }
    void setSigmaTimes(std::vector<Time> times) { sigmaTimes_ = std::move(times); }
    void setSigmaValues(std::vector<Real> values) { sigmaValues_ = std::move(values); }
    void setOptionExpiries(std::vector<std::string> expiries) { optionExpiries_ = std::move(expiries); }
    void setOptionStrikes(std::vector<std::string> strikes) { optionStrikes_ = std::move(strikes); }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    bool operator==(const FxBsData& rhs) const = default;
    bool operator!=(const FxBsData& rhs) const = default;

    QuantLib::ext::shared_ptr<FxData> clone(std::string foreignCcy) const override;

private:
    CalibrationType calibrationType_;
    bool calibrateSigma_;
    ParamType sigmaType_;
    std::vector<Time> sigmaTimes_;
    std::vector<Real> sigmaValues_;
    std::vector<std::string> optionExpiries_;
    std::vector<std::string> optionStrikes_;
};

} // namespace data
} // namespace ore
