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

/*!
  Path generation for multiple assets using the Heston model

  TODO:
  - Check foreign currency drift adjustment
  - Check materiality of variance paths dropping to zero when the Feller constraint is violated
  - Compare terminal spot price distribution in MC vs analytical, with and without Feller violation 
  
*/
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
                            const std::vector<Size>& eqComIdx) const;
    void setAdditionalResults() const;
 
    void generateSingleAssetPaths() const; 
};


struct MultiAssetHestonPaths {
    // MultiAssetHestonPaths() { DLOG("MultiAssetHestonPaths ctor called"); }
    // ~MultiAssetHestonPaths() { DLOG("MultiAssetHestonPaths destructor called"); }
    // MultiAssetHestonPaths(const MultiAssetHestonPaths& p) {
    //     DLOG("MultiAssetHestonPaths copy ctor called");
    //     samples = p.samples;
    //     indexNames = p.indexNames;
    // 	dates = p.dates;
    // 	data = p.data;
    // }
    Size samples;
    std::vector<std::string> indexNames;
    std::vector<Date> dates;
    std::map<Date, std::vector<std::vector<Real>>> data;
};

} // namespace data
} // namespace ore
