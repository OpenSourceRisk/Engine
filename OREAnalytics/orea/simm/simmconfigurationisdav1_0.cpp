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

#include <orea/simm/simmconcentration.hpp>
#include <orea/simm/simmconfigurationisdav1_0.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/make_shared.hpp>

#include <ql/math/matrix.hpp>

using ore::data::parseInteger;
using QuantLib::Integer;
using QuantLib::Matrix;
using QuantLib::Real;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

SimmConfiguration_ISDA_V1_0::SimmConfiguration_ISDA_V1_0(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const std::string& name, const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version) {

    // Set up the correct concentration threshold getter
    simmConcentration_ = boost::make_shared<SimmConcentrationBase>();

    // clang-format off

    // Set up the members for this configuration
    // Explanations of all these members are given in the hpp file
    
    mapBuckets_ = { 
        { RiskType::IRCurve, { "1", "2", "3" } },
        { RiskType::CreditQ, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::CreditVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::CreditNonQ, { "1", "2", "Residual" } },
        { RiskType::CreditVolNonQ, { "1", "2", "Residual" } },
        { RiskType::Equity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "Residual" } },
        { RiskType::EquityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "Residual" } },
        { RiskType::Commodity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" } },
        { RiskType::CommodityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" } }
    };

    mapLabels_1_ = {
        { RiskType::IRCurve, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::CreditQ, { "1y", "2y", "3y", "5y", "10y" } }
    };
    mapLabels_1_[RiskType::IRVol] = mapLabels_1_[RiskType::IRCurve];
    mapLabels_1_[RiskType::EquityVol] = mapLabels_1_[RiskType::IRCurve];
    mapLabels_1_[RiskType::CommodityVol] = mapLabels_1_[RiskType::IRCurve];
    mapLabels_1_[RiskType::FXVol] = mapLabels_1_[RiskType::IRCurve];
    mapLabels_1_[RiskType::CreditNonQ] = mapLabels_1_[RiskType::CreditQ];
    mapLabels_1_[RiskType::CreditVol] = mapLabels_1_[RiskType::CreditQ];
    mapLabels_1_[RiskType::CreditVolNonQ] = mapLabels_1_[RiskType::CreditQ];

    mapLabels_2_ = {
        { RiskType::IRCurve, { "OIS", "Libor1m", "Libor3m", "Libor6m", "Libor12m", "Prime" } },
        { RiskType::CreditQ, { "", "Sec" } }
    };

    // Risk weights
    rwRiskType_ = {
        { RiskType::Inflation, 32 },
        { RiskType::IRVol, 0.21 },
        { RiskType::CreditVol, 0.35 },
        { RiskType::CreditVolNonQ, 0.35 },
        { RiskType::EquityVol, 0.21 },
        { RiskType::CommodityVol, 0.36 },
        { RiskType::FX, 7.9 },
        { RiskType::FXVol, 0.21 },
        { RiskType::BaseCorr, 18.0 }
    };

    rwBucket_ = {
        { RiskType::CreditQ, { 97.0, 110.0, 73.0, 65.0, 52.0, 39.0, 198.0, 638.0, 210.0, 375.0, 240.0, 152.0, 638.0 } },
        { RiskType::CreditNonQ, { 169.0, 1646.0, 1646.0 } },
        { RiskType::Equity, { 22.0, 28.0, 28.0, 25.0, 18.0, 20.0, 24.0, 23.0, 26.0, 27.0, 15.0, 28.0 } },
        { RiskType::Commodity, { 9.0, 19.0, 18.0, 13.0, 24.0, 17.0, 21.0, 35.0, 20.0, 50.0, 21.0, 19.0, 17.0, 15.0, 8.0, 50.0 } }
    };

    rwLabel_1_ = {
        { { RiskType::IRCurve, "1" }, { 77.0, 77.0, 77.0, 64.0, 58.0, 49.0, 47.0, 47.0, 45.0, 45.0, 48.0, 56.0 } },
        { { RiskType::IRCurve, "2" }, { 10.0, 10.0, 10.0, 10.0, 13.0, 16.0, 18.0, 20.0, 25.0, 22.0, 22.0, 23.0 } },
        { { RiskType::IRCurve, "3" }, { 89.0, 89.0, 89.0, 94.0, 104.0, 99.0, 96.0, 99.0, 87.0, 97.0, 97.0, 98.0 } }
    };

    // Curvature weights
    curvatureWeights_ = {
        { RiskType::IRVol, { 0.5, 
                             0.5 * 14.0 / (365.0 / 12.0), 
                             0.5 * 14.0 / (3.0 * 365.0 / 12.0), 
                             0.5 * 14.0 / (6.0 * 365.0 / 12.0), 
                             0.5 * 14.0 / 365.0, 
                             0.5 * 14.0 / (2.0 * 365.0), 
                             0.5 * 14.0 / (3.0 * 365.0), 
                             0.5 * 14.0 / (5.0 * 365.0), 
                             0.5 * 14.0 / (10.0 * 365.0), 
                             0.5 * 14.0 / (15.0 * 365.0), 
                             0.5 * 14.0 / (20.0 * 365.0), 
                             0.5 * 14.0 / (30.0 * 365.0) } 
        },
        { RiskType::CreditVol, { 0.5 * 14.0 / 365.0, 
                                 0.5 * 14.0 / (2.0 * 365.0), 
                                 0.5 * 14.0 / (3.0 * 365.0), 
                                 0.5 * 14.0 / (5.0 * 365.0), 
                                 0.5 * 14.0 / (10.0 * 365.0) } 
        }
    };
    curvatureWeights_[RiskType::EquityVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::CommodityVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::FXVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::CreditVolNonQ] = curvatureWeights_[RiskType::CreditVol];

    // Historical volatility ratios empty (1.0 for everything)

    // Valid risk types
    validRiskTypes_ = {
        RiskType::Commodity,
        RiskType::CommodityVol,
        RiskType::CreditNonQ,
        RiskType::CreditQ,
        RiskType::CreditVol,
        RiskType::CreditVolNonQ,
        RiskType::Equity,
        RiskType::EquityVol,
        RiskType::FX,
        RiskType::FXVol,
        RiskType::Inflation,
        RiskType::IRCurve,
        RiskType::IRVol
    };

    // Risk class correlation matrix
    vector<Real> temp = {
        1.00, 0.09, 0.10, 0.18, 0.32, 0.27,
        0.09, 1.00, 0.24, 0.58, 0.34, 0.29,
        0.10, 0.24, 1.00, 0.23, 0.24, 0.12,
        0.18, 0.58, 0.23, 1.00, 0.26, 0.31,
        0.32, 0.34, 0.24, 0.26, 1.00, 0.37,
        0.27, 0.29, 0.12, 0.31, 0.37, 1.00
    };
    riskClassCorrelation_ = Matrix(6, 6, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations)
    temp = {
        1.000, 1.000, 1.000, 0.782, 0.618, 0.498, 0.438, 0.361, 0.270, 0.196, 0.174, 0.129,
        1.000, 1.000, 1.000, 0.782, 0.618, 0.498, 0.438, 0.361, 0.270, 0.196, 0.174, 0.129,
        1.000, 1.000, 1.000, 0.782, 0.618, 0.498, 0.438, 0.361, 0.270, 0.196, 0.174, 0.129,
        0.782, 0.782, 0.782, 1.000, 0.840, 0.739, 0.667, 0.569, 0.444, 0.375, 0.349, 0.296,
        0.618, 0.618, 0.618, 0.840, 1.000, 0.917, 0.859, 0.757, 0.626, 0.555, 0.526, 0.471,
        0.498, 0.498, 0.498, 0.739, 0.917, 1.000, 0.976, 0.895, 0.749, 0.690, 0.660, 0.602,
        0.438, 0.438, 0.438, 0.667, 0.859, 0.976, 1.000, 0.958, 0.831, 0.779, 0.746, 0.690,
        0.361, 0.361, 0.361, 0.569, 0.757, 0.895, 0.958, 1.000, 0.925, 0.893, 0.859, 0.812,
        0.270, 0.270, 0.270, 0.444, 0.626, 0.749, 0.831, 0.925, 1.000, 0.980, 0.961, 0.931,
        0.196, 0.196, 0.196, 0.375, 0.555, 0.690, 0.779, 0.893, 0.980, 1.000, 0.989, 0.970,
        0.174, 0.174, 0.174, 0.349, 0.526, 0.660, 0.746, 0.859, 0.961, 0.989, 1.000, 0.988,
        0.129, 0.129, 0.129, 0.296, 0.471, 0.602, 0.690, 0.812, 0.931, 0.970, 0.988, 1.000
    };
    irTenorCorrelation_ = Matrix(12, 12, temp.begin(), temp.end());

    // CreditQ inter-bucket correlations
    temp = {
        1.00, 0.51, 0.47, 0.49, 0.46, 0.47, 0.41, 0.36, 0.45, 0.47, 0.47, 0.43,
        0.51, 1.00, 0.52, 0.52, 0.49, 0.52, 0.37, 0.41, 0.51, 0.50, 0.51, 0.46,
        0.47, 0.52, 1.00, 0.54, 0.51, 0.55, 0.37, 0.37, 0.51, 0.49, 0.50, 0.47,
        0.49, 0.52, 0.54, 1.00, 0.53, 0.56, 0.36, 0.37, 0.52, 0.51, 0.51, 0.46,
        0.46, 0.49, 0.51, 0.53, 1.00, 0.54, 0.35, 0.35, 0.49, 0.48, 0.50, 0.44,
        0.47, 0.52, 0.55, 0.56, 0.54, 1.00, 0.37, 0.37, 0.52, 0.49, 0.51, 0.48,
        0.41, 0.37, 0.37, 0.36, 0.35, 0.37, 1.00, 0.29, 0.36, 0.34, 0.36, 0.36,
        0.36, 0.41, 0.37, 0.37, 0.35, 0.37, 0.29, 1.00, 0.37, 0.36, 0.37, 0.33,
        0.45, 0.51, 0.51, 0.52, 0.49, 0.52, 0.36, 0.37, 1.00, 0.49, 0.50, 0.46,
        0.47, 0.50, 0.49, 0.51, 0.48, 0.49, 0.34, 0.36, 0.49, 1.00, 0.49, 0.46,
        0.47, 0.51, 0.50, 0.51, 0.50, 0.51, 0.36, 0.37, 0.50, 0.49, 1.00, 0.46,
        0.43, 0.46, 0.47, 0.46, 0.44, 0.48, 0.36, 0.33, 0.46, 0.46, 0.46, 1.00
    };
    interBucketCorrelation_[RiskType::CreditQ] = Matrix(12, 12, temp.begin(), temp.end());

    // Equity inter-bucket correlations
    temp = {
        1.00, 0.17, 0.18, 0.16, 0.08, 0.10, 0.10, 0.11, 0.16, 0.08, 0.18,
        0.17, 1.00, 0.24, 0.19, 0.07, 0.10, 0.09, 0.10, 0.19, 0.07, 0.18,
        0.18, 0.24, 1.00, 0.21, 0.09, 0.12, 0.13, 0.13, 0.20, 0.10, 0.24,
        0.16, 0.19, 0.21, 1.00, 0.13, 0.17, 0.16, 0.17, 0.20, 0.13, 0.30,
        0.08, 0.07, 0.09, 0.13, 1.00, 0.28, 0.24, 0.28, 0.10, 0.23, 0.38,
        0.10, 0.10, 0.12, 0.17, 0.28, 1.00, 0.30, 0.33, 0.13, 0.26, 0.45,
        0.10, 0.09, 0.13, 0.16, 0.24, 0.30, 1.00, 0.29, 0.13, 0.25, 0.42,
        0.11, 0.10, 0.13, 0.17, 0.28, 0.33, 0.29, 1.00, 0.14, 0.27, 0.45,
        0.16, 0.19, 0.20, 0.20, 0.10, 0.13, 0.13, 0.14, 1.00, 0.11, 0.25,
        0.08, 0.07, 0.10, 0.13, 0.23, 0.26, 0.25, 0.27, 0.11, 1.00, 0.34,
        0.18, 0.18, 0.24, 0.30, 0.38, 0.45, 0.42, 0.45, 0.25, 0.34, 1.00
    };
    interBucketCorrelation_[RiskType::Equity] = Matrix(11, 11, temp.begin(), temp.end());

    // Commodity inter-bucket correlations
    temp = {
        1.00, 0.11, 0.16, 0.13, 0.10, 0.06, 0.20, 0.05, 0.17, 0.03, 0.18, 0.09, 0.10, 0.05, 0.04, 0.00,
        0.11, 1.00, 0.95, 0.95, 0.93, 0.15, 0.27, 0.19, 0.20, 0.14, 0.30, 0.31, 0.26, 0.26, 0.12, 0.00,
        0.16, 0.95, 1.00, 0.92, 0.90, 0.17, 0.24, 0.14, 0.17, 0.12, 0.32, 0.26, 0.16, 0.22, 0.12, 0.00,
        0.13, 0.95, 0.92, 1.00, 0.90, 0.18, 0.26, 0.08, 0.17, 0.08, 0.31, 0.25, 0.15, 0.20, 0.09, 0.00,
        0.10, 0.93, 0.90, 0.90, 1.00, 0.18, 0.37, 0.13, 0.30, 0.21, 0.34, 0.32, 0.27, 0.29, 0.12, 0.00,
        0.06, 0.15, 0.17, 0.18, 0.18, 1.00, 0.07, 0.62, 0.03, 0.15, 0.00, 0.00, 0.23, 0.15, 0.07, 0.00,
        0.20, 0.27, 0.24, 0.26, 0.37, 0.07, 1.00, 0.07, 0.66, 0.20, 0.06, 0.06, 0.12, 0.09, 0.09, 0.00,
        0.05, 0.19, 0.14, 0.08, 0.13, 0.62, 0.07, 1.00, 0.09, 0.12, -0.01, 0.00, 0.18, 0.11, 0.04, 0.00,
        0.17, 0.20, 0.17, 0.17, 0.30, 0.03, 0.66, 0.09, 1.00, 0.12, 0.10, 0.06, 0.12, 0.10, 0.10, 0.00,
        0.03, 0.14, 0.12, 0.08, 0.21, 0.15, 0.20, 0.12, 0.12, 1.00, 0.10, 0.07, 0.09, 0.10, 0.16, 0.00,
        0.18, 0.30, 0.32, 0.31, 0.34, 0.00, 0.06, -0.01, 0.10, 0.10, 1.00, 0.46, 0.20, 0.26, 0.18, 0.00,
        0.09, 0.31, 0.26, 0.25, 0.32, 0.00, 0.06, 0.00, 0.06, 0.07, 0.46, 1.00, 0.25, 0.23, 0.14, 0.00,
        0.10, 0.26, 0.16, 0.15, 0.27, 0.23, 0.12, 0.18, 0.12, 0.09, 0.20, 0.25, 1.00, 0.29, 0.06, 0.00,
        0.05, 0.26, 0.22, 0.20, 0.29, 0.15, 0.09, 0.11, 0.10, 0.10, 0.26, 0.23, 0.29, 1.00, 0.15, 0.00,
        0.04, 0.12, 0.12, 0.09, 0.12, 0.07, 0.09, 0.04, 0.10, 0.16, 0.18, 0.14, 0.06, 0.15, 1.00, 0.00,
        0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1.00
    };
    interBucketCorrelation_[RiskType::Commodity] = Matrix(16, 16, temp.begin(), temp.end());

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%)
    intraBucketCorrelation_[RiskType::Equity] = { 0.14, 0.24, 0.25, 0.2, 0.26, 0.34, 
        0.33, 0.34, 0.21, 0.24, 0.63 };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[RiskType::Commodity] = { 0.71, 0.92, 0.97, 0.97, 0.99, 0.98, 1.0, 0.69,
        0.47, 0.01, 0.67, 0.70, 0.68, 0.22, 0.50, 0.0 };

    // Initialise the single, ad-hoc type, correlations
    xccyCorr_ = 0.0; // not a valid risk type
    infCorr_ = 0.33;
    infVolCorr_ = 0.0; // not a valid risk type
    irSubCurveCorr_ = 0.982;
    irInterCurrencyCorr_ = 0.27;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.98;
    crqDiffIntraCorr_ = 0.55;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.60;
    crnqDiffIntraCorr_ = 0.21;
    crnqInterCorr_ = 0.05;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.0; // not a valid risk type

    // clang-format on
}

} // namespace analytics
} // namespace ore
