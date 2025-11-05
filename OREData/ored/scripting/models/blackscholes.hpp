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

/*! \file ored/scripting/models/blackscholes.hpp
    \brief black scholes model class for n underlyings (fx, equity or commodity)
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/assetmodel.hpp>

namespace ore {
namespace data {

/* This class is the basis for the BlackScholes and LocalVol model implementations */
class BlackScholes final : public AssetModel {
public:
    using AssetModel::AssetModel;

private:
    void performModelCalculations() const override;
    Real initialValue(const Size indexNo) const override;
    Real atmForward(const Size indexNo, const Real t) const override;
    Real compoundingFactor(const Size indexNo, const Date& d1, const Date& d2) const override;

    RandomVariable getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                        const RandomVariable& barrier, const bool above) const override;

    void performCalculationsMc() const;
    void performCalculationsFd() const;
    void generatePaths() const;
    void populatePathValues(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                            const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                            const std::vector<Array>& drift, const std::vector<Matrix>& sqrtCov) const;
    void setAdditionalResults() const;

    // only for MC
    mutable std::vector<Matrix> covariance_; // covariance per effective simulation date
};

} // namespace data
} // namespace ore
