/*
 Copyright (C) 2024 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav2_6_5.hpp>
#include <orea/simm/simmconfigurationisdav2_6_5.hpp>
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
QuantLib::Size SimmConfiguration_ISDA_V2_6_5::group(const string& qualifier, const std::map<QuantLib::Size,
                                                  std::set<string>>& categories) const {
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

QuantLib::Real SimmConfiguration_ISDA_V2_6_5::weight(const CrifRecord::RiskType& rt, boost::optional<string> qualifier,
                                                     boost::optional<std::string> label_1,
                                                     const std::string& calculationCurrency) const {

    if (rt == CrifRecord::RiskType::FX) {
        QL_REQUIRE(calculationCurrency != "", "no calculation currency provided weight");
        QL_REQUIRE(qualifier, "need a qualifier to return a risk weight for the risk type FX");

        QuantLib::Size g1 = group(calculationCurrency, ccyGroups_);
        QuantLib::Size g2 = group(*qualifier, ccyGroups_);
        return rwFX_[g1][g2];
    }

    return SimmConfigurationBase::weight(rt, qualifier, label_1);
}

QuantLib::Real SimmConfiguration_ISDA_V2_6_5::correlation(const CrifRecord::RiskType& firstRt, const string& firstQualifier,
                                                        const string& firstLabel_1, const string& firstLabel_2,
                                                        const CrifRecord::RiskType& secondRt, const string& secondQualifier,
                                                        const string& secondLabel_1, const string& secondLabel_2,
                                                        const std::string& calculationCurrency) const {

    if (firstRt == CrifRecord::RiskType::FX && secondRt == CrifRecord::RiskType::FX) {
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

SimmConfiguration_ISDA_V2_6_5::SimmConfiguration_ISDA_V2_6_5(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const QuantLib::Size& mporDays, const std::string& name,
                                                         const std::string version)
     : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = boost::make_shared<SimmConcentration_ISDA_V2_6_5>(simmBucketMapper_);
    } else {
        // SIMM:Technical Paper, Section I.4: "The Concentration Risk feature is disabled"
        simmConcentration_ = boost::make_shared<SimmConcentrationBase>();
    }

    // clang-format off

    // Set up the members for this configuration
    // Explanations of all these members are given in the hpp file

    mapBuckets_ = {
        { CrifRecord::RiskType::IRCurve, { "1", "2", "3" } },
        { CrifRecord::RiskType::CreditQ, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { CrifRecord::RiskType::CreditVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { CrifRecord::RiskType::CreditNonQ, { "1", "2", "Residual" } },
        { CrifRecord::RiskType::CreditVolNonQ, { "1", "2", "Residual" } },
        { CrifRecord::RiskType::Equity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { CrifRecord::RiskType::EquityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { CrifRecord::RiskType::Commodity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17" } },
        { CrifRecord::RiskType::CommodityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17" } }
    };

    mapLabels_1_ = {
        { CrifRecord::RiskType::IRCurve, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { CrifRecord::RiskType::CreditQ, { "1y", "2y", "3y", "5y", "10y" } },
        { CrifRecord::RiskType::CreditNonQ, { "1y", "2y", "3y", "5y", "10y" } },
        { CrifRecord::RiskType::IRVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { CrifRecord::RiskType::InflationVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { CrifRecord::RiskType::CreditVol, { "1y", "2y", "3y", "5y", "10y" } },
        { CrifRecord::RiskType::CreditVolNonQ, { "1y", "2y", "3y", "5y", "10y" } },
        { CrifRecord::RiskType::EquityVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { CrifRecord::RiskType::CommodityVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { CrifRecord::RiskType::FXVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } }
    };

    mapLabels_2_ = {
        { CrifRecord::RiskType::IRCurve, { "OIS", "Libor1m", "Libor3m", "Libor6m", "Libor12m", "Prime", "Municipal" } },
        { CrifRecord::RiskType::CreditQ, { "", "Sec" } }
    };

    // Populate CCY groups that are used for FX correlations and risk weights
    // The groups consists of High Vol Currencies & regular vol currencies
    ccyGroups_ = {
        { 1, { "ARS", "RUB", "TRY" } },
        { 0, {  } }
    };

    vector<Real> temp;

    if (mporDays_ == 10) {
        // Risk weights
        temp = {
           7.3,  21.4,
           21.4, 35.9
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { CrifRecord::RiskType::Inflation, 52 },
            { CrifRecord::RiskType::XCcyBasis, 21 },
            { CrifRecord::RiskType::IRVol, 0.20 },
            { CrifRecord::RiskType::InflationVol, 0.20 },
            { CrifRecord::RiskType::CreditVol, 0.29 },
            { CrifRecord::RiskType::CreditVolNonQ, 0.29 },
            { CrifRecord::RiskType::CommodityVol, 0.34 },
            { CrifRecord::RiskType::FXVol, 0.35 },
            { CrifRecord::RiskType::BaseCorr, 9.9 }
        };

        rwBucket_ = {
            { CrifRecord::RiskType::CreditQ, {
                { {"1", "", ""}, 69 },
                { {"2", "", ""}, 75 },
                { {"3", "", ""}, 69 },
                { {"4", "", ""}, 47 },
                { {"5", "", ""}, 58 },
                { {"6", "", ""}, 48 },
                { {"7", "", ""}, 153 },
                { {"8", "", ""}, 363 },
                { {"9", "", ""}, 156 },
                { {"10", "", ""}, 188 },
                { {"11", "", ""}, 299 },
                { {"12", "", ""}, 119 },
                { {"Residual", "", ""}, 363 },
            }},
            { CrifRecord::RiskType::CreditNonQ, {
                { {"1", "", ""}, 210 },
                { {"2", "", ""}, 2900 },
                { {"Residual", "", ""}, 2900 },
            }},
            { CrifRecord::RiskType::Equity, {
                { {"1", "", ""}, 27 },
                { {"2", "", ""}, 30 },
                { {"3", "", ""}, 31 },
                { {"4", "", ""}, 27 },
                { {"5", "", ""}, 23 },
                { {"6", "", ""}, 24 },
                { {"7", "", ""}, 26 },
                { {"8", "", ""}, 27 },
                { {"9", "", ""}, 33 },
                { {"10", "", ""}, 39 },
                { {"11", "", ""}, 15 },
                { {"12", "", ""}, 15 },
                { {"Residual", "", ""}, 39 },
            }},
            { CrifRecord::RiskType::Commodity, {
                { {"1", "", ""}, 48 },
                { {"2", "", ""}, 21 },
                { {"3", "", ""}, 23 },
                { {"4", "", ""}, 20 },
                { {"5", "", ""}, 24 },
                { {"6", "", ""}, 33 },
                { {"7", "", ""}, 61 },
                { {"8", "", ""}, 45 },
                { {"9", "", ""}, 65 },
                { {"10", "", ""}, 45 },
                { {"11", "", ""}, 21 },
                { {"12", "", ""}, 19 },
                { {"13", "", ""}, 16 },
                { {"14", "", ""}, 16 },
                { {"15", "", ""}, 11 },
                { {"16", "", ""}, 65 },
                { {"17", "", ""}, 16 },
            }},
            { CrifRecord::RiskType::EquityVol, {
                { {"1", "", ""}, 0.25 },
                { {"2", "", ""}, 0.25 },
                { {"3", "", ""}, 0.25 },
                { {"4", "", ""}, 0.25 },
                { {"5", "", ""}, 0.25 },
                { {"6", "", ""}, 0.25 },
                { {"7", "", ""}, 0.25 },
                { {"8", "", ""}, 0.25 },
                { {"9", "", ""}, 0.25 },
                { {"10", "", ""}, 0.25 },
                { {"11", "", ""}, 0.25 },
                { {"12", "", ""}, 0.56 },
                { {"Residual", "", ""}, 0.25 },
            }},
        };

        rwLabel_1_ = {
            { CrifRecord::RiskType::IRCurve, {
                { {"1", "2w", ""}, 109 },
                { {"1", "1m", ""}, 106 },
                { {"1", "3m", ""}, 91 },
                { {"1", "6m", ""}, 69 },
                { {"1", "1y", ""}, 68 },
                { {"1", "2y", ""}, 68 },
                { {"1", "3y", ""}, 66 },
                { {"1", "5y", ""}, 61 },
                { {"1", "10y", ""}, 59 },
                { {"1", "15y", ""}, 56 },
                { {"1", "20y", ""}, 57 },
                { {"1", "30y", ""}, 65 },
                { {"2", "2w", ""}, 15 },
                { {"2", "1m", ""}, 21 },
                { {"2", "3m", ""}, 10 },
                { {"2", "6m", ""}, 10 },
                { {"2", "1y", ""}, 11 },
                { {"2", "2y", ""}, 15 },
                { {"2", "3y", ""}, 18 },
                { {"2", "5y", ""}, 23 },
                { {"2", "10y", ""}, 25 },
                { {"2", "15y", ""}, 23 },
                { {"2", "20y", ""}, 23 },
                { {"2", "30y", ""}, 25 },
                { {"3", "2w", ""}, 171 },
                { {"3", "1m", ""}, 102 },
                { {"3", "3m", ""}, 94 },
                { {"3", "6m", ""}, 96 },
                { {"3", "1y", ""}, 105 },
                { {"3", "2y", ""}, 96 },
                { {"3", "3y", ""}, 99 },
                { {"3", "5y", ""}, 93 },
                { {"3", "10y", ""}, 99 },
                { {"3", "15y", ""}, 100 },
                { {"3", "20y", ""}, 101 },
                { {"3", "30y", ""}, 96 },
            }}
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[CrifRecord::RiskType::EquityVol] = 0.62;
        historicalVolatilityRatios_[CrifRecord::RiskType::CommodityVol] = 0.85;
        historicalVolatilityRatios_[CrifRecord::RiskType::FXVol] = 0.62;
        hvr_ir_ = 0.69;

        // Curvature weights
        curvatureWeights_ = {
            { CrifRecord::RiskType::IRVol, { 0.5,
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
            { CrifRecord::RiskType::CreditVol, { 0.5 * 14.0 / 365.0,
                                     0.5 * 14.0 / (2.0 * 365.0),
                                     0.5 * 14.0 / (3.0 * 365.0),
                                     0.5 * 14.0 / (5.0 * 365.0),
                                     0.5 * 14.0 / (10.0 * 365.0) }
            }
        };
        curvatureWeights_[CrifRecord::RiskType::InflationVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
        curvatureWeights_[CrifRecord::RiskType::EquityVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
        curvatureWeights_[CrifRecord::RiskType::CommodityVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
        curvatureWeights_[CrifRecord::RiskType::FXVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
        curvatureWeights_[CrifRecord::RiskType::CreditVolNonQ] = curvatureWeights_[CrifRecord::RiskType::CreditVol];

    } else {
       // SIMM:Technical Paper, Section I.1: "All delta and vega risk weights should be replaced with the values for
       // one-day calibration given in the Calibration Results document."

        // Risk weights
        temp = {
           1.8,  4.0,
           4.0, 5.1
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { CrifRecord::RiskType::Inflation, 14 },
            { CrifRecord::RiskType::XCcyBasis, 6.2 },
            { CrifRecord::RiskType::IRVol, 0.051 },
            { CrifRecord::RiskType::InflationVol, 0.051 },
            { CrifRecord::RiskType::CreditVol, 0.075 },
            { CrifRecord::RiskType::CreditVolNonQ, 0.075 },
            { CrifRecord::RiskType::CommodityVol, 0.10 },
            { CrifRecord::RiskType::FXVol, 0.084 },
            { CrifRecord::RiskType::BaseCorr, 2.3 }
        };

        rwBucket_ = {
            { CrifRecord::RiskType::CreditQ, {
                { {"1", "", ""}, 19 },
                { {"2", "", ""}, 23 },
                { {"3", "", ""}, 16 },
                { {"4", "", ""}, 12 },
                { {"5", "", ""}, 13 },
                { {"6", "", ""}, 11 },
                { {"7", "", ""}, 40 },
                { {"8", "", ""}, 100 },
                { {"9", "", ""}, 38 },
                { {"10", "", ""}, 42 },
                { {"11", "", ""}, 50 },
                { {"12", "", ""}, 31 },
                { {"Residual", "", ""}, 100 },
            }},
            { CrifRecord::RiskType::CreditNonQ, {
                { {"1", "", ""}, 63 },
                { {"2", "", ""}, 670 },
                { {"Residual", "", ""}, 670 },
            }},
            { CrifRecord::RiskType::Equity, {
                { {"1", "", ""}, 8.6 },
                { {"2", "", ""}, 9.0 },
                { {"3", "", ""}, 9.4 },
                { {"4", "", ""}, 8.6 },
                { {"5", "", ""}, 7.6 },
                { {"6", "", ""}, 8.1 },
                { {"7", "", ""}, 9.1 },
                { {"8", "", ""}, 9.7 },
                { {"9", "", ""}, 9.5 },
                { {"10", "", ""}, 12 },
                { {"11", "", ""}, 5.3 },
                { {"12", "", ""}, 5.3 },
                { {"Residual", "", ""}, 12 },
            }},
            { CrifRecord::RiskType::Commodity, {
                { {"1", "", ""}, 11 },
                { {"2", "", ""}, 7.2 },
                { {"3", "", ""}, 7.1 },
                { {"4", "", ""}, 7.0 },
                { {"5", "", ""}, 8.1 },
                { {"6", "", ""}, 9.8 },
                { {"7", "", ""}, 18 },
                { {"8", "", ""}, 12 },
                { {"9", "", ""}, 15 },
                { {"10", "", ""}, 11 },
                { {"11", "", ""}, 6.5 },
                { {"12", "", ""}, 6.1 },
                { {"13", "", ""}, 5.5 },
                { {"14", "", ""}, 5.3 },
                { {"15", "", ""}, 3.3 },
                { {"16", "", ""}, 18 },
                { {"17", "", ""}, 4.9 },
            }},
            { CrifRecord::RiskType::EquityVol, {
                { {"1", "", ""}, 0.068 },
                { {"2", "", ""}, 0.068 },
                { {"3", "", ""}, 0.068 },
                { {"4", "", ""}, 0.068 },
                { {"5", "", ""}, 0.068 },
                { {"6", "", ""}, 0.068 },
                { {"7", "", ""}, 0.068 },
                { {"8", "", ""}, 0.068 },
                { {"9", "", ""}, 0.068 },
                { {"10", "", ""}, 0.068 },
                { {"11", "", ""}, 0.068 },
                { {"12", "", ""}, 0.20 },
                { {"Residual", "", ""}, 0.068 },
            }},
        };

        rwLabel_1_ = {
            { CrifRecord::RiskType::IRCurve, {
                { {"1", "2w", ""}, 18 },
                { {"1", "1m", ""}, 15 },
                { {"1", "3m", ""}, 12 },
                { {"1", "6m", ""}, 14 },
                { {"1", "1y", ""}, 18 },
                { {"1", "2y", ""}, 21 },
                { {"1", "3y", ""}, 22 },
                { {"1", "5y", ""}, 20 },
                { {"1", "10y", ""}, 19 },
                { {"1", "15y", ""}, 18 },
                { {"1", "20y", ""}, 18 },
                { {"1", "30y", ""}, 18 },
                { {"2", "2w", ""}, 1.7 },
                { {"2", "1m", ""}, 2.8 },
                { {"2", "3m", ""}, 1.6 },
                { {"2", "6m", ""}, 1.8 },
                { {"2", "1y", ""}, 3.5 },
                { {"2", "2y", ""}, 4.9 },
                { {"2", "3y", ""}, 6.0 },
                { {"2", "5y", ""}, 7.3 },
                { {"2", "10y", ""}, 8.5 },
                { {"2", "15y", ""}, 8.5 },
                { {"2", "20y", ""}, 8.5 },
                { {"2", "30y", ""}, 9.0 },
                { {"3", "2w", ""}, 58 },
                { {"3", "1m", ""}, 36 },
                { {"3", "3m", ""}, 26 },
                { {"3", "6m", ""}, 27 },
                { {"3", "1y", ""}, 30 },
                { {"3", "2y", ""}, 26 },
                { {"3", "3y", ""}, 38 },
                { {"3", "5y", ""}, 36 },
                { {"3", "10y", ""}, 36 },
                { {"3", "15y", ""}, 28 },
                { {"3", "20y", ""}, 31 },
                { {"3", "30y", ""}, 27 },
            }}
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[CrifRecord::RiskType::EquityVol] = 0.57;
        historicalVolatilityRatios_[CrifRecord::RiskType::CommodityVol] = 0.82;
        historicalVolatilityRatios_[CrifRecord::RiskType::FXVol] = 0.80;
        hvr_ir_ = 0.68;

        // Curvature weights
        //SIMM:Technical Paper, Section I.3, this 10-day formula for curvature weights is modified
        curvatureWeights_ = {
            { CrifRecord::RiskType::IRVol, { 0.5 / 10.0,
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
            { CrifRecord::RiskType::CreditVol, { 0.5 * 1.40 / 365.0,
                                     0.5 * 1.40 / (2.0 * 365.0),
                                     0.5 * 1.40 / (3.0 * 365.0),
                                     0.5 * 1.40 / (5.0 * 365.0),
                                     0.5 * 1.40 / (10.0 * 365.0) }
            }
        };
        curvatureWeights_[CrifRecord::RiskType::InflationVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
        curvatureWeights_[CrifRecord::RiskType::EquityVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
        curvatureWeights_[CrifRecord::RiskType::CommodityVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
        curvatureWeights_[CrifRecord::RiskType::FXVol] = curvatureWeights_[CrifRecord::RiskType::IRVol];
        curvatureWeights_[CrifRecord::RiskType::CreditVolNonQ] = curvatureWeights_[CrifRecord::RiskType::CreditVol];
    }

    // Valid risk types
    validRiskTypes_ = {
        CrifRecord::RiskType::Commodity,
        CrifRecord::RiskType::CommodityVol,
        CrifRecord::RiskType::CreditNonQ,
        CrifRecord::RiskType::CreditQ,
        CrifRecord::RiskType::CreditVol,
        CrifRecord::RiskType::CreditVolNonQ,
        CrifRecord::RiskType::Equity,
        CrifRecord::RiskType::EquityVol,
        CrifRecord::RiskType::FX,
        CrifRecord::RiskType::FXVol,
        CrifRecord::RiskType::Inflation,
        CrifRecord::RiskType::IRCurve,
        CrifRecord::RiskType::IRVol,
        CrifRecord::RiskType::InflationVol,
        CrifRecord::RiskType::BaseCorr,
        CrifRecord::RiskType::XCcyBasis,
        CrifRecord::RiskType::ProductClassMultiplier,
        CrifRecord::RiskType::AddOnNotionalFactor,
        CrifRecord::RiskType::PV,
        CrifRecord::RiskType::Notional,
        CrifRecord::RiskType::AddOnFixedAmount
    };

    // Risk class correlation matrix
    riskClassCorrelation_ = {
        { {"", "InterestRate", "CreditQualifying"}, 0.15 },
        { {"", "InterestRate", "CreditNonQualifying"}, 0.09 },
        { {"", "InterestRate", "Equity"}, 0.08 },
        { {"", "InterestRate", "Commodity"}, 0.33 },
        { {"", "InterestRate", "FX"}, 0.09 },
        { {"", "CreditQualifying", "InterestRate"}, 0.15 },
        { {"", "CreditQualifying", "CreditNonQualifying"}, 0.52 },
        { {"", "CreditQualifying", "Equity"}, 0.67 },
        { {"", "CreditQualifying", "Commodity"}, 0.23 },
        { {"", "CreditQualifying", "FX"}, 0.20 },
        { {"", "CreditNonQualifying", "InterestRate"}, 0.09 },
        { {"", "CreditNonQualifying", "CreditQualifying"}, 0.52 },
        { {"", "CreditNonQualifying", "Equity"}, 0.36 },
        { {"", "CreditNonQualifying", "Commodity"}, 0.16 },
        { {"", "CreditNonQualifying", "FX"}, 0.12 },
        { {"", "Equity", "InterestRate"}, 0.08 },
        { {"", "Equity", "CreditQualifying"}, 0.67 },
        { {"", "Equity", "CreditNonQualifying"}, 0.36 },
        { {"", "Equity", "Commodity"}, 0.34 },
        { {"", "Equity", "FX"}, 0.24 },
        { {"", "Commodity", "InterestRate"}, 0.33 },
        { {"", "Commodity", "CreditQualifying"}, 0.23 },
        { {"", "Commodity", "CreditNonQualifying"}, 0.16 },
        { {"", "Commodity", "Equity"}, 0.34 },
        { {"", "Commodity", "FX"}, 0.28 },
        { {"", "FX", "InterestRate"}, 0.09 },
        { {"", "FX", "CreditQualifying"}, 0.20 },
        { {"", "FX", "CreditNonQualifying"}, 0.12 },
        { {"", "FX", "Equity"}, 0.24 },
        { {"", "FX", "Commodity"}, 0.28 },
    };

    // FX correlations
    temp = {
        0.50, 0.17,
        0.17, -0.41
    };
    fxRegVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    temp = {
        0.94, 0.84,
        0.84, 0.50
    };
    fxHighVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations)
    intraBucketCorrelation_[CrifRecord::RiskType::IRCurve] = {
        { {"", "2w", "1m"}, 0.75 },
        { {"", "2w", "3m"}, 0.67 },
        { {"", "2w", "6m"}, 0.57 },
        { {"", "2w", "1y"}, 0.43 },
        { {"", "2w", "2y"}, 0.33 },
        { {"", "2w", "3y"}, 0.28 },
        { {"", "2w", "5y"}, 0.24 },
        { {"", "2w", "10y"}, 0.19 },
        { {"", "2w", "15y"}, 0.17 },
        { {"", "2w", "20y"}, 0.16 },
        { {"", "2w", "30y"}, 0.15 },
        { {"", "1m", "2w"}, 0.75 },
        { {"", "1m", "3m"}, 0.85 },
        { {"", "1m", "6m"}, 0.72 },
        { {"", "1m", "1y"}, 0.52 },
        { {"", "1m", "2y"}, 0.38 },
        { {"", "1m", "3y"}, 0.30 },
        { {"", "1m", "5y"}, 0.24 },
        { {"", "1m", "10y"}, 0.19 },
        { {"", "1m", "15y"}, 0.14 },
        { {"", "1m", "20y"}, 0.12 },
        { {"", "1m", "30y"}, 0.12 },
        { {"", "3m", "2w"}, 0.67 },
        { {"", "3m", "1m"}, 0.85 },
        { {"", "3m", "6m"}, 0.88 },
        { {"", "3m", "1y"}, 0.67 },
        { {"", "3m", "2y"}, 0.52 },
        { {"", "3m", "3y"}, 0.44 },
        { {"", "3m", "5y"}, 0.37 },
        { {"", "3m", "10y"}, 0.30 },
        { {"", "3m", "15y"}, 0.23 },
        { {"", "3m", "20y"}, 0.21 },
        { {"", "3m", "30y"}, 0.21 },
        { {"", "6m", "2w"}, 0.57 },
        { {"", "6m", "1m"}, 0.72 },
        { {"", "6m", "3m"}, 0.88 },
        { {"", "6m", "1y"}, 0.86 },
        { {"", "6m", "2y"}, 0.73 },
        { {"", "6m", "3y"}, 0.64 },
        { {"", "6m", "5y"}, 0.56 },
        { {"", "6m", "10y"}, 0.47 },
        { {"", "6m", "15y"}, 0.41 },
        { {"", "6m", "20y"}, 0.38 },
        { {"", "6m", "30y"}, 0.37 },
        { {"", "1y", "2w"}, 0.43 },
        { {"", "1y", "1m"}, 0.52 },
        { {"", "1y", "3m"}, 0.67 },
        { {"", "1y", "6m"}, 0.86 },
        { {"", "1y", "2y"}, 0.94 },
        { {"", "1y", "3y"}, 0.86 },
        { {"", "1y", "5y"}, 0.78 },
        { {"", "1y", "10y"}, 0.67 },
        { {"", "1y", "15y"}, 0.61 },
        { {"", "1y", "20y"}, 0.57 },
        { {"", "1y", "30y"}, 0.56 },
        { {"", "2y", "2w"}, 0.33 },
        { {"", "2y", "1m"}, 0.38 },
        { {"", "2y", "3m"}, 0.52 },
        { {"", "2y", "6m"}, 0.73 },
        { {"", "2y", "1y"}, 0.94 },
        { {"", "2y", "3y"}, 0.96 },
        { {"", "2y", "5y"}, 0.90 },
        { {"", "2y", "10y"}, 0.80 },
        { {"", "2y", "15y"}, 0.75 },
        { {"", "2y", "20y"}, 0.70 },
        { {"", "2y", "30y"}, 0.69 },
        { {"", "3y", "2w"}, 0.28 },
        { {"", "3y", "1m"}, 0.30 },
        { {"", "3y", "3m"}, 0.44 },
        { {"", "3y", "6m"}, 0.64 },
        { {"", "3y", "1y"}, 0.86 },
        { {"", "3y", "2y"}, 0.96 },
        { {"", "3y", "5y"}, 0.97 },
        { {"", "3y", "10y"}, 0.87 },
        { {"", "3y", "15y"}, 0.81 },
        { {"", "3y", "20y"}, 0.77 },
        { {"", "3y", "30y"}, 0.76 },
        { {"", "5y", "2w"}, 0.24 },
        { {"", "5y", "1m"}, 0.24 },
        { {"", "5y", "3m"}, 0.37 },
        { {"", "5y", "6m"}, 0.56 },
        { {"", "5y", "1y"}, 0.78 },
        { {"", "5y", "2y"}, 0.90 },
        { {"", "5y", "3y"}, 0.97 },
        { {"", "5y", "10y"}, 0.94 },
        { {"", "5y", "15y"}, 0.90 },
        { {"", "5y", "20y"}, 0.86 },
        { {"", "5y", "30y"}, 0.85 },
        { {"", "10y", "2w"}, 0.19 },
        { {"", "10y", "1m"}, 0.19 },
        { {"", "10y", "3m"}, 0.30 },
        { {"", "10y", "6m"}, 0.47 },
        { {"", "10y", "1y"}, 0.67 },
        { {"", "10y", "2y"}, 0.80 },
        { {"", "10y", "3y"}, 0.87 },
        { {"", "10y", "5y"}, 0.94 },
        { {"", "10y", "15y"}, 0.97 },
        { {"", "10y", "20y"}, 0.94 },
        { {"", "10y", "30y"}, 0.94 },
        { {"", "15y", "2w"}, 0.17 },
        { {"", "15y", "1m"}, 0.14 },
        { {"", "15y", "3m"}, 0.23 },
        { {"", "15y", "6m"}, 0.41 },
        { {"", "15y", "1y"}, 0.61 },
        { {"", "15y", "2y"}, 0.75 },
        { {"", "15y", "3y"}, 0.81 },
        { {"", "15y", "5y"}, 0.90 },
        { {"", "15y", "10y"}, 0.97 },
        { {"", "15y", "20y"}, 0.97 },
        { {"", "15y", "30y"}, 0.97 },
        { {"", "20y", "2w"}, 0.16 },
        { {"", "20y", "1m"}, 0.12 },
        { {"", "20y", "3m"}, 0.21 },
        { {"", "20y", "6m"}, 0.38 },
        { {"", "20y", "1y"}, 0.57 },
        { {"", "20y", "2y"}, 0.70 },
        { {"", "20y", "3y"}, 0.77 },
        { {"", "20y", "5y"}, 0.86 },
        { {"", "20y", "10y"}, 0.94 },
        { {"", "20y", "15y"}, 0.97 },
        { {"", "20y", "30y"}, 0.99 },
        { {"", "30y", "2w"}, 0.15 },
        { {"", "30y", "1m"}, 0.12 },
        { {"", "30y", "3m"}, 0.21 },
        { {"", "30y", "6m"}, 0.37 },
        { {"", "30y", "1y"}, 0.56 },
        { {"", "30y", "2y"}, 0.69 },
        { {"", "30y", "3y"}, 0.76 },
        { {"", "30y", "5y"}, 0.85 },
        { {"", "30y", "10y"}, 0.94 },
        { {"", "30y", "15y"}, 0.97 },
        { {"", "30y", "20y"}, 0.99 },
    };

    interBucketCorrelation_[CrifRecord::RiskType::CreditQ] = {
        { {"", "1", "2"}, 0.41 },
        { {"", "1", "3"}, 0.39 },
        { {"", "1", "4"}, 0.35 },
        { {"", "1", "5"}, 0.38 },
        { {"", "1", "6"}, 0.36 },
        { {"", "1", "7"}, 0.43 },
        { {"", "1", "8"}, 0.29 },
        { {"", "1", "9"}, 0.36 },
        { {"", "1", "10"}, 0.36 },
        { {"", "1", "11"}, 0.36 },
        { {"", "1", "12"}, 0.37 },
        { {"", "2", "1"}, 0.41 },
        { {"", "2", "3"}, 0.48 },
        { {"", "2", "4"}, 0.45 },
        { {"", "2", "5"}, 0.48 },
        { {"", "2", "6"}, 0.45 },
        { {"", "2", "7"}, 0.40 },
        { {"", "2", "8"}, 0.35 },
        { {"", "2", "9"}, 0.43 },
        { {"", "2", "10"}, 0.43 },
        { {"", "2", "11"}, 0.42 },
        { {"", "2", "12"}, 0.44 },
        { {"", "3", "1"}, 0.39 },
        { {"", "3", "2"}, 0.48 },
        { {"", "3", "4"}, 0.49 },
        { {"", "3", "5"}, 0.50 },
        { {"", "3", "6"}, 0.50 },
        { {"", "3", "7"}, 0.41 },
        { {"", "3", "8"}, 0.32 },
        { {"", "3", "9"}, 0.46 },
        { {"", "3", "10"}, 0.45 },
        { {"", "3", "11"}, 0.43 },
        { {"", "3", "12"}, 0.48 },
        { {"", "4", "1"}, 0.35 },
        { {"", "4", "2"}, 0.45 },
        { {"", "4", "3"}, 0.49 },
        { {"", "4", "5"}, 0.50 },
        { {"", "4", "6"}, 0.49 },
        { {"", "4", "7"}, 0.38 },
        { {"", "4", "8"}, 0.30 },
        { {"", "4", "9"}, 0.42 },
        { {"", "4", "10"}, 0.44 },
        { {"", "4", "11"}, 0.41 },
        { {"", "4", "12"}, 0.47 },
        { {"", "5", "1"}, 0.38 },
        { {"", "5", "2"}, 0.48 },
        { {"", "5", "3"}, 0.50 },
        { {"", "5", "4"}, 0.50 },
        { {"", "5", "6"}, 0.51 },
        { {"", "5", "7"}, 0.40 },
        { {"", "5", "8"}, 0.31 },
        { {"", "5", "9"}, 0.44 },
        { {"", "5", "10"}, 0.45 },
        { {"", "5", "11"}, 0.43 },
        { {"", "5", "12"}, 0.49 },
        { {"", "6", "1"}, 0.36 },
        { {"", "6", "2"}, 0.45 },
        { {"", "6", "3"}, 0.50 },
        { {"", "6", "4"}, 0.49 },
        { {"", "6", "5"}, 0.51 },
        { {"", "6", "7"}, 0.39 },
        { {"", "6", "8"}, 0.29 },
        { {"", "6", "9"}, 0.42 },
        { {"", "6", "10"}, 0.43 },
        { {"", "6", "11"}, 0.41 },
        { {"", "6", "12"}, 0.49 },
        { {"", "7", "1"}, 0.43 },
        { {"", "7", "2"}, 0.40 },
        { {"", "7", "3"}, 0.41 },
        { {"", "7", "4"}, 0.38 },
        { {"", "7", "5"}, 0.40 },
        { {"", "7", "6"}, 0.39 },
        { {"", "7", "8"}, 0.28 },
        { {"", "7", "9"}, 0.37 },
        { {"", "7", "10"}, 0.38 },
        { {"", "7", "11"}, 0.37 },
        { {"", "7", "12"}, 0.39 },
        { {"", "8", "1"}, 0.29 },
        { {"", "8", "2"}, 0.35 },
        { {"", "8", "3"}, 0.32 },
        { {"", "8", "4"}, 0.30 },
        { {"", "8", "5"}, 0.31 },
        { {"", "8", "6"}, 0.29 },
        { {"", "8", "7"}, 0.28 },
        { {"", "8", "9"}, 0.30 },
        { {"", "8", "10"}, 0.30 },
        { {"", "8", "11"}, 0.29 },
        { {"", "8", "12"}, 0.31 },
        { {"", "9", "1"}, 0.36 },
        { {"", "9", "2"}, 0.43 },
        { {"", "9", "3"}, 0.46 },
        { {"", "9", "4"}, 0.42 },
        { {"", "9", "5"}, 0.44 },
        { {"", "9", "6"}, 0.42 },
        { {"", "9", "7"}, 0.37 },
        { {"", "9", "8"}, 0.30 },
        { {"", "9", "10"}, 0.42 },
        { {"", "9", "11"}, 0.40 },
        { {"", "9", "12"}, 0.44 },
        { {"", "10", "1"}, 0.36 },
        { {"", "10", "2"}, 0.43 },
        { {"", "10", "3"}, 0.45 },
        { {"", "10", "4"}, 0.44 },
        { {"", "10", "5"}, 0.45 },
        { {"", "10", "6"}, 0.43 },
        { {"", "10", "7"}, 0.38 },
        { {"", "10", "8"}, 0.30 },
        { {"", "10", "9"}, 0.42 },
        { {"", "10", "11"}, 0.40 },
        { {"", "10", "12"}, 0.45 },
        { {"", "11", "1"}, 0.36 },
        { {"", "11", "2"}, 0.42 },
        { {"", "11", "3"}, 0.43 },
        { {"", "11", "4"}, 0.41 },
        { {"", "11", "5"}, 0.43 },
        { {"", "11", "6"}, 0.41 },
        { {"", "11", "7"}, 0.37 },
        { {"", "11", "8"}, 0.29 },
        { {"", "11", "9"}, 0.40 },
        { {"", "11", "10"}, 0.40 },
        { {"", "11", "12"}, 0.42 },
        { {"", "12", "1"}, 0.37 },
        { {"", "12", "2"}, 0.44 },
        { {"", "12", "3"}, 0.48 },
        { {"", "12", "4"}, 0.47 },
        { {"", "12", "5"}, 0.49 },
        { {"", "12", "6"}, 0.49 },
        { {"", "12", "7"}, 0.39 },
        { {"", "12", "8"}, 0.31 },
        { {"", "12", "9"}, 0.44 },
        { {"", "12", "10"}, 0.45 },
        { {"", "12", "11"}, 0.42 },
    };

    interBucketCorrelation_[CrifRecord::RiskType::Equity] = {
        { {"", "1", "2"}, 0.14 },
        { {"", "1", "3"}, 0.15 },
        { {"", "1", "4"}, 0.16 },
        { {"", "1", "5"}, 0.13 },
        { {"", "1", "6"}, 0.15 },
        { {"", "1", "7"}, 0.14 },
        { {"", "1", "8"}, 0.15 },
        { {"", "1", "9"}, 0.14 },
        { {"", "1", "10"}, 0.12 },
        { {"", "1", "11"}, 0.17 },
        { {"", "1", "12"}, 0.17 },
        { {"", "2", "1"}, 0.14 },
        { {"", "2", "3"}, 0.18 },
        { {"", "2", "4"}, 0.18 },
        { {"", "2", "5"}, 0.14 },
        { {"", "2", "6"}, 0.17 },
        { {"", "2", "7"}, 0.17 },
        { {"", "2", "8"}, 0.18 },
        { {"", "2", "9"}, 0.16 },
        { {"", "2", "10"}, 0.14 },
        { {"", "2", "11"}, 0.19 },
        { {"", "2", "12"}, 0.19 },
        { {"", "3", "1"}, 0.15 },
        { {"", "3", "2"}, 0.18 },
        { {"", "3", "4"}, 0.19 },
        { {"", "3", "5"}, 0.14 },
        { {"", "3", "6"}, 0.18 },
        { {"", "3", "7"}, 0.21 },
        { {"", "3", "8"}, 0.19 },
        { {"", "3", "9"}, 0.18 },
        { {"", "3", "10"}, 0.14 },
        { {"", "3", "11"}, 0.21 },
        { {"", "3", "12"}, 0.21 },
        { {"", "4", "1"}, 0.16 },
        { {"", "4", "2"}, 0.18 },
        { {"", "4", "3"}, 0.19 },
        { {"", "4", "5"}, 0.17 },
        { {"", "4", "6"}, 0.22 },
        { {"", "4", "7"}, 0.21 },
        { {"", "4", "8"}, 0.23 },
        { {"", "4", "9"}, 0.18 },
        { {"", "4", "10"}, 0.17 },
        { {"", "4", "11"}, 0.24 },
        { {"", "4", "12"}, 0.24 },
        { {"", "5", "1"}, 0.13 },
        { {"", "5", "2"}, 0.14 },
        { {"", "5", "3"}, 0.14 },
        { {"", "5", "4"}, 0.17 },
        { {"", "5", "6"}, 0.25 },
        { {"", "5", "7"}, 0.23 },
        { {"", "5", "8"}, 0.26 },
        { {"", "5", "9"}, 0.13 },
        { {"", "5", "10"}, 0.20 },
        { {"", "5", "11"}, 0.28 },
        { {"", "5", "12"}, 0.28 },
        { {"", "6", "1"}, 0.15 },
        { {"", "6", "2"}, 0.17 },
        { {"", "6", "3"}, 0.18 },
        { {"", "6", "4"}, 0.22 },
        { {"", "6", "5"}, 0.25 },
        { {"", "6", "7"}, 0.29 },
        { {"", "6", "8"}, 0.33 },
        { {"", "6", "9"}, 0.16 },
        { {"", "6", "10"}, 0.26 },
        { {"", "6", "11"}, 0.34 },
        { {"", "6", "12"}, 0.34 },
        { {"", "7", "1"}, 0.14 },
        { {"", "7", "2"}, 0.17 },
        { {"", "7", "3"}, 0.21 },
        { {"", "7", "4"}, 0.21 },
        { {"", "7", "5"}, 0.23 },
        { {"", "7", "6"}, 0.29 },
        { {"", "7", "8"}, 0.30 },
        { {"", "7", "9"}, 0.15 },
        { {"", "7", "10"}, 0.24 },
        { {"", "7", "11"}, 0.33 },
        { {"", "7", "12"}, 0.33 },
        { {"", "8", "1"}, 0.15 },
        { {"", "8", "2"}, 0.18 },
        { {"", "8", "3"}, 0.19 },
        { {"", "8", "4"}, 0.23 },
        { {"", "8", "5"}, 0.26 },
        { {"", "8", "6"}, 0.33 },
        { {"", "8", "7"}, 0.30 },
        { {"", "8", "9"}, 0.16 },
        { {"", "8", "10"}, 0.26 },
        { {"", "8", "11"}, 0.37 },
        { {"", "8", "12"}, 0.37 },
        { {"", "9", "1"}, 0.14 },
        { {"", "9", "2"}, 0.16 },
        { {"", "9", "3"}, 0.18 },
        { {"", "9", "4"}, 0.18 },
        { {"", "9", "5"}, 0.13 },
        { {"", "9", "6"}, 0.16 },
        { {"", "9", "7"}, 0.15 },
        { {"", "9", "8"}, 0.16 },
        { {"", "9", "10"}, 0.12 },
        { {"", "9", "11"}, 0.19 },
        { {"", "9", "12"}, 0.19 },
        { {"", "10", "1"}, 0.12 },
        { {"", "10", "2"}, 0.14 },
        { {"", "10", "3"}, 0.14 },
        { {"", "10", "4"}, 0.17 },
        { {"", "10", "5"}, 0.20 },
        { {"", "10", "6"}, 0.26 },
        { {"", "10", "7"}, 0.24 },
        { {"", "10", "8"}, 0.26 },
        { {"", "10", "9"}, 0.12 },
        { {"", "10", "11"}, 0.26 },
        { {"", "10", "12"}, 0.26 },
        { {"", "11", "1"}, 0.17 },
        { {"", "11", "2"}, 0.19 },
        { {"", "11", "3"}, 0.21 },
        { {"", "11", "4"}, 0.24 },
        { {"", "11", "5"}, 0.28 },
        { {"", "11", "6"}, 0.34 },
        { {"", "11", "7"}, 0.33 },
        { {"", "11", "8"}, 0.37 },
        { {"", "11", "9"}, 0.19 },
        { {"", "11", "10"}, 0.26 },
        { {"", "11", "12"}, 0.40 },
        { {"", "12", "1"}, 0.17 },
        { {"", "12", "2"}, 0.19 },
        { {"", "12", "3"}, 0.21 },
        { {"", "12", "4"}, 0.24 },
        { {"", "12", "5"}, 0.28 },
        { {"", "12", "6"}, 0.34 },
        { {"", "12", "7"}, 0.33 },
        { {"", "12", "8"}, 0.37 },
        { {"", "12", "9"}, 0.19 },
        { {"", "12", "10"}, 0.26 },
        { {"", "12", "11"}, 0.40 },
    };

    interBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        { {"", "1", "2"}, 0.23 },
        { {"", "1", "3"}, 0.19 },
        { {"", "1", "4"}, 0.28 },
        { {"", "1", "5"}, 0.24 },
        { {"", "1", "6"}, 0.32 },
        { {"", "1", "7"}, 0.62 },
        { {"", "1", "8"}, 0.29 },
        { {"", "1", "9"}, 0.50 },
        { {"", "1", "10"}, 0.15 },
        { {"", "1", "11"}, 0.13 },
        { {"", "1", "12"}, 0.08 },
        { {"", "1", "13"}, 0.19 },
        { {"", "1", "14"}, 0.12 },
        { {"", "1", "15"}, 0.04 },
        { {"", "1", "16"}, 0.00 },
        { {"", "1", "17"}, 0.22 },
        { {"", "2", "1"}, 0.23 },
        { {"", "2", "3"}, 0.94 },
        { {"", "2", "4"}, 0.92 },
        { {"", "2", "5"}, 0.89 },
        { {"", "2", "6"}, 0.36 },
        { {"", "2", "7"}, 0.15 },
        { {"", "2", "8"}, 0.23 },
        { {"", "2", "9"}, 0.15 },
        { {"", "2", "10"}, 0.20 },
        { {"", "2", "11"}, 0.42 },
        { {"", "2", "12"}, 0.31 },
        { {"", "2", "13"}, 0.38 },
        { {"", "2", "14"}, 0.28 },
        { {"", "2", "15"}, 0.16 },
        { {"", "2", "16"}, 0.00 },
        { {"", "2", "17"}, 0.67 },
        { {"", "3", "1"}, 0.19 },
        { {"", "3", "2"}, 0.94 },
        { {"", "3", "4"}, 0.91 },
        { {"", "3", "5"}, 0.86 },
        { {"", "3", "6"}, 0.32 },
        { {"", "3", "7"}, 0.11 },
        { {"", "3", "8"}, 0.19 },
        { {"", "3", "9"}, 0.12 },
        { {"", "3", "10"}, 0.22 },
        { {"", "3", "11"}, 0.41 },
        { {"", "3", "12"}, 0.31 },
        { {"", "3", "13"}, 0.37 },
        { {"", "3", "14"}, 0.27 },
        { {"", "3", "15"}, 0.15 },
        { {"", "3", "16"}, 0.00 },
        { {"", "3", "17"}, 0.64 },
        { {"", "4", "1"}, 0.28 },
        { {"", "4", "2"}, 0.92 },
        { {"", "4", "3"}, 0.91 },
        { {"", "4", "5"}, 0.81 },
        { {"", "4", "6"}, 0.40 },
        { {"", "4", "7"}, 0.17 },
        { {"", "4", "8"}, 0.26 },
        { {"", "4", "9"}, 0.18 },
        { {"", "4", "10"}, 0.20 },
        { {"", "4", "11"}, 0.41 },
        { {"", "4", "12"}, 0.26 },
        { {"", "4", "13"}, 0.39 },
        { {"", "4", "14"}, 0.25 },
        { {"", "4", "15"}, 0.14 },
        { {"", "4", "16"}, 0.00 },
        { {"", "4", "17"}, 0.64 },
        { {"", "5", "1"}, 0.24 },
        { {"", "5", "2"}, 0.89 },
        { {"", "5", "3"}, 0.86 },
        { {"", "5", "4"}, 0.81 },
        { {"", "5", "6"}, 0.29 },
        { {"", "5", "7"}, 0.17 },
        { {"", "5", "8"}, 0.21 },
        { {"", "5", "9"}, 0.13 },
        { {"", "5", "10"}, 0.26 },
        { {"", "5", "11"}, 0.42 },
        { {"", "5", "12"}, 0.34 },
        { {"", "5", "13"}, 0.34 },
        { {"", "5", "14"}, 0.32 },
        { {"", "5", "15"}, 0.14 },
        { {"", "5", "16"}, 0.00 },
        { {"", "5", "17"}, 0.62 },
        { {"", "6", "1"}, 0.32 },
        { {"", "6", "2"}, 0.36 },
        { {"", "6", "3"}, 0.32 },
        { {"", "6", "4"}, 0.40 },
        { {"", "6", "5"}, 0.29 },
        { {"", "6", "7"}, 0.30 },
        { {"", "6", "8"}, 0.66 },
        { {"", "6", "9"}, 0.23 },
        { {"", "6", "10"}, 0.07 },
        { {"", "6", "11"}, 0.21 },
        { {"", "6", "12"}, 0.07 },
        { {"", "6", "13"}, 0.23 },
        { {"", "6", "14"}, 0.07 },
        { {"", "6", "15"}, 0.11 },
        { {"", "6", "16"}, 0.00 },
        { {"", "6", "17"}, 0.39 },
        { {"", "7", "1"}, 0.62 },
        { {"", "7", "2"}, 0.15 },
        { {"", "7", "3"}, 0.11 },
        { {"", "7", "4"}, 0.17 },
        { {"", "7", "5"}, 0.17 },
        { {"", "7", "6"}, 0.30 },
        { {"", "7", "8"}, 0.19 },
        { {"", "7", "9"}, 0.78 },
        { {"", "7", "10"}, 0.12 },
        { {"", "7", "11"}, 0.12 },
        { {"", "7", "12"}, 0.02 },
        { {"", "7", "13"}, 0.19 },
        { {"", "7", "14"}, 0.09 },
        { {"", "7", "15"}, 0.00 },
        { {"", "7", "16"}, 0.00 },
        { {"", "7", "17"}, 0.21 },
        { {"", "8", "1"}, 0.29 },
        { {"", "8", "2"}, 0.23 },
        { {"", "8", "3"}, 0.19 },
        { {"", "8", "4"}, 0.26 },
        { {"", "8", "5"}, 0.21 },
        { {"", "8", "6"}, 0.66 },
        { {"", "8", "7"}, 0.19 },
        { {"", "8", "9"}, 0.19 },
        { {"", "8", "10"}, 0.04 },
        { {"", "8", "11"}, 0.10 },
        { {"", "8", "12"}, -0.01 },
        { {"", "8", "13"}, 0.11 },
        { {"", "8", "14"}, 0.04 },
        { {"", "8", "15"}, 0.03 },
        { {"", "8", "16"}, 0.00 },
        { {"", "8", "17"}, 0.21 },
        { {"", "9", "1"}, 0.50 },
        { {"", "9", "2"}, 0.15 },
        { {"", "9", "3"}, 0.12 },
        { {"", "9", "4"}, 0.18 },
        { {"", "9", "5"}, 0.13 },
        { {"", "9", "6"}, 0.23 },
        { {"", "9", "7"}, 0.78 },
        { {"", "9", "8"}, 0.19 },
        { {"", "9", "10"}, 0.07 },
        { {"", "9", "11"}, 0.06 },
        { {"", "9", "12"}, -0.08 },
        { {"", "9", "13"}, 0.13 },
        { {"", "9", "14"}, 0.07 },
        { {"", "9", "15"}, 0.02 },
        { {"", "9", "16"}, 0.00 },
        { {"", "9", "17"}, 0.18 },
        { {"", "10", "1"}, 0.15 },
        { {"", "10", "2"}, 0.20 },
        { {"", "10", "3"}, 0.22 },
        { {"", "10", "4"}, 0.20 },
        { {"", "10", "5"}, 0.26 },
        { {"", "10", "6"}, 0.07 },
        { {"", "10", "7"}, 0.12 },
        { {"", "10", "8"}, 0.04 },
        { {"", "10", "9"}, 0.07 },
        { {"", "10", "11"}, 0.19 },
        { {"", "10", "12"}, 0.10 },
        { {"", "10", "13"}, 0.12 },
        { {"", "10", "14"}, 0.10 },
        { {"", "10", "15"}, 0.01 },
        { {"", "10", "16"}, 0.00 },
        { {"", "10", "17"}, 0.12 },
        { {"", "11", "1"}, 0.13 },
        { {"", "11", "2"}, 0.42 },
        { {"", "11", "3"}, 0.41 },
        { {"", "11", "4"}, 0.41 },
        { {"", "11", "5"}, 0.42 },
        { {"", "11", "6"}, 0.21 },
        { {"", "11", "7"}, 0.12 },
        { {"", "11", "8"}, 0.10 },
        { {"", "11", "9"}, 0.06 },
        { {"", "11", "10"}, 0.19 },
        { {"", "11", "12"}, 0.39 },
        { {"", "11", "13"}, 0.31 },
        { {"", "11", "14"}, 0.24 },
        { {"", "11", "15"}, 0.14 },
        { {"", "11", "16"}, 0.00 },
        { {"", "11", "17"}, 0.39 },
        { {"", "12", "1"}, 0.08 },
        { {"", "12", "2"}, 0.31 },
        { {"", "12", "3"}, 0.31 },
        { {"", "12", "4"}, 0.26 },
        { {"", "12", "5"}, 0.34 },
        { {"", "12", "6"}, 0.07 },
        { {"", "12", "7"}, 0.02 },
        { {"", "12", "8"}, -0.01 },
        { {"", "12", "9"}, -0.08 },
        { {"", "12", "10"}, 0.10 },
        { {"", "12", "11"}, 0.39 },
        { {"", "12", "13"}, 0.22 },
        { {"", "12", "14"}, 0.20 },
        { {"", "12", "15"}, 0.12 },
        { {"", "12", "16"}, 0.00 },
        { {"", "12", "17"}, 0.28 },
        { {"", "13", "1"}, 0.19 },
        { {"", "13", "2"}, 0.38 },
        { {"", "13", "3"}, 0.37 },
        { {"", "13", "4"}, 0.39 },
        { {"", "13", "5"}, 0.34 },
        { {"", "13", "6"}, 0.23 },
        { {"", "13", "7"}, 0.19 },
        { {"", "13", "8"}, 0.11 },
        { {"", "13", "9"}, 0.13 },
        { {"", "13", "10"}, 0.12 },
        { {"", "13", "11"}, 0.31 },
        { {"", "13", "12"}, 0.22 },
        { {"", "13", "14"}, 0.28 },
        { {"", "13", "15"}, 0.19 },
        { {"", "13", "16"}, 0.00 },
        { {"", "13", "17"}, 0.41 },
        { {"", "14", "1"}, 0.12 },
        { {"", "14", "2"}, 0.28 },
        { {"", "14", "3"}, 0.27 },
        { {"", "14", "4"}, 0.25 },
        { {"", "14", "5"}, 0.32 },
        { {"", "14", "6"}, 0.07 },
        { {"", "14", "7"}, 0.09 },
        { {"", "14", "8"}, 0.04 },
        { {"", "14", "9"}, 0.07 },
        { {"", "14", "10"}, 0.10 },
        { {"", "14", "11"}, 0.24 },
        { {"", "14", "12"}, 0.20 },
        { {"", "14", "13"}, 0.28 },
        { {"", "14", "15"}, 0.09 },
        { {"", "14", "16"}, 0.00 },
        { {"", "14", "17"}, 0.22 },
        { {"", "15", "1"}, 0.04 },
        { {"", "15", "2"}, 0.16 },
        { {"", "15", "3"}, 0.15 },
        { {"", "15", "4"}, 0.14 },
        { {"", "15", "5"}, 0.14 },
        { {"", "15", "6"}, 0.11 },
        { {"", "15", "7"}, 0.00 },
        { {"", "15", "8"}, 0.03 },
        { {"", "15", "9"}, 0.02 },
        { {"", "15", "10"}, 0.01 },
        { {"", "15", "11"}, 0.14 },
        { {"", "15", "12"}, 0.12 },
        { {"", "15", "13"}, 0.19 },
        { {"", "15", "14"}, 0.09 },
        { {"", "15", "16"}, 0.00 },
        { {"", "15", "17"}, 0.21 },
        { {"", "16", "1"}, 0.00 },
        { {"", "16", "2"}, 0.00 },
        { {"", "16", "3"}, 0.00 },
        { {"", "16", "4"}, 0.00 },
        { {"", "16", "5"}, 0.00 },
        { {"", "16", "6"}, 0.00 },
        { {"", "16", "7"}, 0.00 },
        { {"", "16", "8"}, 0.00 },
        { {"", "16", "9"}, 0.00 },
        { {"", "16", "10"}, 0.00 },
        { {"", "16", "11"}, 0.00 },
        { {"", "16", "12"}, 0.00 },
        { {"", "16", "13"}, 0.00 },
        { {"", "16", "14"}, 0.00 },
        { {"", "16", "15"}, 0.00 },
        { {"", "16", "17"}, 0.00 },
        { {"", "17", "1"}, 0.22 },
        { {"", "17", "2"}, 0.67 },
        { {"", "17", "3"}, 0.64 },
        { {"", "17", "4"}, 0.64 },
        { {"", "17", "5"}, 0.62 },
        { {"", "17", "6"}, 0.39 },
        { {"", "17", "7"}, 0.21 },
        { {"", "17", "8"}, 0.21 },
        { {"", "17", "9"}, 0.18 },
        { {"", "17", "10"}, 0.12 },
        { {"", "17", "11"}, 0.39 },
        { {"", "17", "12"}, 0.28 },
        { {"", "17", "13"}, 0.41 },
        { {"", "17", "14"}, 0.22 },
        { {"", "17", "15"}, 0.21 },
        { {"", "17", "16"}, 0.00 },
    };

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[CrifRecord::RiskType::Equity] = {
        { {"1", "", ""}, 0.14 },
        { {"2", "", ""}, 0.16 },
        { {"3", "", ""}, 0.23 },
        { {"4", "", ""}, 0.21 },
        { {"5", "", ""}, 0.23 },
        { {"6", "", ""}, 0.32 },
        { {"7", "", ""}, 0.32 },
        { {"8", "", ""}, 0.35 },
        { {"9", "", ""}, 0.21 },
        { {"10", "", ""}, 0.22 },
        { {"11", "", ""}, 0.40 },
        { {"12", "", ""}, 0.40 },
        { {"Residual", "", ""}, 0.00 },
    };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        { {"1", "", ""}, 0.84 },
        { {"2", "", ""}, 0.98 },
        { {"3", "", ""}, 0.98 },
        { {"4", "", ""}, 0.98 },
        { {"5", "", ""}, 0.98 },
        { {"6", "", ""}, 0.93 },
        { {"7", "", ""}, 0.93 },
        { {"8", "", ""}, 0.51 },
        { {"9", "", ""}, 0.59 },
        { {"10", "", ""}, 0.44 },
        { {"11", "", ""}, 0.58 },
        { {"12", "", ""}, 0.60 },
        { {"13", "", ""}, 0.60 },
        { {"14", "", ""}, 0.21 },
        { {"15", "", ""}, 0.17 },
        { {"16", "", ""}, 0.00 },
        { {"17", "", ""}, 0.43 },
    };

    // Initialise the single, ad-hoc type, correlations
    xccyCorr_ = -0.05;
    infCorr_ = 0.26;
    infVolCorr_ = 0.26;
    irSubCurveCorr_ = 0.990;
    irInterCurrencyCorr_ = 0.30;
    crqResidualIntraCorr_ = 0.50;
    crqSameIntraCorr_ = 0.94;
    crqDiffIntraCorr_ = 0.47;
    crnqResidualIntraCorr_ = 0.50;
    crnqSameIntraCorr_ = 0.85;
    crnqDiffIntraCorr_ = 0.29;
    crnqInterCorr_ = 0.51;
    fxCorr_ = 0.50;
    basecorrCorr_ = 0.31;

    // clang-format on
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 8 section 11(d)
of the ISDA-SIMM-v2.6.5 documentation).
*/
QuantLib::Real SimmConfiguration_ISDA_V2_6_5::curvatureMarginScaling() const { return pow(hvr_ir_, -2.0); }

void SimmConfiguration_ISDA_V2_6_5::addLabels2(const CrifRecord::RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V2_6_5::label2(const QuantLib::ext::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::label2(irIndex);
}

} // namespace analytics
} // namespace ore
