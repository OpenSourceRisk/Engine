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

#include <ored/utilities/parsers.hpp>
#include <orea/simm/simmconcentrationisdav1_3_38.hpp>
#include <orea/simm/simmconfigurationisdav1_3_38.hpp>

#include <ql/math/matrix.hpp>

#include <boost/make_shared.hpp>
#include <boost/algorithm/string/predicate.hpp>

using std::string;
using std::vector;
using namespace QuantLib;
using ore::data::parseInteger;

namespace ore {
namespace analytics {

SimmConfiguration_ISDA_V1_3_38::SimmConfiguration_ISDA_V1_3_38(
    const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper, const std::string& name, const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version) {

    // Set up the correct concentration threshold getter
    simmConcentration_ = boost::make_shared<SimmConcentration_ISDA_V1_3_38>(simmBucketMapper_);

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
        { RiskType::Inflation, 45 },
        { RiskType::XCcyBasis, 20 },
        { RiskType::IRVol, 0.21 },
        { RiskType::InflationVol, 0.21 },
        { RiskType::CreditVol, 0.27 },
        { RiskType::CreditVolNonQ, 0.27 },
        { RiskType::CommodityVol, 0.38 },
        { RiskType::FX, 8.0 },
        { RiskType::FXVol, 0.32 },
        { RiskType::BaseCorr, 20.0 }
    };

    rwBucket_ = {
        { RiskType::CreditQ, { 83.0, 85.0, 71.0, 48.0, 46.0, 42.0, 160.0, 229.0, 149.0, 207.0, 138.0, 99.0, 229.0 } },
        { RiskType::CreditNonQ, { 140.0, 2000.0, 2000.0 } },
        { RiskType::Equity, { 25.0, 31.0, 29.0, 27.0, 18.0, 20.0, 25.0, 22.0, 27.0, 28.0, 15.0, 15.0, 31.0 } },
        { RiskType::Commodity, { 19.0, 20.0, 17.0, 18.0, 24.0, 20.0, 24.0, 41.0, 25.0, 89.0, 20.0, 19.0, 16.0, 15.0, 10.0, 89.0, 16.0 } },
        { RiskType::EquityVol, { 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28, 0.64, 0.28 } },
    };

    rwLabel_1_ = {
        { { RiskType::IRCurve, "1" }, { 108.0, 108.0, 94.0, 67.0, 55.0, 52.0, 50.0, 51.0, 51.0, 50.0, 53.0, 60.0 } },
        { { RiskType::IRCurve, "2" }, { 20.0, 20.0, 10.0, 11.0, 14.0, 20.0, 22.0, 20.0, 19.0, 20.0, 23.0, 27.0 } },
        { { RiskType::IRCurve, "3" }, { 91.0, 91.0, 87.0, 91.0, 95.0, 99.0, 96.0, 102.0, 101.0, 100.0, 101.0, 101.0 } }
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
    historicalVolatilityRatios_[RiskType::EquityVol] = 0.67;
    historicalVolatilityRatios_[RiskType::CommodityVol] = 0.81;
    historicalVolatilityRatios_[RiskType::FXVol] = 0.61;

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
        1.00, 0.28, 0.14, 0.18, 0.30, 0.17, 
        0.28, 1.00, 0.58, 0.66, 0.46, 0.27, 
        0.14, 0.58, 1.00, 0.42, 0.27, 0.14, 
        0.18, 0.66, 0.42, 1.00, 0.39, 0.24, 
        0.30, 0.46, 0.27, 0.39, 1.00, 0.32, 
        0.17, 0.27, 0.14, 0.24, 0.32, 1.00
    };
    riskClassCorrelation_ = Matrix(6, 6, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations)
    temp = {
        1.00, 1.00, 0.79, 0.67, 0.53, 0.42, 0.37, 0.30, 0.22, 0.18, 0.16, 0.12,
        1.00, 1.00, 0.79, 0.67, 0.53, 0.42, 0.37, 0.30, 0.22, 0.18, 0.16, 0.12,
        0.79, 0.79, 1.00, 0.85, 0.69, 0.57, 0.50, 0.42, 0.32, 0.25, 0.23, 0.20,
        0.67, 0.67, 0.85, 1.00, 0.86, 0.76, 0.69, 0.59, 0.47, 0.40, 0.37, 0.32,
        0.53, 0.53, 0.69, 0.86, 1.00, 0.93, 0.87, 0.77, 0.63, 0.57, 0.54, 0.50,
        0.42, 0.42, 0.57, 0.76, 0.93, 1.00, 0.98, 0.90, 0.77, 0.70, 0.67, 0.63,
        0.37, 0.37, 0.50, 0.69, 0.87, 0.98, 1.00, 0.96, 0.84, 0.78, 0.75, 0.71,
        0.30, 0.30, 0.42, 0.59, 0.77, 0.90, 0.96, 1.00, 0.93, 0.89, 0.86, 0.82,
        0.22, 0.22, 0.32, 0.47, 0.63, 0.77, 0.84, 0.93, 1.00, 0.98, 0.96, 0.94,
        0.18, 0.18, 0.25, 0.40, 0.57, 0.70, 0.78, 0.89, 0.98, 1.00, 0.99, 0.98,
        0.16, 0.16, 0.23, 0.37, 0.54, 0.67, 0.75, 0.86, 0.96, 0.99, 1.00, 0.99,
        0.12, 0.12, 0.20, 0.32, 0.50, 0.63, 0.71, 0.82, 0.94, 0.98, 0.99, 1.00
    };
    irTenorCorrelation_ = Matrix(12, 12, temp.begin(), temp.end());

    // CreditQ inter-bucket correlations
    temp = {
        1.00, 0.42, 0.39, 0.39, 0.40, 0.38, 0.39, 0.34, 0.37, 0.39, 0.37, 0.31,
        0.42, 1.00, 0.44, 0.45, 0.47, 0.45, 0.33, 0.40, 0.41, 0.44, 0.43, 0.37,
        0.39, 0.44, 1.00, 0.43, 0.45, 0.43, 0.32, 0.35, 0.41, 0.42, 0.40, 0.36,
        0.39, 0.45, 0.43, 1.00, 0.47, 0.44, 0.30, 0.34, 0.39, 0.43, 0.39, 0.36,
        0.40, 0.47, 0.45, 0.47, 1.00, 0.47, 0.31, 0.35, 0.40, 0.44, 0.42, 0.37,
        0.38, 0.45, 0.43, 0.44, 0.47, 1.00, 0.30, 0.34, 0.38, 0.40, 0.39, 0.38,
        0.39, 0.33, 0.32, 0.30, 0.31, 0.30, 1.00, 0.28, 0.31, 0.31, 0.30, 0.26,
        0.34, 0.40, 0.35, 0.34, 0.35, 0.34, 0.28, 1.00, 0.34, 0.35, 0.33, 0.30,
        0.37, 0.41, 0.41, 0.39, 0.40, 0.38, 0.31, 0.34, 1.00, 0.40, 0.37, 0.32,
        0.39, 0.44, 0.42, 0.43, 0.44, 0.40, 0.31, 0.35, 0.40, 1.00, 0.40, 0.35,
        0.37, 0.43, 0.40, 0.39, 0.42, 0.39, 0.30, 0.33, 0.37, 0.40, 1.00, 0.34,
        0.31, 0.37, 0.36, 0.36, 0.37, 0.38, 0.26, 0.30, 0.32, 0.35, 0.34, 1.00
    };
    interBucketCorrelation_[RiskType::CreditQ] = Matrix(12, 12, temp.begin(), temp.end());

    // Equity inter-bucket correlations
    temp = {
        1.00, 0.15, 0.14, 0.16, 0.10, 0.12, 0.10, 0.11, 0.13, 0.09, 0.17, 0.17,
        0.15, 1.00, 0.16, 0.17, 0.10, 0.11, 0.10, 0.11, 0.14, 0.09, 0.17, 0.17,
        0.14, 0.16, 1.00, 0.19, 0.14, 0.17, 0.18, 0.17, 0.16, 0.14, 0.25, 0.25,
        0.16, 0.17, 0.19, 1.00, 0.15, 0.18, 0.18, 0.18, 0.18, 0.14, 0.28, 0.28,
        0.10, 0.10, 0.14, 0.15, 1.00, 0.28, 0.23, 0.27, 0.13, 0.21, 0.35, 0.35,
        0.12, 0.11, 0.17, 0.18, 0.28, 1.00, 0.30, 0.34, 0.16, 0.26, 0.45, 0.45,
        0.10, 0.10, 0.18, 0.18, 0.23, 0.30, 1.00, 0.29, 0.15, 0.24, 0.41, 0.41,
        0.11, 0.11, 0.17, 0.18, 0.27, 0.34, 0.29, 1.00, 0.16, 0.26, 0.44, 0.44,
        0.13, 0.14, 0.16, 0.18, 0.13, 0.16, 0.15, 0.16, 1.00, 0.13, 0.24, 0.24,
        0.09, 0.09, 0.14, 0.14, 0.21, 0.26, 0.24, 0.26, 0.13, 1.00, 0.33, 0.33,
        0.17, 0.17, 0.25, 0.28, 0.35, 0.45, 0.41, 0.44, 0.24, 0.33, 1.00, 0.63,
        0.17, 0.17, 0.25, 0.28, 0.35, 0.45, 0.41, 0.44, 0.24, 0.33, 0.63, 1.00
    };
    interBucketCorrelation_[RiskType::Equity] = Matrix(12, 12, temp.begin(), temp.end());

    // Commodity inter-bucket correlations
    temp = {
        1.00, 0.18, 0.15, 0.20, 0.25, 0.08, 0.19, 0.01, 0.27, 0.00, 0.15, 0.02, 0.06, 0.07, -0.04, 0.00, 0.06,
        0.18, 1.00, 0.89, 0.94, 0.93, 0.32, 0.22, 0.27, 0.24, 0.09, 0.45, 0.21, 0.32, 0.28, 0.17, 0.00, 0.37,
        0.15, 0.89, 1.00, 0.87, 0.88, 0.25, 0.16, 0.19, 0.12, 0.10, 0.26, -0.01, 0.19, 0.17, 0.10, 0.00, 0.27,
        0.20, 0.94, 0.87, 1.00, 0.92, 0.29, 0.22, 0.26, 0.19, 0.00, 0.32, 0.05, 0.20, 0.22, 0.13, 0.00, 0.28,
        0.25, 0.93, 0.88, 0.92, 1.00, 0.30, 0.26, 0.22, 0.28, 0.12, 0.42, 0.23, 0.28, 0.29, 0.17, 0.00, 0.34,
        0.08, 0.32, 0.25, 0.29, 0.30, 1.00, 0.13, 0.57, 0.05, 0.14, 0.15, -0.02, 0.13, 0.17, 0.01, 0.00, 0.26,
        0.19, 0.22, 0.16, 0.22, 0.26, 0.13, 1.00, 0.07, 0.80, 0.19, 0.16, 0.05, 0.17, 0.18, 0.00, 0.00, 0.18,
        0.01, 0.27, 0.19, 0.26, 0.22, 0.57, 0.07, 1.00, 0.13, 0.06, 0.16, 0.03, 0.10, 0.12, 0.06, 0.00, 0.23,
        0.27, 0.24, 0.12, 0.19, 0.28, 0.05, 0.80, 0.13, 1.00, 0.15, 0.17, 0.05, 0.15, 0.13, -0.03, 0.00, 0.13,
        0.00, 0.09, 0.10, 0.00, 0.12, 0.14, 0.19, 0.06, 0.15, 1.00, 0.07, 0.07, 0.17, 0.10, 0.02, 0.00, 0.11,
        0.15, 0.45, 0.26, 0.32, 0.42, 0.15, 0.16, 0.16, 0.17, 0.07, 1.00, 0.34, 0.20, 0.21, 0.16, 0.00, 0.27,
        0.02, 0.21, -0.01, 0.05, 0.23, -0.02, 0.05, 0.03, 0.05, 0.07, 0.34, 1.00, 0.17, 0.26, 0.11, 0.00, 0.14,
        0.06, 0.32, 0.19, 0.20, 0.28, 0.13, 0.17, 0.10, 0.15, 0.17, 0.20, 0.17, 1.00, 0.35, 0.09, 0.00, 0.22,
        0.07, 0.28, 0.17, 0.22, 0.29, 0.17, 0.18, 0.12, 0.13, 0.10, 0.21, 0.26, 0.35, 1.00, 0.06, 0.00, 0.20,
        -0.04, 0.17, 0.10, 0.13, 0.17, 0.01, 0.00, 0.06, -0.03, 0.02, 0.16, 0.11, 0.09, 0.06, 1.00, 0.00, 0.16,
        0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1.00, 0.00,
        0.06, 0.37, 0.27, 0.28, 0.34, 0.26, 0.18, 0.23, 0.13, 0.11, 0.27, 0.14, 0.22, 0.20, 0.16, 0.00, 1.00
    };
    interBucketCorrelation_[RiskType::Commodity] = Matrix(17, 17, temp.begin(), temp.end());

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%)
    intraBucketCorrelation_[RiskType::Equity] = { 0.14, 0.20, 0.19, 0.21, 0.24, 0.35, 0.34,
        0.34, 0.20, 0.24, 0.63, 0.63 };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[RiskType::Commodity] = { 0.55, 0.98, 0.94, 0.99, 1.00, 0.96, 1.00, 0.65, 1.00,
        0.55, 0.55, 0.69, 0.77, 0.24, 0.86, 0.00, 0.28 };

    // Initialise the single, ad-hoc type, correlations
    xccyCorr_ = 0.20;
    infCorr_ = 0.29;
    infVolCorr_ = 0.29;
    irSubCurveCorr_ = 0.98;
    irInterCurrencyCorr_ = 0.23;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.97;
    crqDiffIntraCorr_ = 0.45;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.57;
    crnqDiffIntraCorr_ = 0.27;
    crnqInterCorr_ = 0.21;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.10;

    // clang-format on
}

void SimmConfiguration_ISDA_V1_3_38::addLabels2(const RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V1_3_38::labels2(const boost::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::labels2(irIndex);
}

} // namespace analytics
} // namespace ore
