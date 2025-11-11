/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/models/heston.hpp
    \brief Heston model class for n underlyings (fx, equity or commodity)
*/

#pragma once

#include <ored/scripting/models/assetmodel.hpp>

namespace ore {
namespace data {

class Heston final : public AssetModel {
public:
    using AssetModel::AssetModel;

private:
    void performModelCalculations() const override;
    Real initialValue(const Size indexNo) const override;
    Real atmForward(const Size indexNo, const Real t) const override;
    Real compoundingFactor(const Size indexNo, const Date& d1, const Date& d2) const override;

    void performCalculationsMc() const;
    void performCalculationsFd() const;
    void generatePaths() const;
    void populatePathValues(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                            const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                            const Matrix& correlation, const Matrix& sqrtCorr,
                            const std::vector<Array>& deterministicDrift, const std::vector<Size>& eqComIdx,
                            const std::vector<Real>& t, const std::vector<Real>& dt,
                            const std::vector<Real>& sqrtdt) const;
    void setAdditionalResults() const;
};

} // namespace data
} // namespace ore
