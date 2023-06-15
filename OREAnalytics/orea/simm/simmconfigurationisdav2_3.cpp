/*
 Copyright (C) 2020 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav2_3.hpp>
#include <orea/simm/simmconfigurationisdav2_3.hpp>
#include <ql/math/matrix.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/make_shared.hpp>

using QuantLib::InterestRateIndex;
using QuantLib::Matrix;
using QuantLib::Real;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

QuantLib::Size SimmConfiguration_ISDA_V2_3::group(const string& qualifier,
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

QuantLib::Real SimmConfiguration_ISDA_V2_3::weight(const RiskType& rt, boost::optional<string> qualifier,
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

QuantLib::Real SimmConfiguration_ISDA_V2_3::correlation(const RiskType& firstRt, const string& firstQualifier,
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

SimmConfiguration_ISDA_V2_3::SimmConfiguration_ISDA_V2_3(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const QuantLib::Size& mporDays, const std::string& name,
                                                         const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = boost::make_shared<SimmConcentration_ISDA_V2_3>(simmBucketMapper_);
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
        { 1, { } },
        { 0, { } },
    };

    vector<Real> temp;

    if (mporDays_ == 10) {
        // Risk weights
        temp = {
            7.5, 7.5, 
            7.5, 7.5
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 50 },
            { RiskType::XCcyBasis, 22 },
            { RiskType::IRVol, 0.16 },
            { RiskType::InflationVol, 0.16 },   
            { RiskType::CreditVol, 0.46 },
            { RiskType::CreditVolNonQ, 0.46 },
            { RiskType::CommodityVol, 0.41 },
            { RiskType::FXVol, 0.30 },
            { RiskType::BaseCorr, 12.0 }
        };

        rwBucket_ = {
            { RiskType::CreditQ, { 75.0, 89.0, 68.0, 51.0, 50.0, 47.0, 157.0, 333.0, 142.0, 214.0, 143.0, 160.0, 333.0 } },
            { RiskType::CreditNonQ, { 240.0, 1000.0, 1000.0 } },
            { RiskType::Equity, { 23.0, 25.0, 29.0, 26.0, 20.0, 21.0, 24.0, 24.0, 29.0, 28.0, 15.0, 15.0, 29.0 } },
            { RiskType::Commodity, { 16.0, 20.0, 23.0, 18.0, 28.0, 18.0, 17.0, 57.0, 21.0, 39.0, 20.0, 20.0, 15.0, 15.0, 11.0, 57.0, 16.0 } },
            { RiskType::EquityVol, { 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.26, 0.67, 0.26 } }, 
        };
        
        rwLabel_1_ = {
            { { RiskType::IRCurve, "1" }, { 114.0, 107.0, 95.0, 71.0, 56.0, 53.0, 50.0, 51.0, 53.0, 50.0, 54.0, 63.0 } },
            { { RiskType::IRCurve, "2" }, { 15.0, 21.0, 10.0, 10.0, 11.0, 15.0, 18.0, 19.0, 19.0, 18.0, 20.0, 22.0 } },
            { { RiskType::IRCurve, "3" }, { 103.0, 96.0, 84.0, 84.0, 89.0, 87.0, 90.0, 89.0, 90.0, 99.0, 100.0, 96.0 } }
        };
        
        // Historical volatility ratios 
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.60;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.75;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.58;
        hvr_ir_ = 0.49;
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
            1.8, 1.8, 
            1.8, 1.8 
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 14.0 },
            { RiskType::XCcyBasis, 5.4 },
            { RiskType::IRVol, 0.042 },
            { RiskType::InflationVol, 0.042 },   
            { RiskType::CreditVol, 0.091 },
            { RiskType::CreditVolNonQ, 0.091 },
            { RiskType::CommodityVol, 0.13 },
            { RiskType::FXVol, 0.077 },
            { RiskType::BaseCorr, 2.6 }
        };

        rwBucket_ = {
            { RiskType::CreditQ, { 17.0, 22.0, 14.0, 11.0, 11.0, 9.5, 39.0, 63.0, 28.0, 45.0, 32.0, 35.0, 63.0 } },
            { RiskType::CreditNonQ, { 83.0, 220.0, 220.0 } },
            { RiskType::Equity, { 7.6, 8.1, 9.4, 8.3, 6.8, 7.4, 8.5, 8.6, 9.4, 9.3, 5.5, 5.5, 9.4 } },
            { RiskType::Commodity, { 5.7, 7.0, 7.0, 5.5, 8.9, 6.6, 6.5, 13.0, 6.8, 13, 6.0, 5.7, 4.9, 4.6, 3.0, 13.0, 4.8} },
            { RiskType::EquityVol, { 0.065, 0.065, 0.065, 0.065, 0.065, 0.065, 0.065, 0.065, 0.065, 0.065, 0.065, 0.22, 0.065 } }, 
        };
        
        rwLabel_1_ = {
            { { RiskType::IRCurve, "1" }, { 18.0, 15.0, 10.0, 11.0, 13.0, 15.0, 16.0, 16.0, 16.0, 16.0, 16.0, 17.0 } },
            { { RiskType::IRCurve, "2" }, { 1.7, 3.4, 1.9, 1.5, 2.8, 4.5, 4.8, 6.1, 6.3, 6.8, 7.3, 7.5 } },
            { { RiskType::IRCurve, "3" }, { 29.0, 35.0, 18.0, 20.0, 21.0, 23.0, 33.0, 32.0, 31.0, 26.0, 27.0, 26.0 } }
        };

        // Historical volatility ratios 
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.54;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.73;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.76;
        hvr_ir_ = 0.53;

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
        1.00, 0.29, 0.28, 0.31, 0.35, 0.18,
        0.29, 1.00, 0.51, 0.61, 0.43, 0.35,
        0.28, 0.51, 1.00, 0.47, 0.34, 0.18,
        0.31, 0.61, 0.47, 1.00, 0.47, 0.30,
        0.35, 0.43, 0.34, 0.47, 1.00, 0.31,
        0.18, 0.35, 0.18, 0.30, 0.31, 1.00
    };
    riskClassCorrelation_ = Matrix(6, 6, temp.begin(), temp.end());

    // FX correlations
    temp = {
        0.500, 0.500,
        0.500, 0.500
    };
    fxRegVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    temp = {
        0.500, 0.500,
        0.500, 0.500
    };
    fxHighVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations) 
    temp = {
        1.00, 0.73, 0.64, 0.57, 0.44, 0.34, 0.29, 0.24, 0.18, 0.13, 0.11, 0.09,
        0.73, 1.00, 0.78, 0.67, 0.50, 0.37, 0.30, 0.24, 0.18, 0.13, 0.11, 0.10,
        0.64, 0.78, 1.00, 0.85, 0.66, 0.52, 0.43, 0.35, 0.27, 0.20, 0.17, 0.17,
        0.57, 0.67, 0.85, 1.00, 0.81, 0.68, 0.59, 0.50, 0.41, 0.35, 0.33, 0.31,
        0.44, 0.50, 0.66, 0.81, 1.00, 0.94, 0.85, 0.76, 0.65, 0.59, 0.56, 0.54,
        0.34, 0.37, 0.52, 0.68, 0.94, 1.00, 0.95, 0.89, 0.79, 0.75, 0.72, 0.70,
        0.29, 0.30, 0.43, 0.59, 0.85, 0.95, 1.00, 0.96, 0.88, 0.83, 0.80, 0.78,
        0.24, 0.24, 0.35, 0.50, 0.76, 0.89, 0.96, 1.00, 0.95, 0.91, 0.88, 0.87,
        0.18, 0.18, 0.27, 0.41, 0.65, 0.79, 0.88, 0.95, 1.00, 0.97, 0.95, 0.95,
        0.13, 0.13, 0.20, 0.35, 0.59, 0.75, 0.83, 0.91, 0.97, 1.00, 0.98, 0.98,
        0.11, 0.11, 0.17, 0.33, 0.56, 0.72, 0.80, 0.88, 0.95, 0.98, 1.00, 0.99,
        0.09, 0.10, 0.17, 0.31, 0.54, 0.70, 0.78, 0.87, 0.95, 0.98, 0.99, 1.00
    };
    irTenorCorrelation_ = Matrix(12, 12, temp.begin(), temp.end());

    // CreditQ inter-bucket correlations 
    temp = {
        1.00, 0.39, 0.39, 0.40, 0.40, 0.37, 0.39, 0.32, 0.35, 0.33, 0.33, 0.30,
        0.39, 1.00, 0.43, 0.45, 0.45, 0.43, 0.32, 0.34, 0.39, 0.36, 0.36, 0.31,
        0.39, 0.43, 1.00, 0.47, 0.48, 0.45, 0.33, 0.33, 0.41, 0.38, 0.39, 0.35,
        0.40, 0.45, 0.47, 1.00, 0.49, 0.47, 0.33, 0.34, 0.41, 0.40, 0.39, 0.34,
        0.40, 0.45, 0.48, 0.49, 1.00, 0.48, 0.33, 0.34, 0.41, 0.39, 0.40, 0.34,
        0.37, 0.43, 0.45, 0.47, 0.48, 1.00, 0.31, 0.32, 0.38, 0.36, 0.37, 0.32,
        0.39, 0.32, 0.33, 0.33, 0.33, 0.31, 1.00, 0.26, 0.31, 0.28, 0.27, 0.25,
        0.32, 0.34, 0.33, 0.34, 0.34, 0.32, 0.26, 1.00, 0.31, 0.28, 0.28, 0.25,
        0.35, 0.39, 0.41, 0.41, 0.41, 0.38, 0.31, 0.31, 1.00, 0.35, 0.34, 0.32,
        0.33, 0.36, 0.38, 0.40, 0.39, 0.36, 0.28, 0.28, 0.35, 1.00, 0.34, 0.29,
        0.33, 0.36, 0.39, 0.39, 0.40, 0.37, 0.27, 0.28, 0.34, 0.34, 1.00, 0.30,
        0.30, 0.31, 0.35, 0.34, 0.34, 0.32, 0.25, 0.25, 0.32, 0.29, 0.30, 1.00
    };
    interBucketCorrelation_[RiskType::CreditQ] = Matrix(12, 12, temp.begin(), temp.end());

    // Equity inter-bucket correlations
    temp = {
        1.00, 0.17, 0.18, 0.17, 0.11, 0.15, 0.15, 0.15, 0.15, 0.11, 0.19, 0.19,
        0.17, 1.00, 0.23, 0.21, 0.13, 0.17, 0.18, 0.17, 0.19, 0.13, 0.22, 0.22,
        0.18, 0.23, 1.00, 0.24, 0.14, 0.19, 0.22, 0.19, 0.20, 0.15, 0.25, 0.25,
        0.17, 0.21, 0.24, 1.00, 0.14, 0.19, 0.20, 0.19, 0.20, 0.15, 0.24, 0.24,
        0.11, 0.13, 0.14, 0.14, 1.00, 0.22, 0.21, 0.22, 0.13, 0.16, 0.24, 0.24,
        0.15, 0.17, 0.19, 0.19, 0.22, 1.00, 0.29, 0.29, 0.17, 0.21, 0.32, 0.32,
        0.15, 0.18, 0.22, 0.20, 0.21, 0.29, 1.00, 0.28, 0.17, 0.21, 0.34, 0.34,
        0.15, 0.17, 0.19, 0.19, 0.22, 0.29, 0.28, 1.00, 0.17, 0.21, 0.33, 0.33,
        0.15, 0.19, 0.20, 0.20, 0.13, 0.17, 0.17, 0.17, 1.00, 0.13, 0.21, 0.21,
        0.11, 0.13, 0.15, 0.15, 0.16, 0.21, 0.21, 0.21, 0.13, 1.00, 0.22, 0.22,
        0.19, 0.22, 0.25, 0.24, 0.24, 0.32, 0.34, 0.33, 0.21, 0.22, 1.00, 0.41,
        0.19, 0.22, 0.25, 0.24, 0.24, 0.32, 0.34, 0.33, 0.21, 0.22, 0.41, 1.00
    };
    interBucketCorrelation_[RiskType::Equity] = Matrix(12, 12, temp.begin(), temp.end());

    // Commodity inter-bucket correlations
    temp = {
        1.00, 0.15, 0.15, 0.21, 0.16, 0.03, 0.11, 0.02, 0.12, 0.15, 0.15, 0.06, 0.00, 0.04, 0.06, 0.00, 0.11,
        0.15, 1.00, 0.74, 0.92, 0.89, 0.34, 0.23, 0.16, 0.22, 0.26, 0.31, 0.32, 0.22, 0.25, 0.19, 0.00, 0.57,
        0.15, 0.74, 1.00, 0.73, 0.69, 0.15, 0.22, 0.08, 0.14, 0.16, 0.21, 0.15, -0.03, 0.16, 0.14, 0.00, 0.42,
        0.21, 0.92, 0.73, 1.00, 0.83, 0.30, 0.26, 0.07, 0.19, 0.22, 0.28, 0.31, 0.13, 0.22, 0.11, 0.00, 0.48,
        0.16, 0.89, 0.69, 0.83, 1.00, 0.12, 0.14, 0.00, 0.06, 0.10, 0.24, 0.20, 0.06, 0.20, 0.09, 0.00, 0.49,
        0.03, 0.34, 0.15, 0.30, 0.12, 1.00, 0.25, 0.58, 0.21, 0.14, 0.23, 0.15, 0.25, 0.15, 0.12, 0.00, 0.37,
        0.11, 0.23, 0.22, 0.26, 0.14, 0.25, 1.00, 0.19, 0.64, 0.19, 0.03, -0.03, 0.04, 0.05, 0.06, 0.00, 0.17,
        0.02, 0.16, 0.08, 0.07, 0.00, 0.58, 0.19, 1.00, 0.17, -0.01, 0.08, 0.01, 0.11, 0.08, 0.08, 0.00, 0.15,
        0.12, 0.22, 0.14, 0.19, 0.06, 0.21, 0.64, 0.17, 1.00, 0.10, 0.05, -0.04, 0.05, 0.03, 0.05, 0.00, 0.17,
        0.15, 0.26, 0.16, 0.22, 0.10, 0.14, 0.19, -0.01, 0.10, 1.00, 0.12, 0.13, 0.12, 0.10, 0.12, 0.00, 0.17,
        0.15, 0.31, 0.21, 0.28, 0.24, 0.23, 0.03, 0.08, 0.05, 0.12, 1.00, 0.34, 0.23, 0.17, 0.14, 0.00, 0.30,
        0.06, 0.32, 0.15, 0.31, 0.20, 0.15, -0.03, 0.01, -0.04, 0.13, 0.34, 1.00, 0.15, 0.19, 0.11, 0.00, 0.27,
        0.00, 0.22, -0.03, 0.13, 0.06, 0.25, 0.04, 0.11, 0.05, 0.12, 0.23, 0.15, 1.00, 0.26, 0.14, 0.00, 0.26,
        0.04, 0.25, 0.16, 0.22, 0.20, 0.15, 0.05, 0.08, 0.03, 0.10, 0.17, 0.19, 0.26, 1.00, 0.10, 0.00, 0.22,
        0.06, 0.19, 0.14, 0.11, 0.09, 0.12, 0.06, 0.08, 0.05, 0.12, 0.14, 0.11, 0.14, 0.10, 1.00, 0.00, 0.20,
        0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1.00, 0.00,
        0.11, 0.57, 0.42, 0.48, 0.49, 0.37, 0.17, 0.15, 0.17, 0.17, 0.30, 0.27, 0.26, 0.22, 0.20, 0.00, 1.00
    };
    interBucketCorrelation_[RiskType::Commodity] = Matrix(17, 17, temp.begin(), temp.end());

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[RiskType::Equity] = { 0.14, 0.20, 0.28, 0.23, 0.18, 0.29, 0.34,
        0.30, 0.19, 0.17, 0.41, 0.41 };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[RiskType::Commodity] = { 0.16, 0.98, 0.77, 0.82, 0.98, 0.89, 0.96, 0.48, 0.64, 
        0.39, 0.45, 0.53, 0.65, 0.12, 0.21, 0.00, 0.35 };

    // Initialise the single, ad-hoc type, correlations 
    xccyCorr_ = 0.04;
    infCorr_ = 0.42;
    infVolCorr_ = 0.42;
    irSubCurveCorr_ = 0.986;
    irInterCurrencyCorr_ = 0.20;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.94;
    crqDiffIntraCorr_ = 0.42;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.77;
    crnqDiffIntraCorr_ = 0.47;
    crnqInterCorr_ = 0.69;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.18;

    // clang-format on
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 8 section 11(d)
of the ISDA-SIMM-v2.3 documentation).
*/
QuantLib::Real SimmConfiguration_ISDA_V2_3::curvatureMarginScaling() const { return pow(hvr_ir_, -2.0); }

void SimmConfiguration_ISDA_V2_3::addLabels2(const RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V2_3::labels2(const boost::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::labels2(irIndex);
}

} // namespace analytics
} // namespace ore
