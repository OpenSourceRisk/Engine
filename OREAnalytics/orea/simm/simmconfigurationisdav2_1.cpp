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

#include <orea/simm/simmconcentrationisdav2_1.hpp>
#include <orea/simm/simmconfigurationisdav2_1.hpp>

#include <ql/math/matrix.hpp>

#include <boost/make_shared.hpp>
#include <boost/algorithm/string/predicate.hpp>

using QuantLib::InterestRateIndex;
using QuantLib::Matrix;
using QuantLib::Real;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

SimmConfiguration_ISDA_V2_1::SimmConfiguration_ISDA_V2_1(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const std::string& name, const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version) {

    // Set up the correct concentration threshold getter
    simmConcentration_ = boost::make_shared<SimmConcentration_ISDA_V2_1>(simmBucketMapper_);

    // clang-format off

    // Set up the members for this configuration
    // Explanations of all these members are given in the hpp file
    
    mapBuckets_ = { 
        { RiskType::IRCurve, { "1", "2", "3" } },
        { RiskType::CreditQ, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::CreditVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::CreditNonQ, { "1", "2", "Residual" } },
        { RiskType::CreditVolNonQ, { "1", "2", "Residual" } },
        { RiskType::Equity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::EquityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::Commodity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17" } },
        { RiskType::CommodityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17" } }
    };

    mapLabels_1_ = {
        { RiskType::IRCurve, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::CreditQ, { "1y", "2y", "3y", "5y", "10y" } },
        { RiskType::CreditNonQ, { "1y", "2y", "3y", "5y", "10y" } },
        { RiskType::IRVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::InflationVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::CreditVol, { "1y", "2y", "3y", "5y", "10y" } },
        { RiskType::CreditVolNonQ, { "1y", "2y", "3y", "5y", "10y" } },
        { RiskType::EquityVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::CommodityVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::FXVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } }
    };

    mapLabels_2_ = {
        { RiskType::IRCurve, { "OIS", "Libor1m", "Libor3m", "Libor6m", "Libor12m", "Prime", "Municipal" } },
        { RiskType::CreditQ, { "", "Sec" } }
    };

   
    // Risk weights 
    rwRiskType_ = {
        { RiskType::Inflation, 48 },
        { RiskType::XCcyBasis, 21 },
        { RiskType::IRVol, 0.16 },
        { RiskType::InflationVol, 0.16 },   
        { RiskType::CreditVol, 0.27 },
        { RiskType::CreditVolNonQ, 0.27 },
        { RiskType::CommodityVol, 0.27 },
        { RiskType::FX, 8.1 },
        { RiskType::FXVol, 0.30 },
        { RiskType::BaseCorr, 19.0 }
    };

    rwBucket_ = {
        { RiskType::CreditQ, { 69.0, 107.0, 72.0, 55.0, 48.0, 41.0, 166.0, 187.0, 177.0, 187.0, 129.0, 136.0, 187.0 } },
        { RiskType::CreditNonQ, { 150.0, 1200.0, 1200.0 } },
        { RiskType::Equity, { 24.0, 30.0, 31.0, 25.0, 21.0, 22.0, 27.0, 24.0, 33.0, 34.0, 17.0, 17.0, 34.0 } },
        { RiskType::Commodity, { 19.0, 20.0, 17.0, 19.0, 24.0, 22.0, 26.0, 50.0, 27.0, 54.0, 20.0, 20.0, 17.0, 14.0, 10.0, 54.0, 16.0 } },
        { RiskType::EquityVol, { 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.63, 0.28 } }, 
    };
    
    rwLabel_1_ = {
        { { RiskType::IRCurve, "1" }, { 114.0, 115.0, 102.0, 71.0, 61.0, 52.0, 50.0, 51.0, 51.0, 51.0, 54.0, 62.0 } },
        { { RiskType::IRCurve, "2" }, { 33.0, 20.0, 10.0, 11.0, 14.0, 20.0, 22.0, 20.0, 20.0, 21.0, 23.0, 27.0 } },
        { { RiskType::IRCurve, "3" }, { 91.0, 91.0, 95.0, 88.0, 99.0, 101.0, 101.0, 99.0, 108.0, 100.0, 101.0, 101.0 } }
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
    curvatureWeights_[RiskType::InflationVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::EquityVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::CommodityVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::FXVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::CreditVolNonQ] = curvatureWeights_[RiskType::CreditVol];

    // Historical volatility ratios 
    historicalVolatilityRatios_[RiskType::EquityVol] = 0.59;
    historicalVolatilityRatios_[RiskType::CommodityVol] = 0.74;
    historicalVolatilityRatios_[RiskType::FXVol] = 0.63;

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
        RiskType::IRVol,
        RiskType::InflationVol,
        RiskType::BaseCorr,
        RiskType::XCcyBasis,
        RiskType::ProductClassMultiplier,
        RiskType::AddOnNotionalFactor,
        RiskType::PV,
        RiskType::Notional,
        RiskType::AddOnFixedAmount
    };

    // Risk class correlation matrix  
    vector<Real> temp = {
        1.00, 0.25, 0.15, 0.19, 0.30, 0.26,
        0.25, 1.00, 0.26, 0.65, 0.45, 0.24,
        0.15, 0.26, 1.00, 0.17, 0.22, 0.11,
        0.19, 0.65, 0.17, 1.00, 0.39, 0.23,
        0.30, 0.45, 0.22, 0.39, 1.00, 0.32,
        0.26, 0.24, 0.11, 0.23, 0.32, 1.00
    };
    riskClassCorrelation_ = Matrix(6, 6, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations) 
    temp = {
        1.00, 0.63, 0.59, 0.47, 0.31, 0.22, 0.18, 0.14, 0.09, 0.06, 0.04, 0.05,
        0.63, 1.00, 0.79, 0.67, 0.52, 0.42, 0.37, 0.30, 0.23, 0.18, 0.15, 0.13,
        0.59, 0.79, 1.00, 0.84, 0.68, 0.56, 0.50, 0.42, 0.32, 0.26, 0.24, 0.21,
        0.47, 0.67, 0.84, 1.00, 0.86, 0.76, 0.69, 0.60, 0.48, 0.42, 0.38, 0.33,
        0.31, 0.52, 0.68, 0.86, 1.00, 0.94, 0.89, 0.80, 0.67, 0.60, 0.57, 0.53,
        0.22, 0.42, 0.56, 0.76, 0.94, 1.00, 0.98, 0.91, 0.79, 0.73, 0.70, 0.66,
        0.18, 0.37, 0.50, 0.69, 0.89, 0.98, 1.00, 0.96, 0.87, 0.81, 0.78, 0.74,
        0.14, 0.30, 0.42, 0.60, 0.80, 0.91, 0.96, 1.00, 0.95, 0.91, 0.88, 0.84,
        0.09, 0.23, 0.32, 0.48, 0.67, 0.79, 0.87, 0.95, 1.00, 0.98, 0.97, 0.94,
        0.06, 0.18, 0.26, 0.42, 0.60, 0.73, 0.81, 0.91, 0.98, 1.00, 0.99, 0.97,
        0.04, 0.15, 0.24, 0.38, 0.57, 0.70, 0.78, 0.88, 0.97, 0.99, 1.00, 0.99,
        0.05, 0.13, 0.21, 0.33, 0.53, 0.66, 0.74, 0.84, 0.94, 0.97, 0.99, 1.00
    };
    irTenorCorrelation_ = Matrix(12, 12, temp.begin(), temp.end());

    // CreditQ inter-bucket correlations 
    temp = {
        1.00, 0.38, 0.36, 0.36, 0.39, 0.35, 0.34, 0.32, 0.34, 0.33, 0.34, 0.31,
        0.38, 1.00, 0.41, 0.41, 0.43, 0.40, 0.29, 0.38, 0.38, 0.38, 0.38, 0.34,
        0.36, 0.41, 1.00, 0.41, 0.42, 0.39, 0.30, 0.34, 0.39, 0.37, 0.38, 0.35,
        0.36, 0.41, 0.41, 1.00, 0.43, 0.40, 0.28, 0.33, 0.37, 0.38, 0.38, 0.34,
        0.39, 0.43, 0.42, 0.43, 1.00, 0.42, 0.31, 0.35, 0.38, 0.39, 0.41, 0.36,
        0.35, 0.40, 0.39, 0.40, 0.42, 1.00, 0.27, 0.32, 0.34, 0.35, 0.36, 0.33,
        0.34, 0.29, 0.30, 0.28, 0.31, 0.27, 1.00, 0.24, 0.28, 0.27, 0.27, 0.26,
        0.32, 0.38, 0.34, 0.33, 0.35, 0.32, 0.24, 1.00, 0.33, 0.32, 0.32, 0.29,
        0.34, 0.38, 0.39, 0.37, 0.38, 0.34, 0.28, 0.33, 1.00, 0.35, 0.35, 0.33,
        0.33, 0.38, 0.37, 0.38, 0.39, 0.35, 0.27, 0.32, 0.35, 1.00, 0.36, 0.32,
        0.34, 0.38, 0.38, 0.38, 0.41, 0.36, 0.27, 0.32, 0.35, 0.36, 1.00, 0.33,
        0.31, 0.34, 0.35, 0.34, 0.36, 0.33, 0.26, 0.29, 0.33, 0.32, 0.33, 1.00
    };
    interBucketCorrelation_[RiskType::CreditQ] = Matrix(12, 12, temp.begin(), temp.end());

    // Equity inter-bucket correlations
    temp = {
        1.00, 0.16, 0.16, 0.17, 0.13, 0.15, 0.15, 0.15, 0.13, 0.11, 0.19, 0.19,
        0.16, 1.00, 0.20, 0.20, 0.14, 0.16, 0.16, 0.16, 0.15, 0.13, 0.20, 0.20,
        0.16, 0.20, 1.00, 0.22, 0.15, 0.19, 0.22, 0.19, 0.16, 0.15, 0.25, 0.25,
        0.17, 0.20, 0.22, 1.00, 0.17, 0.21, 0.21, 0.21, 0.17, 0.15, 0.27, 0.27,
        0.13, 0.14, 0.15, 0.17, 1.00, 0.25, 0.23, 0.26, 0.14, 0.17, 0.32, 0.32,
        0.15, 0.16, 0.19, 0.21, 0.25, 1.00, 0.30, 0.31, 0.16, 0.21, 0.38, 0.38,
        0.15, 0.16, 0.22, 0.21, 0.23, 0.30, 1.00, 0.29, 0.16, 0.21, 0.38, 0.38,
        0.15, 0.16, 0.19, 0.21, 0.26, 0.31, 0.29, 1.00, 0.17, 0.21, 0.39, 0.39,
        0.13, 0.15, 0.16, 0.17, 0.14, 0.16, 0.16, 0.17, 1.00, 0.13, 0.21, 0.21,
        0.11, 0.13, 0.15, 0.15, 0.17, 0.21, 0.21, 0.21, 0.13, 1.00, 0.25, 0.25,
        0.19, 0.20, 0.25, 0.27, 0.32, 0.38, 0.38, 0.39, 0.21, 0.25, 1.00, 0.51,
        0.19, 0.20, 0.25, 0.27, 0.32, 0.38, 0.38, 0.39, 0.21, 0.25, 0.51, 1.00
    };
    interBucketCorrelation_[RiskType::Equity] = Matrix(12, 12, temp.begin(), temp.end());

    // Commodity inter-bucket correlations
    temp = {
        1.00, 0.16, 0.11, 0.19, 0.22, 0.12, 0.22, 0.02, 0.27, 0.08, 0.11, 0.05, 0.04, 0.06, 0.01, 0.00, 0.10,
        0.16, 1.00, 0.89, 0.94, 0.93, 0.32, 0.24, 0.19, 0.21, 0.06, 0.39, 0.23, 0.39, 0.29, 0.13, 0.00, 0.66,
        0.11, 0.89, 1.00, 0.87, 0.88, 0.17, 0.17, 0.13, 0.12, 0.03, 0.24, 0.04, 0.27, 0.19, 0.08, 0.00, 0.61,
        0.19, 0.94, 0.87, 1.00, 0.92, 0.37, 0.27, 0.21, 0.21, 0.03, 0.36, 0.16, 0.27, 0.28, 0.09, 0.00, 0.64,
        0.22, 0.93, 0.88, 0.92, 1.00, 0.29, 0.26, 0.19, 0.23, 0.10, 0.40, 0.27, 0.38, 0.30, 0.15, 0.00, 0.64,
        0.12, 0.32, 0.17, 0.37, 0.29, 1.00, 0.19, 0.60, 0.18, 0.09, 0.22, 0.09, 0.14, 0.16, 0.10, 0.00, 0.37,
        0.22, 0.24, 0.17, 0.27, 0.26, 0.19, 1.00, 0.06, 0.68, 0.16, 0.21, 0.10, 0.24, 0.25, -0.01, 0.00, 0.27,
        0.02, 0.19, 0.13, 0.21, 0.19, 0.60, 0.06, 1.00, 0.12, 0.01, 0.10, 0.03, 0.02, 0.07, 0.10, 0.00, 0.21,
        0.27, 0.21, 0.12, 0.21, 0.23, 0.18, 0.68, 0.12, 1.00, 0.05, 0.16, 0.03, 0.19, 0.16, -0.01, 0.00, 0.19,
        0.08, 0.06, 0.03, 0.03, 0.10, 0.09, 0.16, 0.01, 0.05, 1.00, 0.08, 0.04, 0.05, 0.11, 0.02, 0.00, 0.00,
        0.11, 0.39, 0.24, 0.36, 0.40, 0.22, 0.21, 0.10, 0.16, 0.08, 1.00, 0.34, 0.19, 0.22, 0.15, 0.00, 0.34,
        0.05, 0.23, 0.04, 0.16, 0.27, 0.09, 0.10, 0.03, 0.03, 0.04, 0.34, 1.00, 0.14, 0.26, 0.09, 0.00, 0.20,
        0.04, 0.39, 0.27, 0.27, 0.38, 0.14, 0.24, 0.02, 0.19, 0.05, 0.19, 0.14, 1.00, 0.30, 0.16, 0.00, 0.40,
        0.06, 0.29, 0.19, 0.28, 0.30, 0.16, 0.25, 0.07, 0.16, 0.11, 0.22, 0.26, 0.30, 1.00, 0.09, 0.00, 0.30,
        0.01, 0.13, 0.08, 0.09, 0.15, 0.10, -0.01, 0.10, -0.01, 0.02, 0.15, 0.09, 0.16, 0.09, 1.00, 0.00, 0.16,
        0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1.00, 0.00,
        0.10, 0.66, 0.61, 0.64, 0.64, 0.37, 0.27, 0.21, 0.19, 0.00, 0.34, 0.20, 0.40, 0.30, 0.16, 0.00, 1.00
    };
    interBucketCorrelation_[RiskType::Commodity] = Matrix(17, 17, temp.begin(), temp.end());

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[RiskType::Equity] = { 0.14, 0.20, 0.25, 0.23, 0.23, 0.32, 0.35,
        0.32, 0.17, 0.16, 0.51, 0.51 };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[RiskType::Commodity] = { 0.27, 0.97, 0.92, 0.97, 0.99, 1.00, 1.00, 0.40, 0.73, 
        0.13, 0.53, 0.64, 0.63, 0.26, 0.26, 0.00, 0.38 };

    // Initialise the single, ad-hoc type, correlations 
    xccyCorr_ = 0.19;
    infCorr_ = 0.33;
    infVolCorr_ = 0.33;
    irSubCurveCorr_ = 0.98;
    irInterCurrencyCorr_ = 0.21;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.96;
    crqDiffIntraCorr_ = 0.39;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.57;
    crnqDiffIntraCorr_ = 0.20;
    crnqInterCorr_ = 0.16;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.05;

    // clang-format on
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 5 section 11
of the ISDA-SIMM-v2.1 documentation).
*/
QuantLib::Real SimmConfiguration_ISDA_V2_1::curvatureMarginScaling() const {
    QuantLib::Real hvr = 0.62;
    return pow(hvr, -2.0);
}

void SimmConfiguration_ISDA_V2_1::addLabels2(const RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V2_1::labels2(const boost::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::labels2(irIndex);
}

} // namespace analytics
} // namespace ore
