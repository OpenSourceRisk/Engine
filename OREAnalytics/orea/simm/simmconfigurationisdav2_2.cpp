/*
 Copyright (C) 2019 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav2_2.hpp>
#include <orea/simm/simmconfigurationisdav2_2.hpp>

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

QuantLib::Size SimmConfiguration_ISDA_V2_2::group(const string& qualifier,
                                                  const std::map<QuantLib::Size, std::set<string>>& categories) const {
    QuantLib::Size result = 0;
    for (const auto& kv : categories) {
        if (kv.second.empty()) {
            result = kv.first;
        } else {
            if (kv.second.count(qualifier) > 0) {
                // Found qualifier in category so return it
                return kv.first;
            }
        }
    }

    // If we get here, result should hold category with empty set
    return result;
}

QuantLib::Real SimmConfiguration_ISDA_V2_2::weight(const RiskType& rt, boost::optional<string> qualifier,
                                                   boost::optional<std::string> label_1,
                                                   const std::string& calculationCurrency) const {

    if (rt == RiskType::FX) {
        QL_REQUIRE(calculationCurrency != "", "no calculation currency provided weight");
        QL_REQUIRE(qualifier, "need a qualifier to return a risk weight for the risk type FX");

        QuantLib::Size g1 = group(calculationCurrency, ccyGroups_);
        QuantLib::Size g2 = group(*qualifier, ccyGroups_);
        return rwFX_[g1][g2];
    }

    return SimmConfigurationBase::weight(rt, qualifier, label_1);
}

QuantLib::Real SimmConfiguration_ISDA_V2_2::correlation(const RiskType& firstRt, const string& firstQualifier,
                                                        const string& firstLabel_1, const string& firstLabel_2,
                                                        const RiskType& secondRt, const string& secondQualifier,
                                                        const string& secondLabel_1, const string& secondLabel_2,
                                                        const std::string& calculationCurrency) const {

    if (firstRt == RiskType::FX && secondRt == RiskType::FX) {
        QL_REQUIRE(calculationCurrency != "", "no calculation currency provided corr");
        QuantLib::Size g = group(calculationCurrency, ccyGroups_);
        QuantLib::Size g1 = group(firstQualifier, ccyGroups_);
        QuantLib::Size g2 = group(secondQualifier, ccyGroups_);
        if (g == 0) {
            return fxRegVolCorrelation_[g1][g2];
        } else if (g == 1) {
            return fxHighVolCorrelation_[g1][g2];
        } else {
            QL_FAIL("FX Volatility group " << g << " not recognized");
        }
    }

    return SimmConfigurationBase::correlation(firstRt, firstQualifier, firstLabel_1, firstLabel_2, secondRt,
                                              secondQualifier, secondLabel_1, secondLabel_2);
}

SimmConfiguration_ISDA_V2_2::SimmConfiguration_ISDA_V2_2(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const QuantLib::Size& mporDays, const std::string& name,
                                                         const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = boost::make_shared<SimmConcentration_ISDA_V2_2>(simmBucketMapper_);
    } else {
        // SIMM:Technical Paper, Section I.4: "The Concentration Risk feature is disabled"
        simmConcentration_ = boost::make_shared<SimmConcentrationBase>();
    }

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

    // Populate CCY groups that are used for FX correlations and risk weights
    // The groups consists of High Vol Currencies (currently just BRL) & regular vol currencies (all others)
    ccyGroups_ = {
        { 1, {"BRL"} },
        { 0, { } },
    };

    vector<Real> temp;

    if (mporDays_ == 10) {
        // Risk weights
        temp = {
            7.57, 10.28, 
            10.28, 10.28 
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 47 },
            { RiskType::XCcyBasis, 20 },
            { RiskType::IRVol, 0.16 },
            { RiskType::InflationVol, 0.16 },   
            { RiskType::CreditVol, 0.39 },
            { RiskType::CreditVolNonQ, 0.39 },
            { RiskType::CommodityVol, 0.35 },
            { RiskType::FXVol, 0.30 },
            { RiskType::BaseCorr, 7.0 }
        };

        rwBucket_ = {
            { RiskType::CreditQ, { 72.0, 97.0, 75.0, 53.0, 45.0, 53.0, 165.0, 250.0, 191.0, 179.0, 132.0, 129.0, 250.0 } },
            { RiskType::CreditNonQ, { 100.0, 1600.0, 1600.0 } },
            { RiskType::Equity, { 22.0, 26.0, 29.0, 26.0, 19.0, 21.0, 25.0, 24.0, 30.0, 29.0, 17.0, 17.0, 30.0 } },
            { RiskType::Commodity, { 19.0, 20.0, 16.0, 19.0, 24.0, 24.0, 28.0, 42.0, 28.0, 53.0, 20.0, 19.0, 15.0, 16.0, 11.0, 53.0, 16.0 } },
            { RiskType::EquityVol, { 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.62, 0.26 } }, 
        };
        
        rwLabel_1_ = {
            { { RiskType::IRCurve, "1" }, { 116.0, 106.0, 94.0, 71.0, 59.0, 52.0, 49.0, 51.0, 51.0, 51.0, 54.0, 62.0 } },
            { { RiskType::IRCurve, "2" }, { 14.0, 20.0, 10.0, 10.0, 14.0, 20.0, 22.0, 20.0, 20.0, 20.0, 22.0, 27.0 } },
            { { RiskType::IRCurve, "3" }, { 85.0, 80.0, 79.0, 86.0, 97.0, 102.0, 104.0, 102.0, 103.0, 99.0, 99.0, 100.0 } }
        };
        
        // Historical volatility ratios 
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.61;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.78;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.65;
        hvr_ir_ = 0.53;
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

    } else {
        // SIMM:Technical Paper, Section I.1: "All delta and vega risk weights should be replaced with the values for 
        // one-day calibration given in the Calibration Results document."
        
        // Risk weights
        temp = {
            1.85, 3.04, 
            3.04, 3.04 
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 9.4 },
            { RiskType::XCcyBasis, 5.3 },
            { RiskType::IRVol, 0.04 },
            { RiskType::InflationVol, 0.04 },   
            { RiskType::CreditVol, 0.084 },
            { RiskType::CreditVolNonQ, 0.084 },
            { RiskType::CommodityVol, 0.09 },
            { RiskType::FXVol, 0.078 },
            { RiskType::BaseCorr, 1.5 }
        };

        rwBucket_ = {
            { RiskType::CreditQ, { 16.0, 21.0, 15.0, 11.0, 10.0, 12.0, 37.0, 42.0, 31.0, 33.0, 29.0, 27.0, 42.0 } },
            { RiskType::CreditNonQ, { 20.0, 320.0, 320.0 } },
            { RiskType::Equity, { 7.6, 8.4, 9.7, 8.4, 6.8, 7.4, 8.7, 8.7, 9.9, 9.6, 7.0, 7.0, 9.9 } },
            { RiskType::Commodity, { 5.6, 8.2, 5.8, 6.4, 10.0, 9.1, 8.0, 12.3, 6.9, 12.4, 6.4, 5.7, 5.0, 4.8, 3.9, 12.4, 4.7 } },
            { RiskType::EquityVol, { 0.066, 0.066, 0.066, 0.066, 0.066, 0.066, 0.066, 0.066, 0.066, 0.066, 0.066, 0.21, 0.066 } }, 
        };
        
        rwLabel_1_ = {
            { { RiskType::IRCurve, "1" }, { 18.0, 16.0, 11.0, 11.0, 13.0, 14.0, 14.0, 16.0, 16.0, 16.0, 16.0, 16.0 } },
            { { RiskType::IRCurve, "2" }, { 3.5, 3.4, 1.7, 1.5, 3.2, 4.4, 5.1, 6.6, 7.3, 7.1, 7.8, 8.6 } },
            { { RiskType::IRCurve, "3" }, { 41.0, 24.0, 17.0, 17.0, 20.0, 23.0, 24.0, 27.0, 30.0, 28.0, 29.0, 29.0 } }
        };

        // Historical volatility ratios 
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.55;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.69;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.82;
        hvr_ir_ = 0.59;

        // Curvature weights
        //SIMM:Technical Paper, Section I.3, this 10-day formula for curvature weights is modified
        curvatureWeights_ = {
            { RiskType::IRVol, { 0.5 / 10.0, 
                                 0.5 * 1.40 / (365.0 / 12.0), 
                                 0.5 * 1.40 / (3.0 * 365.0 / 12.0), 
                                 0.5 * 1.40 / (6.0 * 365.0 / 12.0), 
                                 0.5 * 1.40 / 365.0, 
                                 0.5 * 1.40 / (2.0 * 365.0), 
                                 0.5 * 1.40 / (3.0 * 365.0), 
                                 0.5 * 1.40 / (5.0 * 365.0), 
                                 0.5 * 1.40 / (10.0 * 365.0), 
                                 0.5 * 1.40 / (15.0 * 365.0), 
                                 0.5 * 1.40 / (20.0 * 365.0), 
                                 0.5 * 1.40 / (30.0 * 365.0) } 
            },
            { RiskType::CreditVol, { 0.5 * 1.40 / 365.0, 
                                     0.5 * 1.40 / (2.0 * 365.0), 
                                     0.5 * 1.40 / (3.0 * 365.0), 
                                     0.5 * 1.40 / (5.0 * 365.0), 
                                     0.5 * 1.40 / (10.0 * 365.0) } 
            }
        };
        curvatureWeights_[RiskType::InflationVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::EquityVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::CommodityVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::FXVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::CreditVolNonQ] = curvatureWeights_[RiskType::CreditVol];
        
    }
    

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
    temp = {
        1.00, 0.27, 0.26, 0.29, 0.31, 0.14,
        0.27, 1.00, 0.19, 0.63, 0.42, 0.25,
        0.26, 0.19, 1.00, 0.19, 0.20, 0.14,
        0.29, 0.63, 0.19, 1.00, 0.43, 0.25,
        0.31, 0.42, 0.20, 0.43, 1.00, 0.30,
        0.14, 0.25, 0.14, 0.25, 0.30, 1.00
    };
    riskClassCorrelation_ = Matrix(6, 6, temp.begin(), temp.end());

    // FX correlations
    temp = {
        0.500, 0.368,
        0.368, 0.500
    };
    fxRegVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    temp = {
        0.729, 0.500,
        0.500, 0.500
    };
    fxHighVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations) 
    temp = {
        1.00, 0.69, 0.64, 0.55, 0.34, 0.21, 0.16, 0.11, 0.06, 0.01, 0.00, -0.02,
        0.69, 1.00, 0.79, 0.65, 0.50, 0.39, 0.33, 0.26, 0.20, 0.14, 0.11, 0.09,
        0.64, 0.79, 1.00, 0.85, 0.67, 0.53, 0.46, 0.37, 0.29, 0.22, 0.19, 0.16,
        0.55, 0.65, 0.85, 1.00, 0.85, 0.72, 0.65, 0.56, 0.46, 0.38, 0.34, 0.30,
        0.34, 0.50, 0.67, 0.85, 1.00, 0.93, 0.88, 0.78, 0.66, 0.60, 0.56, 0.52,
        0.21, 0.39, 0.53, 0.72, 0.93, 1.00, 0.98, 0.91, 0.81, 0.75, 0.72, 0.68,
        0.16, 0.33, 0.46, 0.65, 0.88, 0.98, 1.00, 0.96, 0.87, 0.83, 0.80, 0.76,
        0.11, 0.26, 0.37, 0.56, 0.78, 0.91, 0.96, 1.00, 0.95, 0.92, 0.89, 0.86,
        0.06, 0.20, 0.29, 0.46, 0.66, 0.81, 0.87, 0.95, 1.00, 0.98, 0.97, 0.95,
        0.01, 0.14, 0.22, 0.38, 0.60, 0.75, 0.83, 0.92, 0.98, 1.00, 0.99, 0.98,
        0.00, 0.11, 0.19, 0.34, 0.56, 0.72, 0.80, 0.89, 0.97, 0.99, 1.00, 0.99,
        -0.02, 0.09, 0.16, 0.30, 0.52, 0.68, 0.76, 0.86, 0.95, 0.98, 0.99, 1.00
    };
    irTenorCorrelation_ = Matrix(12, 12, temp.begin(), temp.end());

    // CreditQ inter-bucket correlations 
    temp = {
        1.00, 0.43, 0.42, 0.41, 0.43, 0.39, 0.40, 0.35, 0.37, 0.37, 0.37, 0.32,
        0.43, 1.00, 0.46, 0.46, 0.47, 0.44, 0.34, 0.39, 0.41, 0.41, 0.41, 0.35,
        0.42, 0.46, 1.00, 0.47, 0.48, 0.45, 0.34, 0.38, 0.42, 0.42, 0.41, 0.36,
        0.41, 0.46, 0.47, 1.00, 0.48, 0.45, 0.34, 0.37, 0.40, 0.42, 0.41, 0.35,
        0.43, 0.47, 0.48, 0.48, 1.00, 0.47, 0.34, 0.37, 0.40, 0.42, 0.43, 0.36,
        0.39, 0.44, 0.45, 0.45, 0.47, 1.00, 0.32, 0.35, 0.38, 0.39, 0.39, 0.34,
        0.40, 0.34, 0.34, 0.34, 0.34, 0.32, 1.00, 0.26, 0.31, 0.30, 0.29, 0.26,
        0.35, 0.39, 0.38, 0.37, 0.37, 0.35, 0.26, 1.00, 0.34, 0.33, 0.33, 0.30,
        0.37, 0.41, 0.42, 0.40, 0.40, 0.38, 0.31, 0.34, 1.00, 0.37, 0.36, 0.33,
        0.37, 0.41, 0.42, 0.42, 0.42, 0.39, 0.30, 0.33, 0.37, 1.00, 0.38, 0.32,
        0.37, 0.41, 0.41, 0.41, 0.43, 0.39, 0.29, 0.33, 0.36, 0.38, 1.00, 0.32,
        0.32, 0.35, 0.36, 0.35, 0.36, 0.34, 0.26, 0.30, 0.33, 0.32, 0.32, 1.00
    };
    interBucketCorrelation_[RiskType::CreditQ] = Matrix(12, 12, temp.begin(), temp.end());

    // Equity inter-bucket correlations
    temp = {
        1.00, 0.17, 0.18, 0.19, 0.12, 0.14, 0.15, 0.15, 0.15, 0.12, 0.18, 0.18,
        0.17, 1.00, 0.22, 0.22, 0.14, 0.17, 0.18, 0.18, 0.18, 0.14, 0.22, 0.22,
        0.18, 0.22, 1.00, 0.24, 0.14, 0.20, 0.23, 0.20, 0.19, 0.16, 0.25, 0.25,
        0.19, 0.22, 0.24, 1.00, 0.16, 0.21, 0.21, 0.22, 0.19, 0.16, 0.25, 0.25,
        0.12, 0.14, 0.14, 0.16, 1.00, 0.23, 0.21, 0.24, 0.13, 0.17, 0.26, 0.26,
        0.14, 0.17, 0.20, 0.21, 0.23, 1.00, 0.29, 0.30, 0.16, 0.22, 0.32, 0.32,
        0.15, 0.18, 0.23, 0.21, 0.21, 0.29, 1.00, 0.29, 0.16, 0.22, 0.33, 0.33,
        0.15, 0.18, 0.20, 0.22, 0.24, 0.30, 0.29, 1.00, 0.17, 0.22, 0.33, 0.33,
        0.15, 0.18, 0.19, 0.19, 0.13, 0.16, 0.16, 0.17, 1.00, 0.13, 0.20, 0.20,
        0.12, 0.14, 0.16, 0.16, 0.17, 0.22, 0.22, 0.22, 0.13, 1.00, 0.23, 0.23,
        0.18, 0.22, 0.25, 0.25, 0.26, 0.32, 0.33, 0.33, 0.20, 0.23, 1.00, 0.40,
        0.18, 0.22, 0.25, 0.25, 0.26, 0.32, 0.33, 0.33, 0.20, 0.23, 0.40, 1.00
    };
    interBucketCorrelation_[RiskType::Equity] = Matrix(12, 12, temp.begin(), temp.end());

    // Commodity inter-bucket correlations
    temp = {
        1.00, 0.15, 0.13, 0.17, 0.16, 0.02, 0.19, -0.02, 0.19, 0.01, 0.12, 0.12, -0.01, 0.11, 0.02, 0.00, 0.09,
        0.15, 1.00, 0.93, 0.95, 0.93, 0.23, 0.20, 0.20, 0.20, 0.06, 0.36, 0.23, 0.30, 0.28, 0.10, 0.00, 0.63,
        0.13, 0.93, 1.00, 0.90, 0.89, 0.15, 0.18, 0.13, 0.14, 0.02, 0.22, 0.08, 0.21, 0.13, 0.02, 0.00, 0.58,
        0.17, 0.95, 0.90, 1.00, 0.90, 0.18, 0.22, 0.16, 0.20, 0.00, 0.35, 0.18, 0.27, 0.25, 0.05, 0.00, 0.60,
        0.16, 0.93, 0.89, 0.90, 1.00, 0.23, 0.21, 0.20, 0.19, 0.09, 0.38, 0.25, 0.33, 0.27, 0.09, 0.00, 0.62,
        0.02, 0.23, 0.15, 0.18, 0.23, 1.00, 0.18, 0.63, 0.16, 0.02, 0.13, -0.02, 0.18, 0.10, -0.03, 0.00, 0.27,
        0.19, 0.20, 0.18, 0.22, 0.21, 0.18, 1.00, 0.08, 0.62, 0.11, 0.11, 0.01, 0.07, 0.10, 0.02, 0.00, 0.15,
        -0.02, 0.20, 0.13, 0.16, 0.20, 0.63, 0.08, 1.00, 0.16, -0.04, 0.09, 0.02, 0.10, 0.06, 0.03, 0.00, 0.20,
        0.19, 0.20, 0.14, 0.20, 0.19, 0.16, 0.62, 0.16, 1.00, 0.01, 0.11, 0.0, 0.10, 0.09, 0.05, 0.00, 0.13,
        0.01, 0.06, 0.02, 0.00, 0.09, 0.02, 0.11, -0.04, 0.01, 1.00, 0.07, 0.05, 0.07, 0.07, 0.01, 0.00, 0.01,
        0.12, 0.36, 0.22, 0.35, 0.38, 0.13, 0.11, 0.09, 0.11, 0.07, 1.00, 0.35, 0.21, 0.20, 0.10, 0.00, 0.31,
        0.12, 0.23, 0.08, 0.18, 0.25, -0.02, 0.01, 0.02, 0.00, 0.05, 0.35, 1.00, 0.11, 0.24, 0.05, 0.00, 0.16,
        -0.01, 0.30, 0.21, 0.27, 0.33, 0.18, 0.07, 0.10, 0.10, 0.07, 0.21, 0.11, 1.00, 0.27, 0.13, 0.00, 0.35,
        0.11, 0.28, 0.13, 0.25, 0.27, 0.10, 0.10, 0.06, 0.09, 0.07, 0.20, 0.24, 0.27, 1.00, 0.08, 0.00, 0.21,
        0.02, 0.10, 0.02, 0.05, 0.09, -0.03, 0.02, 0.03, 0.05, 0.01, 0.10, 0.05, 0.13, 0.08, 1.00, 0.00, 0.12,
        0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1.00, 0.00,
        0.09, 0.63, 0.58, 0.60, 0.62, 0.27, 0.15, 0.20, 0.13, 0.01, 0.31, 0.16, 0.35, 0.21, 0.12, 0.00, 1.00
    };
    interBucketCorrelation_[RiskType::Commodity] = Matrix(17, 17, temp.begin(), temp.end());

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[RiskType::Equity] = { 0.16, 0.21, 0.28, 0.25, 0.20, 0.30, 0.33,
        0.31, 0.18, 0.18, 0.40, 0.40 };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[RiskType::Commodity] = { 0.28, 0.98, 0.92, 0.97, 0.99, 0.74, 0.87, 0.35, 0.69, 
        0.14, 0.53, 0.63, 0.61, 0.18, 0.15, 0.00, 0.36 };

    // Initialise the single, ad-hoc type, correlations 
    xccyCorr_ = 0.13;
    infCorr_ = 0.39;
    infVolCorr_ = 0.39;
    irSubCurveCorr_ = 0.985;
    irInterCurrencyCorr_ = 0.22;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.95;
    crqDiffIntraCorr_ = 0.41;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.43;
    crnqDiffIntraCorr_ = 0.15;
    crnqInterCorr_ = 0.17;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.14;

    // clang-format on
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 5 section 11
of the ISDA-SIMM-v2.1 documentation).
*/
QuantLib::Real SimmConfiguration_ISDA_V2_2::curvatureMarginScaling() const { return pow(hvr_ir_, -2.0); }

void SimmConfiguration_ISDA_V2_2::addLabels2(const RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V2_2::labels2(const boost::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::labels2(irIndex);
}

} // namespace analytics
} // namespace ore
