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

#include <orea/simm/simmconcentrationisdav2_7_2412_1.hpp>
#include <orea/simm/simmconfigurationisdav2_7_2412_1.hpp>
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
QuantLib::Size SimmConfiguration_ISDA_V2_7_2412_1::group(const string& qualifier, const std::map<QuantLib::Size,
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

QuantLib::Real SimmConfiguration_ISDA_V2_7_2412_1::weight(const CrifRecord::RiskType& rt, boost::optional<string> qualifier,
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

QuantLib::Real SimmConfiguration_ISDA_V2_7_2412_1::correlation(const CrifRecord::RiskType& firstRt, const string& firstQualifier,
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

SimmConfiguration_ISDA_V2_7_2412_1::SimmConfiguration_ISDA_V2_7_2412_1(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const QuantLib::Size& mporDays, const std::string& name,
                                                         const std::string version)
     : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentration_ISDA_V2_7_2412_1>(simmBucketMapper_);
    } else {
        // SIMM:Technical Paper, Section I.4: "The Concentration Risk feature is disabled"
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentrationBase>();
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
           7,  21.4,
           21.4, 35.9
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { CrifRecord::RiskType::Inflation, 52 },
            { CrifRecord::RiskType::XCcyBasis, 22 },
            { CrifRecord::RiskType::IRVol, 0.21 },
            { CrifRecord::RiskType::InflationVol, 0.20 },
            { CrifRecord::RiskType::CreditVol, 0.34 },
            { CrifRecord::RiskType::CreditVolNonQ, 0.34 },
            { CrifRecord::RiskType::CommodityVol, 0.37 },
            { CrifRecord::RiskType::FXVol, 0.35 },
            { CrifRecord::RiskType::BaseCorr, 9.6 }
        };

        rwBucket_ = {
            { CrifRecord::RiskType::CreditQ, {
                { {"1", "", ""}, 63 },
                { {"2", "", ""}, 84 },
                { {"3", "", ""}, 74 },
                { {"4", "", ""}, 53 },
                { {"5", "", ""}, 55 },
                { {"6", "", ""}, 40 },
                { {"7", "", ""}, 165 },
                { {"8", "", ""}, 235 },
                { {"9", "", ""}, 187 },
                { {"10", "", ""}, 202 },
                { {"11", "", ""}, 148 },
                { {"12", "", ""}, 152 },
                { {"Residual", "", ""}, 235 },
            }},
            { CrifRecord::RiskType::CreditNonQ, {
                { {"1", "", ""}, 210 },
                { {"2", "", ""}, 2700 },
                { {"Residual", "", ""}, 2700 },
            }},
            { CrifRecord::RiskType::Equity, {
                { {"1", "", ""}, 28 },
                { {"2", "", ""}, 29 },
                { {"3", "", ""}, 29 },
                { {"4", "", ""}, 28 },
                { {"5", "", ""}, 23 },
                { {"6", "", ""}, 24 },
                { {"7", "", ""}, 25 },
                { {"8", "", ""}, 29 },
                { {"9", "", ""}, 33 },
                { {"10", "", ""}, 41 },
                { {"11", "", ""}, 16 },
                { {"12", "", ""}, 16 },
                { {"Residual", "", ""}, 41 },
            }},
            { CrifRecord::RiskType::Commodity, {
                { {"1", "", ""}, 48 },
                { {"2", "", ""}, 21 },
                { {"3", "", ""}, 23 },
                { {"4", "", ""}, 20 },
                { {"5", "", ""}, 24 },
                { {"6", "", ""}, 33 },
                { {"7", "", ""}, 61 },
                { {"8", "", ""}, 37 },
                { {"9", "", ""}, 64 },
                { {"10", "", ""}, 45 },
                { {"11", "", ""}, 21 },
                { {"12", "", ""}, 17 },
                { {"13", "", ""}, 16 },
                { {"14", "", ""}, 17 },
                { {"15", "", ""}, 10 },
                { {"16", "", ""}, 64 },
                { {"17", "", ""}, 16 },
            }},
            { CrifRecord::RiskType::EquityVol, {
                { {"1", "", ""}, 0.26 },
                { {"2", "", ""}, 0.26 },
                { {"3", "", ""}, 0.26 },
                { {"4", "", ""}, 0.26 },
                { {"5", "", ""}, 0.26 },
                { {"6", "", ""}, 0.26 },
                { {"7", "", ""}, 0.26 },
                { {"8", "", ""}, 0.26 },
                { {"9", "", ""}, 0.26 },
                { {"10", "", ""}, 0.26 },
                { {"11", "", ""}, 0.26 },
                { {"12", "", ""}, 0.6 },
                { {"Residual", "", ""}, 0.26 },
            }},
        };

        rwLabel_1_ = {
            { CrifRecord::RiskType::IRCurve, {
                { {"1", "2w", ""}, 108 },
                { {"1", "1m", ""}, 106 },
                { {"1", "3m", ""}, 91 },
                { {"1", "6m", ""}, 68 },
                { {"1", "1y", ""}, 68 },
                { {"1", "2y", ""}, 69 },
                { {"1", "3y", ""}, 66 },
                { {"1", "5y", ""}, 61 },
                { {"1", "10y", ""}, 60 },
                { {"1", "15y", ""}, 57 },
                { {"1", "20y", ""}, 57 },
                { {"1", "30y", ""}, 65 },
                { {"2", "2w", ""}, 15 },
                { {"2", "1m", ""}, 18 },
                { {"2", "3m", ""}, 11 },
                { {"2", "6m", ""}, 10 },
                { {"2", "1y", ""}, 11 },
                { {"2", "2y", ""}, 15 },
                { {"2", "3y", ""}, 18 },
                { {"2", "5y", ""}, 23 },
                { {"2", "10y", ""}, 25 },
                { {"2", "15y", ""}, 23 },
                { {"2", "20y", ""}, 23 },
                { {"2", "30y", ""}, 25 },
                { {"3", "2w", ""}, 172 },
                { {"3", "1m", ""}, 105 },
                { {"3", "3m", ""}, 84 },
                { {"3", "6m", ""}, 89 },
                { {"3", "1y", ""}, 105 },
                { {"3", "2y", ""}, 97 },
                { {"3", "3y", ""}, 103 },
                { {"3", "5y", ""}, 93 },
                { {"3", "10y", ""}, 88 },
                { {"3", "15y", ""}, 98 },
                { {"3", "20y", ""}, 101 },
                { {"3", "30y", ""}, 96 },
            }}
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[CrifRecord::RiskType::EquityVol] = 0.58;
        historicalVolatilityRatios_[CrifRecord::RiskType::CommodityVol] = 0.9;
        historicalVolatilityRatios_[CrifRecord::RiskType::FXVol] = 0.66;
        hvr_ir_ = 0.75;

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
           1.8,  3.9,
           3.9, 5
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { CrifRecord::RiskType::Inflation, 14 },
            { CrifRecord::RiskType::XCcyBasis, 5.8 },
            { CrifRecord::RiskType::IRVol, 0.055 },
            { CrifRecord::RiskType::InflationVol, 0.055 },
            { CrifRecord::RiskType::CreditVol, 0.094 },
            { CrifRecord::RiskType::CreditVolNonQ, 0.094 },
            { CrifRecord::RiskType::CommodityVol, 0.11 },
            { CrifRecord::RiskType::FXVol, 0.087 },
            { CrifRecord::RiskType::BaseCorr, 2 }
        };

        rwBucket_ = {
            { CrifRecord::RiskType::CreditQ, {
                { {"1", "", ""}, 17 },
                { {"2", "", ""}, 23 },
                { {"3", "", ""}, 16 },
                { {"4", "", ""}, 13 },
                { {"5", "", ""}, 13 },
                { {"6", "", ""}, 9.2 },
                { {"7", "", ""}, 43 },
                { {"8", "", ""}, 73 },
                { {"9", "", ""}, 43 },
                { {"10", "", ""}, 47 },
                { {"11", "", ""}, 32 },
                { {"12", "", ""}, 37 },
                { {"Residual", "", ""}, 73 },
            }},
            { CrifRecord::RiskType::CreditNonQ, {
                { {"1", "", ""}, 63 },
                { {"2", "", ""}, 570 },
                { {"Residual", "", ""}, 570 },
            }},
            { CrifRecord::RiskType::Equity, {
                { {"1", "", ""}, 8.7 },
                { {"2", "", ""}, 8.9 },
                { {"3", "", ""}, 9 },
                { {"4", "", ""}, 8.7 },
                { {"5", "", ""}, 7.7 },
                { {"6", "", ""}, 8.2 },
                { {"7", "", ""}, 8.9 },
                { {"8", "", ""}, 10 },
                { {"9", "", ""}, 9.3 },
                { {"10", "", ""}, 12 },
                { {"11", "", ""}, 5.4 },
                { {"12", "", ""}, 5.4 },
                { {"Residual", "", ""}, 12 },
            }},
            { CrifRecord::RiskType::Commodity, {
                { {"1", "", ""}, 11 },
                { {"2", "", ""}, 7.2 },
                { {"3", "", ""}, 7.1 },
                { {"4", "", ""}, 7.0 },
                { {"5", "", ""}, 8.1 },
                { {"6", "", ""}, 9.7 },
                { {"7", "", ""}, 17 },
                { {"8", "", ""}, 11 },
                { {"9", "", ""}, 15 },
                { {"10", "", ""}, 12 },
                { {"11", "", ""}, 6.5 },
                { {"12", "", ""}, 5.9 },
                { {"13", "", ""}, 4.8 },
                { {"14", "", ""}, 5.1 },
                { {"15", "", ""}, 3.2 },
                { {"16", "", ""}, 17 },
                { {"17", "", ""}, 4.9 },
            }},
            { CrifRecord::RiskType::EquityVol, {
                { {"1", "", ""}, 0.074 },
                { {"2", "", ""}, 0.074 },
                { {"3", "", ""}, 0.074 },
                { {"4", "", ""}, 0.074 },
                { {"5", "", ""}, 0.074 },
                { {"6", "", ""}, 0.074 },
                { {"7", "", ""}, 0.074 },
                { {"8", "", ""}, 0.074 },
                { {"9", "", ""}, 0.074 },
                { {"10", "", ""}, 0.074 },
                { {"11", "", ""}, 0.074 },
                { {"12", "", ""}, 0.20 },
                { {"Residual", "", ""}, 0.074 },
            }},
        };

        rwLabel_1_ = {
            { CrifRecord::RiskType::IRCurve, {
                { {"1", "2w", ""}, 28 },
                { {"1", "1m", ""}, 19 },
                { {"1", "3m", ""}, 20 },
                { {"1", "6m", ""}, 24 },
                { {"1", "1y", ""}, 29 },
                { {"1", "2y", ""}, 30 },
                { {"1", "3y", ""}, 30 },
                { {"1", "5y", ""}, 28 },
                { {"1", "10y", ""}, 26 },
                { {"1", "15y", ""}, 25 },
                { {"1", "20y", ""}, 24 },
                { {"1", "30y", ""}, 24 },
                { {"2", "2w", ""}, 7.6 },
                { {"2", "1m", ""}, 8.3 },
                { {"2", "3m", ""}, 5.6 },
                { {"2", "6m", ""}, 5.1 },
                { {"2", "1y", ""}, 6.9 },
                { {"2", "2y", ""}, 8.2 },
                { {"2", "3y", ""}, 9.8 },
                { {"2", "5y", ""}, 12 },
                { {"2", "10y", ""}, 13 },
                { {"2", "15y", ""}, 13 },
                { {"2", "20y", ""}, 13 },
                { {"2", "30y", ""}, 14 },
                { {"3", "2w", ""}, 66 },
                { {"3", "1m", ""}, 39 },
                { {"3", "3m", ""}, 26 },
                { {"3", "6m", ""}, 26 },
                { {"3", "1y", ""}, 29 },
                { {"3", "2y", ""}, 27 },
                { {"3", "3y", ""}, 36 },
                { {"3", "5y", ""}, 34 },
                { {"3", "10y", ""}, 30 },
                { {"3", "15y", ""}, 25 },
                { {"3", "20y", ""}, 30 },
                { {"3", "30y", ""}, 24 },
            }}
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[CrifRecord::RiskType::EquityVol] = 0.54;
        historicalVolatilityRatios_[CrifRecord::RiskType::CommodityVol] = 0.84;
        historicalVolatilityRatios_[CrifRecord::RiskType::FXVol] = 0.83;
        hvr_ir_ = 0.54;

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
        { {"", "InterestRate", "CreditQualifying"}, 0.11 },
        { {"", "InterestRate", "CreditNonQualifying"}, 0.14 },
        { {"", "InterestRate", "Equity"}, 0.1 },
        { {"", "InterestRate", "Commodity"}, 0.3 },
        { {"", "InterestRate", "FX"}, 0.1 },
        { {"", "CreditQualifying", "InterestRate"}, 0.11 },
        { {"", "CreditQualifying", "CreditNonQualifying"}, 0.63 },
        { {"", "CreditQualifying", "Equity"}, 0.66 },
        { {"", "CreditQualifying", "Commodity"}, 0.18 },
        { {"", "CreditQualifying", "FX"}, 0.25 },
        { {"", "CreditNonQualifying", "InterestRate"}, 0.14 },
        { {"", "CreditNonQualifying", "CreditQualifying"}, 0.63 },
        { {"", "CreditNonQualifying", "Equity"}, 0.52 },
        { {"", "CreditNonQualifying", "Commodity"}, 0.21 },
        { {"", "CreditNonQualifying", "FX"}, 0.17 },
        { {"", "Equity", "InterestRate"}, 0.1 },
        { {"", "Equity", "CreditQualifying"}, 0.66 },
        { {"", "Equity", "CreditNonQualifying"}, 0.52 },
        { {"", "Equity", "Commodity"}, 0.28 },
        { {"", "Equity", "FX"}, 0.32 },
        { {"", "Commodity", "InterestRate"}, 0.3 },
        { {"", "Commodity", "CreditQualifying"}, 0.18 },
        { {"", "Commodity", "CreditNonQualifying"}, 0.21 },
        { {"", "Commodity", "Equity"}, 0.28 },
        { {"", "Commodity", "FX"}, 0.25 },
        { {"", "FX", "InterestRate"}, 0.1 },
        { {"", "FX", "CreditQualifying"}, 0.25 },
        { {"", "FX", "CreditNonQualifying"}, 0.17 },
        { {"", "FX", "Equity"}, 0.32 },
        { {"", "FX", "Commodity"}, 0.25 },
    };

    // FX correlations
    temp = {
        0.50, 0.16,
        0.16, -0.41
    };
    fxRegVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    temp = {
        0.94, 0.84,
        0.84, 0.50
    };
    fxHighVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations)
    intraBucketCorrelation_[CrifRecord::RiskType::IRCurve] = {
        { {"", "2w", "1m"}, 0.74 },
        { {"", "2w", "3m"}, 0.64 },
        { {"", "2w", "6m"}, 0.52 },
        { {"", "2w", "1y"}, 0.39 },
        { {"", "2w", "2y"}, 0.28 },
        { {"", "2w", "3y"}, 0.24 },
        { {"", "2w", "5y"}, 0.21 },
        { {"", "2w", "10y"}, 0.17 },
        { {"", "2w", "15y"}, 0.17 },
        { {"", "2w", "20y"}, 0.16 },
        { {"", "2w", "30y"}, 0.16 },
        { {"", "1m", "2w"}, 0.74 },
        { {"", "1m", "3m"}, 0.84 },
        { {"", "1m", "6m"}, 0.7 },
        { {"", "1m", "1y"}, 0.49 },
        { {"", "1m", "2y"}, 0.35 },
        { {"", "1m", "3y"}, 0.29 },
        { {"", "1m", "5y"}, 0.24 },
        { {"", "1m", "10y"}, 0.2 },
        { {"", "1m", "15y"}, 0.15 },
        { {"", "1m", "20y"}, 0.14 },
        { {"", "1m", "30y"}, 0.14 },
        { {"", "3m", "2w"}, 0.64 },
        { {"", "3m", "1m"}, 0.84 },
        { {"", "3m", "6m"}, 0.89 },
        { {"", "3m", "1y"}, 0.68 },
        { {"", "3m", "2y"}, 0.52 },
        { {"", "3m", "3y"}, 0.45 },
        { {"", "3m", "5y"}, 0.38 },
        { {"", "3m", "10y"}, 0.33 },
        { {"", "3m", "15y"}, 0.27 },
        { {"", "3m", "20y"}, 0.24 },
        { {"", "3m", "30y"}, 0.25 },
        { {"", "6m", "2w"}, 0.52 },
        { {"", "6m", "1m"}, 0.7 },
        { {"", "6m", "3m"}, 0.89 },
        { {"", "6m", "1y"}, 0.86 },
        { {"", "6m", "2y"}, 0.73 },
        { {"", "6m", "3y"}, 0.65 },
        { {"", "6m", "5y"}, 0.58 },
        { {"", "6m", "10y"}, 0.51 },
        { {"", "6m", "15y"}, 0.46 },
        { {"", "6m", "20y"}, 0.42 },
        { {"", "6m", "30y"}, 0.42 },
        { {"", "1y", "2w"}, 0.39 },
        { {"", "1y", "1m"}, 0.49 },
        { {"", "1y", "3m"}, 0.68 },
        { {"", "1y", "6m"}, 0.86 },
        { {"", "1y", "2y"}, 0.94 },
        { {"", "1y", "3y"}, 0.88 },
        { {"", "1y", "5y"}, 0.81 },
        { {"", "1y", "10y"}, 0.73 },
        { {"", "1y", "15y"}, 0.68 },
        { {"", "1y", "20y"}, 0.63 },
        { {"", "1y", "30y"}, 0.62 },
        { {"", "2y", "2w"}, 0.28 },
        { {"", "2y", "1m"}, 0.35 },
        { {"", "2y", "3m"}, 0.52 },
        { {"", "2y", "6m"}, 0.73 },
        { {"", "2y", "1y"}, 0.94 },
        { {"", "2y", "3y"}, 0.97 },
        { {"", "2y", "5y"}, 0.92 },
        { {"", "2y", "10y"}, 0.85 },
        { {"", "2y", "15y"}, 0.81 },
        { {"", "2y", "20y"}, 0.76 },
        { {"", "2y", "30y"}, 0.75 },
        { {"", "3y", "2w"}, 0.24 },
        { {"", "3y", "1m"}, 0.29 },
        { {"", "3y", "3m"}, 0.45 },
        { {"", "3y", "6m"}, 0.65 },
        { {"", "3y", "1y"}, 0.88 },
        { {"", "3y", "2y"}, 0.97 },
        { {"", "3y", "5y"}, 0.97 },
        { {"", "3y", "10y"}, 0.91 },
        { {"", "3y", "15y"}, 0.87 },
        { {"", "3y", "20y"}, 0.81 },
        { {"", "3y", "30y"}, 0.81 },
        { {"", "5y", "2w"}, 0.21 },
        { {"", "5y", "1m"}, 0.24 },
        { {"", "5y", "3m"}, 0.38 },
        { {"", "5y", "6m"}, 0.58 },
        { {"", "5y", "1y"}, 0.81 },
        { {"", "5y", "2y"}, 0.92 },
        { {"", "5y", "3y"}, 0.97 },
        { {"", "5y", "10y"}, 0.96 },
        { {"", "5y", "15y"}, 0.93 },
        { {"", "5y", "20y"}, 0.88 },
        { {"", "5y", "30y"}, 0.88 },
        { {"", "10y", "2w"}, 0.17 },
        { {"", "10y", "1m"}, 0.2 },
        { {"", "10y", "3m"}, 0.33 },
        { {"", "10y", "6m"}, 0.51 },
        { {"", "10y", "1y"}, 0.73 },
        { {"", "10y", "2y"}, 0.85 },
        { {"", "10y", "3y"}, 0.91 },
        { {"", "10y", "5y"}, 0.96 },
        { {"", "10y", "15y"}, 0.98 },
        { {"", "10y", "20y"}, 0.95 },
        { {"", "10y", "30y"}, 0.95 },
        { {"", "15y", "2w"}, 0.17 },
        { {"", "15y", "1m"}, 0.15 },
        { {"", "15y", "3m"}, 0.27 },
        { {"", "15y", "6m"}, 0.46 },
        { {"", "15y", "1y"}, 0.68 },
        { {"", "15y", "2y"}, 0.81 },
        { {"", "15y", "3y"}, 0.87 },
        { {"", "15y", "5y"}, 0.93 },
        { {"", "15y", "10y"}, 0.98 },
        { {"", "15y", "20y"}, 0.98 },
        { {"", "15y", "30y"}, 0.97 },
        { {"", "20y", "2w"}, 0.16 },
        { {"", "20y", "1m"}, 0.14 },
        { {"", "20y", "3m"}, 0.24 },
        { {"", "20y", "6m"}, 0.42 },
        { {"", "20y", "1y"}, 0.63 },
        { {"", "20y", "2y"}, 0.76 },
        { {"", "20y", "3y"}, 0.81 },
        { {"", "20y", "5y"}, 0.88 },
        { {"", "20y", "10y"}, 0.95 },
        { {"", "20y", "15y"}, 0.98 },
        { {"", "20y", "30y"}, 0.98 },
        { {"", "30y", "2w"}, 0.16 },
        { {"", "30y", "1m"}, 0.14 },
        { {"", "30y", "3m"}, 0.25 },
        { {"", "30y", "6m"}, 0.42 },
        { {"", "30y", "1y"}, 0.62 },
        { {"", "30y", "2y"}, 0.75 },
        { {"", "30y", "3y"}, 0.81 },
        { {"", "30y", "5y"}, 0.88 },
        { {"", "30y", "10y"}, 0.95 },
        { {"", "30y", "15y"}, 0.97 },
        { {"", "30y", "20y"}, 0.98 }
    };

    interBucketCorrelation_[CrifRecord::RiskType::CreditQ] = {
        { {"", "1", "2"}, 0.41 },
        { {"", "1", "3"}, 0.39 },
        { {"", "1", "4"}, 0.36 },
        { {"", "1", "5"}, 0.38 },
        { {"", "1", "6"}, 0.36 },
        { {"", "1", "7"}, 0.43 },
        { {"", "1", "8"}, 0.31 },
        { {"", "1", "9"}, 0.36 },
        { {"", "1", "10"}, 0.36 },
        { {"", "1", "11"}, 0.35 },
        { {"", "1", "12"}, 0.37 },
        { {"", "2", "1"}, 0.41 },
        { {"", "2", "3"}, 0.48 },
        { {"", "2", "4"}, 0.46 },
        { {"", "2", "5"}, 0.47 },
        { {"", "2", "6"}, 0.46 },
        { {"", "2", "7"}, 0.41 },
        { {"", "2", "8"}, 0.37 },
        { {"", "2", "9"}, 0.44 },
        { {"", "2", "10"}, 0.43 },
        { {"", "2", "11"}, 0.43 },
        { {"", "2", "12"}, 0.41 },
        { {"", "3", "1"}, 0.39 },
        { {"", "3", "2"}, 0.48 },
        { {"", "3", "4"}, 0.51 },
        { {"", "3", "5"}, 0.51 },
        { {"", "3", "6"}, 0.52 },
        { {"", "3", "7"}, 0.42 },
        { {"", "3", "8"}, 0.35 },
        { {"", "3", "9"}, 0.47 },
        { {"", "3", "10"}, 0.46 },
        { {"", "3", "11"}, 0.46 },
        { {"", "3", "12"}, 0.44 },
        { {"", "4", "1"}, 0.36 },
        { {"", "4", "2"}, 0.46 },
        { {"", "4", "3"}, 0.51 },
        { {"", "4", "5"}, 0.5 },
        { {"", "4", "6"}, 0.51 },
        { {"", "4", "7"}, 0.4 },
        { {"", "4", "8"}, 0.33 },
        { {"", "4", "9"}, 0.44 },
        { {"", "4", "10"}, 0.45 },
        { {"", "4", "11"}, 0.44 },
        { {"", "4", "12"}, 0.44 },
        { {"", "5", "1"}, 0.38 },
        { {"", "5", "2"}, 0.47 },
        { {"", "5", "3"}, 0.51 },
        { {"", "5", "4"}, 0.5 },
        { {"", "5", "6"}, 0.52 },
        { {"", "5", "7"}, 0.4 },
        { {"", "5", "8"}, 0.33 },
        { {"", "5", "9"}, 0.44 },
        { {"", "5", "10"}, 0.44 },
        { {"", "5", "11"}, 0.45 },
        { {"", "5", "12"}, 0.45 },
        { {"", "6", "1"}, 0.36 },
        { {"", "6", "2"}, 0.46 },
        { {"", "6", "3"}, 0.52 },
        { {"", "6", "4"}, 0.51 },
        { {"", "6", "5"}, 0.52 },
        { {"", "6", "7"}, 0.41 },
        { {"", "6", "8"}, 0.33 },
        { {"", "6", "9"}, 0.44 },
        { {"", "6", "10"}, 0.44 },
        { {"", "6", "11"}, 0.45 },
        { {"", "6", "12"}, 0.47 },
        { {"", "7", "1"}, 0.43 },
        { {"", "7", "2"}, 0.41 },
        { {"", "7", "3"}, 0.42 },
        { {"", "7", "4"}, 0.4 },
        { {"", "7", "5"}, 0.4 },
        { {"", "7", "6"}, 0.41 },
        { {"", "7", "8"}, 0.31 },
        { {"", "7", "9"}, 0.4 },
        { {"", "7", "10"}, 0.39 },
        { {"", "7", "11"}, 0.38 },
        { {"", "7", "12"}, 0.41 },
        { {"", "8", "1"}, 0.31 },
        { {"", "8", "2"}, 0.37 },
        { {"", "8", "3"}, 0.35 },
        { {"", "8", "4"}, 0.33 },
        { {"", "8", "5"}, 0.33 },
        { {"", "8", "6"}, 0.33 },
        { {"", "8", "7"}, 0.31 },
        { {"", "8", "9"}, 0.33 },
        { {"", "8", "10"}, 0.33 },
        { {"", "8", "11"}, 0.31 },
        { {"", "8", "12"}, 0.33 },
        { {"", "9", "1"}, 0.36 },
        { {"", "9", "2"}, 0.44 },
        { {"", "9", "3"}, 0.47 },
        { {"", "9", "4"}, 0.44 },
        { {"", "9", "5"}, 0.44 },
        { {"", "9", "6"}, 0.44 },
        { {"", "9", "7"}, 0.4 },
        { {"", "9", "8"}, 0.33 },
        { {"", "9", "10"}, 0.42 },
        { {"", "9", "11"}, 0.41 },
        { {"", "9", "12"}, 0.41 },
        { {"", "10", "1"}, 0.36 },
        { {"", "10", "2"}, 0.43 },
        { {"", "10", "3"}, 0.46 },
        { {"", "10", "4"}, 0.45 },
        { {"", "10", "5"}, 0.44 },
        { {"", "10", "6"}, 0.44 },
        { {"", "10", "7"}, 0.39 },
        { {"", "10", "8"}, 0.33 },
        { {"", "10", "9"}, 0.42 },
        { {"", "10", "11"}, 0.41 },
        { {"", "10", "12"}, 0.42 },
        { {"", "11", "1"}, 0.35 },
        { {"", "11", "2"}, 0.43 },
        { {"", "11", "3"}, 0.46 },
        { {"", "11", "4"}, 0.44 },
        { {"", "11", "5"}, 0.45 },
        { {"", "11", "6"}, 0.45 },
        { {"", "11", "7"}, 0.38 },
        { {"", "11", "8"}, 0.31 },
        { {"", "11", "9"}, 0.41 },
        { {"", "11", "10"}, 0.41 },
        { {"", "11", "12"}, 0.4 },
        { {"", "12", "1"}, 0.37 },
        { {"", "12", "2"}, 0.41 },
        { {"", "12", "3"}, 0.44 },
        { {"", "12", "4"}, 0.44 },
        { {"", "12", "5"}, 0.45 },
        { {"", "12", "6"}, 0.47 },
        { {"", "12", "7"}, 0.41 },
        { {"", "12", "8"}, 0.33 },
        { {"", "12", "9"}, 0.41 },
        { {"", "12", "10"}, 0.42 },
        { {"", "12", "11"}, 0.4 }
    };

    interBucketCorrelation_[CrifRecord::RiskType::Equity] = {
         { {"", "1", "2"}, 0.15 },
        { {"", "1", "3"}, 0.17 },
        { {"", "1", "4"}, 0.16 },
        { {"", "1", "5"}, 0.13 },
        { {"", "1", "6"}, 0.14 },
        { {"", "1", "7"}, 0.14 },
        { {"", "1", "8"}, 0.14 },
        { {"", "1", "9"}, 0.16 },
        { {"", "1", "10"}, 0.11 },
        { {"", "1", "11"}, 0.17 },
        { {"", "1", "12"}, 0.17 },
        { {"", "2", "1"}, 0.15 },
        { {"", "2", "3"}, 0.19 },
        { {"", "2", "4"}, 0.18 },
        { {"", "2", "5"}, 0.14 },
        { {"", "2", "6"}, 0.17 },
        { {"", "2", "7"}, 0.17 },
        { {"", "2", "8"}, 0.17 },
        { {"", "2", "9"}, 0.19 },
        { {"", "2", "10"}, 0.13 },
        { {"", "2", "11"}, 0.19 },
        { {"", "2", "12"}, 0.19 },
        { {"", "3", "1"}, 0.17 },
        { {"", "3", "2"}, 0.19 },
        { {"", "3", "4"}, 0.2 },
        { {"", "3", "5"}, 0.14 },
        { {"", "3", "6"}, 0.17 },
        { {"", "3", "7"}, 0.21 },
        { {"", "3", "8"}, 0.17 },
        { {"", "3", "9"}, 0.21 },
        { {"", "3", "10"}, 0.13 },
        { {"", "3", "11"}, 0.2 },
        { {"", "3", "12"}, 0.2 },
        { {"", "4", "1"}, 0.16 },
        { {"", "4", "2"}, 0.18 },
        { {"", "4", "3"}, 0.2 },
        { {"", "4", "5"}, 0.17 },
        { {"", "4", "6"}, 0.21 },
        { {"", "4", "7"}, 0.21 },
        { {"", "4", "8"}, 0.22 },
        { {"", "4", "9"}, 0.2 },
        { {"", "4", "10"}, 0.16 },
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
        { {"", "5", "10"}, 0.19 },
        { {"", "5", "11"}, 0.29 },
        { {"", "5", "12"}, 0.29 },
        { {"", "6", "1"}, 0.14 },
        { {"", "6", "2"}, 0.17 },
        { {"", "6", "3"}, 0.17 },
        { {"", "6", "4"}, 0.21 },
        { {"", "6", "5"}, 0.25 },
        { {"", "6", "7"}, 0.3 },
        { {"", "6", "8"}, 0.33 },
        { {"", "6", "9"}, 0.17 },
        { {"", "6", "10"}, 0.24 },
        { {"", "6", "11"}, 0.34 },
        { {"", "6", "12"}, 0.34 },
        { {"", "7", "1"}, 0.14 },
        { {"", "7", "2"}, 0.17 },
        { {"", "7", "3"}, 0.21 },
        { {"", "7", "4"}, 0.21 },
        { {"", "7", "5"}, 0.23 },
        { {"", "7", "6"}, 0.3 },
        { {"", "7", "8"}, 0.3 },
        { {"", "7", "9"}, 0.18 },
        { {"", "7", "10"}, 0.23 },
        { {"", "7", "11"}, 0.33 },
        { {"", "7", "12"}, 0.33 },
        { {"", "8", "1"}, 0.14 },
        { {"", "8", "2"}, 0.17 },
        { {"", "8", "3"}, 0.17 },
        { {"", "8", "4"}, 0.22 },
        { {"", "8", "5"}, 0.26 },
        { {"", "8", "6"}, 0.33 },
        { {"", "8", "7"}, 0.3 },
        { {"", "8", "9"}, 0.17 },
        { {"", "8", "10"}, 0.24 },
        { {"", "8", "11"}, 0.37 },
        { {"", "8", "12"}, 0.37 },
        { {"", "9", "1"}, 0.16 },
        { {"", "9", "2"}, 0.19 },
        { {"", "9", "3"}, 0.21 },
        { {"", "9", "4"}, 0.2 },
        { {"", "9", "5"}, 0.13 },
        { {"", "9", "6"}, 0.17 },
        { {"", "9", "7"}, 0.18 },
        { {"", "9", "8"}, 0.17 },
        { {"", "9", "10"}, 0.13 },
        { {"", "9", "11"}, 0.2 },
        { {"", "9", "12"}, 0.2 },
        { {"", "10", "1"}, 0.11 },
        { {"", "10", "2"}, 0.13 },
        { {"", "10", "3"}, 0.13 },
        { {"", "10", "4"}, 0.16 },
        { {"", "10", "5"}, 0.19 },
        { {"", "10", "6"}, 0.24 },
        { {"", "10", "7"}, 0.23 },
        { {"", "10", "8"}, 0.24 },
        { {"", "10", "9"}, 0.13 },
        { {"", "10", "11"}, 0.24 },
        { {"", "10", "12"}, 0.24 },
        { {"", "11", "1"}, 0.17 },
        { {"", "11", "2"}, 0.19 },
        { {"", "11", "3"}, 0.2 },
        { {"", "11", "4"}, 0.24 },
        { {"", "11", "5"}, 0.29 },
        { {"", "11", "6"}, 0.34 },
        { {"", "11", "7"}, 0.33 },
        { {"", "11", "8"}, 0.37 },
        { {"", "11", "9"}, 0.2 },
        { {"", "11", "10"}, 0.24 },
        { {"", "11", "12"}, 0.43 },
        { {"", "12", "1"}, 0.17 },
        { {"", "12", "2"}, 0.19 },
        { {"", "12", "3"}, 0.2 },
        { {"", "12", "4"}, 0.24 },
        { {"", "12", "5"}, 0.29 },
        { {"", "12", "6"}, 0.34 },
        { {"", "12", "7"}, 0.33 },
        { {"", "12", "8"}, 0.37 },
        { {"", "12", "9"}, 0.2 },
        { {"", "12", "10"}, 0.24 },
        { {"", "12", "11"}, 0.43 }
    };

    interBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        { {"", "1", "2"}, 0.22 },
        { {"", "1", "3"}, 0.17 },
        { {"", "1", "4"}, 0.26 },
        { {"", "1", "5"}, 0.23 },
        { {"", "1", "6"}, 0.3 },
        { {"", "1", "7"}, 0.63 },
        { {"", "1", "8"}, 0.2 },
        { {"", "1", "9"}, 0.42 },
        { {"", "1", "10"}, 0.2 },
        { {"", "1", "11"}, 0.13 },
        { {"", "1", "12"}, 0.1 },
        { {"", "1", "13"}, 0.13 },
        { {"", "1", "14"}, 0.1 },
        { {"", "1", "15"}, 0.02 },
        { {"", "1", "16"}, 0 },
        { {"", "1", "17"}, 0.19 },
        { {"", "2", "1"}, 0.22 },
        { {"", "2", "3"}, 0.94 },
        { {"", "2", "4"}, 0.91 },
        { {"", "2", "5"}, 0.88 },
        { {"", "2", "6"}, 0.25 },
        { {"", "2", "7"}, 0.08 },
        { {"", "2", "8"}, 0.19 },
        { {"", "2", "9"}, 0.1 },
        { {"", "2", "10"}, 0.17 },
        { {"", "2", "11"}, 0.4 },
        { {"", "2", "12"}, 0.29 },
        { {"", "2", "13"}, 0.3 },
        { {"", "2", "14"}, 0.24 },
        { {"", "2", "15"}, 0.17 },
        { {"", "2", "16"}, 0 },
        { {"", "2", "17"}, 0.63 },
        { {"", "3", "1"}, 0.17 },
        { {"", "3", "2"}, 0.94 },
        { {"", "3", "4"}, 0.9 },
        { {"", "3", "5"}, 0.86 },
        { {"", "3", "6"}, 0.19 },
        { {"", "3", "7"}, 0.03 },
        { {"", "3", "8"}, 0.15 },
        { {"", "3", "9"}, 0.06 },
        { {"", "3", "10"}, 0.2 },
        { {"", "3", "11"}, 0.37 },
        { {"", "3", "12"}, 0.26 },
        { {"", "3", "13"}, 0.29 },
        { {"", "3", "14"}, 0.22 },
        { {"", "3", "15"}, 0.16 },
        { {"", "3", "16"}, 0 },
        { {"", "3", "17"}, 0.58 },
        { {"", "4", "1"}, 0.26 },
        { {"", "4", "2"}, 0.91 },
        { {"", "4", "3"}, 0.9 },
        { {"", "4", "5"}, 0.8 },
        { {"", "4", "6"}, 0.28 },
        { {"", "4", "7"}, 0.1 },
        { {"", "4", "8"}, 0.23 },
        { {"", "4", "9"}, 0.13 },
        { {"", "4", "10"}, 0.21 },
        { {"", "4", "11"}, 0.35 },
        { {"", "4", "12"}, 0.19 },
        { {"", "4", "13"}, 0.32 },
        { {"", "4", "14"}, 0.19 },
        { {"", "4", "15"}, 0.15 },
        { {"", "4", "16"}, 0 },
        { {"", "4", "17"}, 0.58 },
        { {"", "5", "1"}, 0.23 },
        { {"", "5", "2"}, 0.88 },
        { {"", "5", "3"}, 0.86 },
        { {"", "5", "4"}, 0.8 },
        { {"", "5", "6"}, 0.18 },
        { {"", "5", "7"}, 0.12 },
        { {"", "5", "8"}, 0.16 },
        { {"", "5", "9"}, 0.08 },
        { {"", "5", "10"}, 0.21 },
        { {"", "5", "11"}, 0.4 },
        { {"", "5", "12"}, 0.31 },
        { {"", "5", "13"}, 0.28 },
        { {"", "5", "14"}, 0.29 },
        { {"", "5", "15"}, 0.18 },
        { {"", "5", "16"}, 0 },
        { {"", "5", "17"}, 0.59 },
        { {"", "6", "1"}, 0.3 },
        { {"", "6", "2"}, 0.25 },
        { {"", "6", "3"}, 0.19 },
        { {"", "6", "4"}, 0.28 },
        { {"", "6", "5"}, 0.18 },
        { {"", "6", "7"}, 0.24 },
        { {"", "6", "8"}, 0.6 },
        { {"", "6", "9"}, 0.16 },
        { {"", "6", "10"}, 0 },
        { {"", "6", "11"}, 0.17 },
        { {"", "6", "12"}, 0.07 },
        { {"", "6", "13"}, 0.19 },
        { {"", "6", "14"}, 0.03 },
        { {"", "6", "15"}, 0.13 },
        { {"", "6", "16"}, 0 },
        { {"", "6", "17"}, 0.29 },
        { {"", "7", "1"}, 0.63 },
        { {"", "7", "2"}, 0.08 },
        { {"", "7", "3"}, 0.03 },
        { {"", "7", "4"}, 0.1 },
        { {"", "7", "5"}, 0.12 },
        { {"", "7", "6"}, 0.24 },
        { {"", "7", "8"}, 0.14 },
        { {"", "7", "9"}, 0.7 },
        { {"", "7", "10"}, 0.07 },
        { {"", "7", "11"}, 0.09 },
        { {"", "7", "12"}, 0.08 },
        { {"", "7", "13"}, 0.12 },
        { {"", "7", "14"}, 0.05 },
        { {"", "7", "15"}, 0.01 },
        { {"", "7", "16"}, 0 },
        { {"", "7", "17"}, 0.16 },
        { {"", "8", "1"}, 0.2 },
        { {"", "8", "2"}, 0.19 },
        { {"", "8", "3"}, 0.15 },
        { {"", "8", "4"}, 0.23 },
        { {"", "8", "5"}, 0.16 },
        { {"", "8", "6"}, 0.6 },
        { {"", "8", "7"}, 0.14 },
        { {"", "8", "9"}, 0.14 },
        { {"", "8", "10"}, 0 },
        { {"", "8", "11"}, 0.1 },
        { {"", "8", "12"}, 0.03 },
        { {"", "8", "13"}, 0.13 },
        { {"", "8", "14"}, 0.03 },
        { {"", "8", "15"}, 0.06 },
        { {"", "8", "16"}, 0 },
        { {"", "8", "17"}, 0.19 },
        { {"", "9", "1"}, 0.42 },
        { {"", "9", "2"}, 0.1 },
        { {"", "9", "3"}, 0.06 },
        { {"", "9", "4"}, 0.13 },
        { {"", "9", "5"}, 0.08 },
        { {"", "9", "6"}, 0.16 },
        { {"", "9", "7"}, 0.7 },
        { {"", "9", "8"}, 0.14 },
        { {"", "9", "10"}, 0.03 },
        { {"", "9", "11"}, 0.06 },
        { {"", "9", "12"}, -0.02 },
        { {"", "9", "13"}, 0.14 },
        { {"", "9", "14"}, 0.04 },
        { {"", "9", "15"}, 0.01 },
        { {"", "9", "16"}, 0 },
        { {"", "9", "17"}, 0.15 },
        { {"", "10", "1"}, 0.2 },
        { {"", "10", "2"}, 0.17 },
        { {"", "10", "3"}, 0.2 },
        { {"", "10", "4"}, 0.21 },
        { {"", "10", "5"}, 0.21 },
        { {"", "10", "6"}, 0 },
        { {"", "10", "7"}, 0.07 },
        { {"", "10", "8"}, 0 },
        { {"", "10", "9"}, 0.03 },
        { {"", "10", "11"}, 0.16 },
        { {"", "10", "12"}, 0.1 },
        { {"", "10", "13"}, 0.08 },
        { {"", "10", "14"}, 0.1 },
        { {"", "10", "15"}, 0.01 },
        { {"", "10", "16"}, 0 },
        { {"", "10", "17"}, 0.08 },
        { {"", "11", "1"}, 0.13 },
        { {"", "11", "2"}, 0.4 },
        { {"", "11", "3"}, 0.37 },
        { {"", "11", "4"}, 0.35 },
        { {"", "11", "5"}, 0.4 },
        { {"", "11", "6"}, 0.17 },
        { {"", "11", "7"}, 0.09 },
        { {"", "11", "8"}, 0.1 },
        { {"", "11", "9"}, 0.06 },
        { {"", "11", "10"}, 0.16 },
        { {"", "11", "12"}, 0.41 },
        { {"", "11", "13"}, 0.28 },
        { {"", "11", "14"}, 0.22 },
        { {"", "11", "15"}, 0.18 },
        { {"", "11", "16"}, 0 },
        { {"", "11", "17"}, 0.37 },
        { {"", "12", "1"}, 0.1 },
        { {"", "12", "2"}, 0.29 },
        { {"", "12", "3"}, 0.26 },
        { {"", "12", "4"}, 0.19 },
        { {"", "12", "5"}, 0.31 },
        { {"", "12", "6"}, 0.07 },
        { {"", "12", "7"}, 0.08 },
        { {"", "12", "8"}, 0.03 },
        { {"", "12", "9"}, -0.02 },
        { {"", "12", "10"}, 0.1 },
        { {"", "12", "11"}, 0.41 },
        { {"", "12", "13"}, 0.2 },
        { {"", "12", "14"}, 0.19 },
        { {"", "12", "15"}, 0.1 },
        { {"", "12", "16"}, 0 },
        { {"", "12", "17"}, 0.25 },
        { {"", "13", "1"}, 0.13 },
        { {"", "13", "2"}, 0.3 },
        { {"", "13", "3"}, 0.29 },
        { {"", "13", "4"}, 0.32 },
        { {"", "13", "5"}, 0.28 },
        { {"", "13", "6"}, 0.19 },
        { {"", "13", "7"}, 0.12 },
        { {"", "13", "8"}, 0.13 },
        { {"", "13", "9"}, 0.14 },
        { {"", "13", "10"}, 0.08 },
        { {"", "13", "11"}, 0.28 },
        { {"", "13", "12"}, 0.2 },
        { {"", "13", "14"}, 0.17 },
        { {"", "13", "15"}, 0.16 },
        { {"", "13", "16"}, 0 },
        { {"", "13", "17"}, 0.32 },
        { {"", "14", "1"}, 0.1 },
        { {"", "14", "2"}, 0.24 },
        { {"", "14", "3"}, 0.22 },
        { {"", "14", "4"}, 0.19 },
        { {"", "14", "5"}, 0.29 },
        { {"", "14", "6"}, 0.03 },
        { {"", "14", "7"}, 0.05 },
        { {"", "14", "8"}, 0.03 },
        { {"", "14", "9"}, 0.04 },
        { {"", "14", "10"}, 0.1 },
        { {"", "14", "11"}, 0.22 },
        { {"", "14", "12"}, 0.19 },
        { {"", "14", "13"}, 0.17 },
        { {"", "14", "15"}, 0.13 },
        { {"", "14", "16"}, 0 },
        { {"", "14", "17"}, 0.22 },
        { {"", "15", "1"}, 0.02 },
        { {"", "15", "2"}, 0.17 },
        { {"", "15", "3"}, 0.16 },
        { {"", "15", "4"}, 0.15 },
        { {"", "15", "5"}, 0.18 },
        { {"", "15", "6"}, 0.13 },
        { {"", "15", "7"}, 0.01 },
        { {"", "15", "8"}, 0.06 },
        { {"", "15", "9"}, 0.01 },
        { {"", "15", "10"}, 0.01 },
        { {"", "15", "11"}, 0.18 },
        { {"", "15", "12"}, 0.1 },
        { {"", "15", "13"}, 0.16 },
        { {"", "15", "14"}, 0.13 },
        { {"", "15", "16"}, 0 },
        { {"", "15", "17"}, 0.18 },
        { {"", "16", "1"}, 0 },
        { {"", "16", "2"}, 0 },
        { {"", "16", "3"}, 0 },
        { {"", "16", "4"}, 0 },
        { {"", "16", "5"}, 0 },
        { {"", "16", "6"}, 0 },
        { {"", "16", "7"}, 0 },
        { {"", "16", "8"}, 0 },
        { {"", "16", "9"}, 0 },
        { {"", "16", "10"}, 0 },
        { {"", "16", "11"}, 0 },
        { {"", "16", "12"}, 0 },
        { {"", "16", "13"}, 0 },
        { {"", "16", "14"}, 0 },
        { {"", "16", "15"}, 0 },
        { {"", "16", "17"}, 0 },
        { {"", "17", "1"}, 0.19 },
        { {"", "17", "2"}, 0.63 },
        { {"", "17", "3"}, 0.58 },
        { {"", "17", "4"}, 0.58 },
        { {"", "17", "5"}, 0.59 },
        { {"", "17", "6"}, 0.29 },
        { {"", "17", "7"}, 0.16 },
        { {"", "17", "8"}, 0.19 },
        { {"", "17", "9"}, 0.15 },
        { {"", "17", "10"}, 0.08 },
        { {"", "17", "11"}, 0.37 },
        { {"", "17", "12"}, 0.25 },
        { {"", "17", "13"}, 0.32 },
        { {"", "17", "14"}, 0.22 },
        { {"", "17", "15"}, 0.18 },
        { {"", "17", "16"}, 0 }
    };

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[CrifRecord::RiskType::Equity] = {
        { {"1", "", ""}, 0.14 },
        { {"2", "", ""}, 0.16 },
        { {"3", "", ""}, 0.26 },
        { {"4", "", ""}, 0.21 },
        { {"5", "", ""}, 0.22 },
        { {"6", "", ""}, 0.31 },
        { {"7", "", ""}, 0.34 },
        { {"8", "", ""}, 0.35 },
        { {"9", "", ""}, 0.25 },
        { {"10", "", ""}, 0.21 },
        { {"11", "", ""}, 0.43 },
        { {"12", "", ""}, 0.43 },
        { {"Residual", "", ""}, 0 }
    };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        { {"1", "", ""}, 0.83 },
        { {"2", "", ""}, 0.98 },
        { {"3", "", ""}, 0.98 },
        { {"4", "", ""}, 0.98 },
        { {"5", "", ""}, 0.98 },
        { {"6", "", ""}, 0.94 },
        { {"7", "", ""}, 0.94 },
        { {"8", "", ""}, 0.37 },
        { {"9", "", ""}, 0.58 },
        { {"10", "", ""}, 0.5 },
        { {"11", "", ""}, 0.61 },
        { {"12", "", ""}, 0.62 },
        { {"13", "", ""}, 0.57 },
        { {"14", "", ""}, 0.14 },
        { {"15", "", ""}, 0.16 },
        { {"16", "", ""}, 0 },
        { {"17", "", ""}, 0.34 }
    };

    // Initialise the single, ad-hoc type, correlations
    xccyCorr_ = -0.05;
    infCorr_ = 0.37;
    infVolCorr_ = 0.37;
    irSubCurveCorr_ = 0.983;
    irInterCurrencyCorr_ = 0.34;
    crqResidualIntraCorr_ = 0.50;
    crqSameIntraCorr_ = 0.94;
    crqDiffIntraCorr_ = 0.47;
    crnqResidualIntraCorr_ = 0.50;
    crnqSameIntraCorr_ = 0.85;
    crnqDiffIntraCorr_ = 0.29;
    crnqInterCorr_ = 0.61;
    fxCorr_ = 0.50;
    basecorrCorr_ = 0.21;

    // clang-format on
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 8 section 11(d)
of the ISDA-SIMM-v2.7 documentation).
*/
QuantLib::Real SimmConfiguration_ISDA_V2_7_2412_1::curvatureMarginScaling() const { return std::pow(hvr_ir_, -2.0); }

void SimmConfiguration_ISDA_V2_7_2412_1::addLabels2(const CrifRecord::RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V2_7_2412_1::label2(const QuantLib::ext::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::label2(irIndex);
}

} // namespace analytics
} // namespace ore
