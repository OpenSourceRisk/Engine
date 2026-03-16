/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav1_3.hpp>
#include <orea/simm/simmconfigurationisdav1_3.hpp>

#include <boost/make_shared.hpp>

#include <ql/math/matrix.hpp>

using QuantLib::Matrix;
using QuantLib::Real;
using std::set;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

typedef CrifRecord::RiskType RiskType; 

SimmConfiguration_ISDA_V1_3::SimmConfiguration_ISDA_V1_3(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const std::string& name, const std::string version)
    : SimmConfiguration_ISDA_V1_0(simmBucketMapper, name, version) {

    // Now add/modify the pieces that we need to amend

    // Set up the correct concentration threshold getter
    simmConcentration_ = QuantLib::ext::make_shared<SimmConcentration_ISDA_V1_3>(simmBucketMapper_);

    // clang-format off

    // Set up the members for this configuration
    // Explanations of all these members are given in the hpp file

    mapLabels_1_[RiskType::InflationVol] = { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" };

    // Risk weights
    rwRiskType_[RiskType::XCcyBasis] = 18;
    rwRiskType_[RiskType::InflationVol] = 0.21;

    // Curvature weights
    curvatureWeights_[RiskType::InflationVol] = curvatureWeights_[RiskType::IRVol];

    // Historical volatility ratios (no additions)

    // Valid risk types
    set<RiskType> temp = {
        RiskType::InflationVol,
        RiskType::BaseCorr,
        RiskType::XCcyBasis,
        RiskType::ProductClassMultiplier,
        RiskType::AddOnNotionalFactor,
        RiskType::PV,
        RiskType::Notional
    };
    validRiskTypes_.insert(temp.begin(), temp.end());

    // Initialise the single, ad-hoc type, correlations
    xccyCorr_ = 0.18;
    infVolCorr_ = 0.33;
    basecorrCorr_ = 0.30;

    // clang-format on
}

} // namespace analytics
} // namespace ore
