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
class BlackScholesLocalVolBase : public AssetModel {
public:
    using AssetModel::AssetModel;

protected:
    // shared impl for BlackScholes and LocalVol

    Real initialValue(const Size indexNo) const override;
    Real atmForward(const Size indexNo, const Real t) const override;
    Real compoundingFactor(const Size indexNo, const Date& d1, const Date& d2) const override;

    void performCalculationsFd(const bool localVol) const;
    void setAdditionalResults(const bool localVol) const;
};

} // namespace data
} // namespace ore
