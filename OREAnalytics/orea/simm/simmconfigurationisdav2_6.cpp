/*
 Copyright (C) 2023 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <orea/simm/simmconcentrationisdav2_6.hpp>
#include <orea/simm/simmconfigurationisdav2_6.hpp>
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

QuantLib::Size SimmConfiguration_ISDA_V2_6::group(const string& qualifier,
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

QuantLib::Real SimmConfiguration_ISDA_V2_6::weight(const RiskType& rt, boost::optional<string> qualifier,
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

QuantLib::Real SimmConfiguration_ISDA_V2_6::correlation(const RiskType& firstRt, const string& firstQualifier,
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

SimmConfiguration_ISDA_V2_6::SimmConfiguration_ISDA_V2_6(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const QuantLib::Size& mporDays, const std::string& name,
                                                         const std::string version)
     : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = boost::make_shared<SimmConcentration_ISDA_V2_6>(simmBucketMapper_);
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
    // The groups consists of High Vol Currencies & regular vol currencies
    ccyGroups_ = {
        { 1, { "BRL", "RUB", "TRY" } },
        { 0, {  } }
    };

    vector<Real> temp;

    if (mporDays_ == 10) {
        // Risk weights
        temp = {
           7.4,  14.7,
           14.7, 21.4
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 61 },
            { RiskType::XCcyBasis, 21 },
            { RiskType::IRVol, 0.23 },
            { RiskType::InflationVol, 0.23 },
            { RiskType::CreditVol, 0.76 },
            { RiskType::CreditVolNonQ, 0.76 },
            { RiskType::CommodityVol, 0.55 },
            { RiskType::FXVol, 0.48 },
            { RiskType::BaseCorr, 10 }
        };

        rwBucket_ = {
            { RiskType::CreditQ, { 75, 90, 84, 54, 62, 48, 185, 343, 255, 250, 214, 173, 343 } },
            { RiskType::CreditNonQ, { 280, 1300, 1300 } },
            { RiskType::Equity, { 26, 28, 33, 27, 23, 25, 31, 27, 30, 29, 18, 18, 33 } },
            { RiskType::Commodity, { 48, 29, 33, 25, 35, 30, 60, 52, 68, 63, 21, 21, 15, 16, 13, 68, 17 } },
            { RiskType::EquityVol, { 0.45, 0.45, 0.45, 0.45, 0.45, 0.45, 0.45, 0.45, 0.45, 0.45, 0.45, 0.96, 0.45 } },
        };

        rwLabel_1_ = {
            { { RiskType::IRCurve, "1" }, { 109, 105, 90, 71, 66, 66, 64, 60, 60, 61, 61, 67 } },
            { { RiskType::IRCurve, "2" }, { 15, 18, 9.0, 11, 13, 15, 19, 23, 23, 22, 22, 23 } },
            { { RiskType::IRCurve, "3" }, { 163, 109, 87, 89, 102, 96, 101, 97, 97, 102, 106, 101 } },
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.6;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.74;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.57;
        hvr_ir_ = 0.47;

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
           1.8,  3.5,
           3.5, 4.5
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 15 },
            { RiskType::XCcyBasis, 6.0 },
            { RiskType::IRVol, 0.046 },
            { RiskType::InflationVol, 0.046 },
            { RiskType::CreditVol, 0.09 },
            { RiskType::CreditVolNonQ, 0.09 },
            { RiskType::CommodityVol, 0.14 },
            { RiskType::FXVol, 0.1 },
            { RiskType::BaseCorr, 2.4 }
        };

        rwBucket_ = {
            { RiskType::CreditQ, { 20, 27, 17, 12, 13, 12, 50, 93, 51, 57, 43, 37, 93 } },
            { RiskType::CreditNonQ, { 66, 280, 280 } },
            { RiskType::Equity, { 9.1, 9.8, 10.0, 9.0, 7.7, 8.5, 9.9, 9.8, 9.9, 10, 6.1, 6.1, 10.0 } },
            { RiskType::Commodity, { 11, 9.1, 8.3, 7.4, 10, 9.3, 17, 12, 14, 18, 6.6, 6.7, 5.0, 4.8, 3.8, 18, 5.2 } },
            { RiskType::EquityVol, { 0.093, 0.093, 0.093, 0.093, 0.093, 0.093, 0.093, 0.093, 0.093, 0.093, 0.093, 0.23, 0.093 } },
        };

        rwLabel_1_ = {
            { { RiskType::IRCurve, "1" }, { 19, 15, 12, 13, 15, 18, 18, 18, 18, 18, 17, 18 } },
            { { RiskType::IRCurve, "2" }, { 1.7, 2.9, 1.7, 2.0, 3.4, 4.8, 5.8, 7.3, 7.8, 7.5, 8.0, 9.0 } },
            { { RiskType::IRCurve, "3" }, { 55, 29, 18, 21, 26, 25, 34, 33, 34, 31, 34, 28 } },
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.55;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.74;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.74;
        hvr_ir_ = 0.51;

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
        1.00, 0.04, 0.04, 0.07, 0.37, 0.14,
        0.04, 1.00, 0.54, 0.7, 0.27, 0.37,
        0.04, 0.54, 1.00, 0.46, 0.24, 0.15,
        0.07, 0.7, 0.46, 1.00, 0.35, 0.39,
        0.37, 0.27, 0.24, 0.35, 1.00, 0.35,
        0.14, 0.37, 0.15, 0.39, 0.35, 1.00
    };
    riskClassCorrelation_ = Matrix(6, 6, temp.begin(), temp.end());

    // FX correlations
    temp = {
        0.5, 0.25,
        0.25, -0.05
    };
    fxRegVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    temp = {
        0.88, 0.72,
        0.72, 0.5
    };
    fxHighVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations)
    temp = {
        1.0, 0.77, 0.67, 0.59, 0.48, 0.39, 0.34, 0.3, 0.25, 0.23, 0.21, 0.2,
        0.77, 1.0, 0.84, 0.74, 0.56, 0.43, 0.36, 0.31, 0.26, 0.21, 0.19, 0.19,
        0.67, 0.84, 1.0, 0.88, 0.69, 0.55, 0.47, 0.4, 0.34, 0.27, 0.25, 0.25,
        0.59, 0.74, 0.88, 1.0, 0.86, 0.73, 0.65, 0.57, 0.49, 0.4, 0.38, 0.37,
        0.48, 0.56, 0.69, 0.86, 1.0, 0.94, 0.87, 0.79, 0.68, 0.6, 0.57, 0.55,
        0.39, 0.43, 0.55, 0.73, 0.94, 1.0, 0.96, 0.91, 0.8, 0.74, 0.7, 0.69,
        0.34, 0.36, 0.47, 0.65, 0.87, 0.96, 1.0, 0.97, 0.88, 0.81, 0.77, 0.76,
        0.3, 0.31, 0.4, 0.57, 0.79, 0.91, 0.97, 1.0, 0.95, 0.9, 0.86, 0.85,
        0.25, 0.26, 0.34, 0.49, 0.68, 0.8, 0.88, 0.95, 1.0, 0.97, 0.94, 0.94,
        0.23, 0.21, 0.27, 0.4, 0.6, 0.74, 0.81, 0.9, 0.97, 1.0, 0.98, 0.97,
        0.21, 0.19, 0.25, 0.38, 0.57, 0.7, 0.77, 0.86, 0.94, 0.98, 1.0, 0.99,
        0.2, 0.19, 0.25, 0.37, 0.55, 0.69, 0.76, 0.85, 0.94, 0.97, 0.99, 1.0
    };
    irTenorCorrelation_ = Matrix(12, 12, temp.begin(), temp.end());

    // CreditQ inter-bucket correlations
     temp = {
        1.0, 0.38, 0.38, 0.35, 0.37, 0.34, 0.42, 0.32, 0.34, 0.33, 0.34, 0.33,
        0.38, 1.0, 0.48, 0.46, 0.48, 0.46, 0.39, 0.4, 0.41, 0.41, 0.43, 0.4,
        0.38, 0.48, 1.0, 0.5, 0.51, 0.5, 0.4, 0.39, 0.45, 0.44, 0.47, 0.42,
        0.35, 0.46, 0.5, 1.0, 0.5, 0.5, 0.37, 0.37, 0.41, 0.43, 0.45, 0.4,
        0.37, 0.48, 0.51, 0.5, 1.0, 0.5, 0.39, 0.38, 0.43, 0.43, 0.46, 0.42,
        0.34, 0.46, 0.5, 0.5, 0.5, 1.0, 0.37, 0.35, 0.39, 0.41, 0.44, 0.41,
        0.42, 0.39, 0.4, 0.37, 0.39, 0.37, 1.0, 0.33, 0.37, 0.37, 0.35, 0.35,
        0.32, 0.4, 0.39, 0.37, 0.38, 0.35, 0.33, 1.0, 0.36, 0.37, 0.37, 0.36,
        0.34, 0.41, 0.45, 0.41, 0.43, 0.39, 0.37, 0.36, 1.0, 0.41, 0.4, 0.38,
        0.33, 0.41, 0.44, 0.43, 0.43, 0.41, 0.37, 0.37, 0.41, 1.0, 0.41, 0.39,
        0.34, 0.43, 0.47, 0.45, 0.46, 0.44, 0.35, 0.37, 0.4, 0.41, 1.0, 0.4,
        0.33, 0.4, 0.42, 0.4, 0.42, 0.41, 0.35, 0.36, 0.38, 0.39, 0.4, 1.0
    };
    interBucketCorrelation_[RiskType::CreditQ] = Matrix(12, 12, temp.begin(), temp.end());

     // Equity inter-bucket correlations
     temp = {
        1.0, 0.18, 0.19, 0.19, 0.14, 0.16, 0.15, 0.16, 0.18, 0.12, 0.19, 0.19,
        0.18, 1.0, 0.22, 0.21, 0.15, 0.18, 0.18, 0.19, 0.2, 0.14, 0.21, 0.21,
        0.19, 0.22, 1.0, 0.22, 0.13, 0.17, 0.19, 0.17, 0.22, 0.13, 0.19, 0.19,
        0.19, 0.21, 0.22, 1.0, 0.18, 0.22, 0.22, 0.23, 0.22, 0.17, 0.25, 0.25,
        0.14, 0.15, 0.13, 0.18, 1.0, 0.29, 0.26, 0.29, 0.14, 0.24, 0.31, 0.31,
        0.16, 0.18, 0.17, 0.22, 0.29, 1.0, 0.33, 0.36, 0.17, 0.29, 0.38, 0.38,
        0.15, 0.18, 0.19, 0.22, 0.26, 0.33, 1.0, 0.33, 0.17, 0.28, 0.36, 0.36,
        0.16, 0.19, 0.17, 0.23, 0.29, 0.36, 0.33, 1.0, 0.18, 0.29, 0.39, 0.39,
        0.18, 0.2, 0.22, 0.22, 0.14, 0.17, 0.17, 0.18, 1.0, 0.13, 0.21, 0.21,
        0.12, 0.14, 0.13, 0.17, 0.24, 0.29, 0.28, 0.29, 0.13, 1.0, 0.3, 0.3,
        0.19, 0.21, 0.19, 0.25, 0.31, 0.38, 0.36, 0.39, 0.21, 0.3, 1.0, 0.44,
        0.19, 0.21, 0.19, 0.25, 0.31, 0.38, 0.36, 0.39, 0.21, 0.3, 0.44, 1.0
    };
    interBucketCorrelation_[RiskType::Equity] = Matrix(12, 12, temp.begin(), temp.end());

    // Commodity inter-bucket correlations
    temp = {
        1.0, 0.22, 0.18, 0.21, 0.2, 0.24, 0.49, 0.16, 0.38, 0.14, 0.1, 0.02, 0.12, 0.11, 0.02, 0.0, 0.17,
        0.22, 1.0, 0.92, 0.9, 0.88, 0.25, 0.08, 0.19, 0.17, 0.17, 0.42, 0.28, 0.36, 0.27, 0.2, 0.0, 0.64,
        0.18, 0.92, 1.0, 0.87, 0.84, 0.16, 0.07, 0.15, 0.1, 0.18, 0.33, 0.22, 0.27, 0.23, 0.16, 0.0, 0.54,
        0.21, 0.9, 0.87, 1.0, 0.77, 0.19, 0.11, 0.18, 0.16, 0.14, 0.32, 0.22, 0.28, 0.22, 0.11, 0.0, 0.58,
        0.2, 0.88, 0.84, 0.77, 1.0, 0.19, 0.09, 0.12, 0.13, 0.18, 0.42, 0.34, 0.32, 0.29, 0.13, 0.0, 0.59,
        0.24, 0.25, 0.16, 0.19, 0.19, 1.0, 0.31, 0.62, 0.23, 0.1, 0.21, 0.05, 0.18, 0.1, 0.08, 0.0, 0.28,
        0.49, 0.08, 0.07, 0.11, 0.09, 0.31, 1.0, 0.21, 0.79, 0.17, 0.1, -0.08, 0.1, 0.07, -0.02, 0.0, 0.13,
        0.16, 0.19, 0.15, 0.18, 0.12, 0.62, 0.21, 1.0, 0.16, 0.08, 0.13, -0.07, 0.07, 0.05, 0.02, 0.0, 0.19,
        0.38, 0.17, 0.1, 0.16, 0.13, 0.23, 0.79, 0.16, 1.0, 0.15, 0.09, -0.06, 0.06, 0.06, 0.01, 0.0, 0.16,
        0.14, 0.17, 0.18, 0.14, 0.18, 0.1, 0.17, 0.08, 0.15, 1.0, 0.16, 0.09, 0.14, 0.09, 0.03, 0.0, 0.11,
        0.1, 0.42, 0.33, 0.32, 0.42, 0.21, 0.1, 0.13, 0.09, 0.16, 1.0, 0.36, 0.3, 0.25, 0.18, 0.0, 0.37,
        0.02, 0.28, 0.22, 0.22, 0.34, 0.05, -0.08, -0.07, -0.06, 0.09, 0.36, 1.0, 0.2, 0.18, 0.11, 0.0, 0.26,
        0.12, 0.36, 0.27, 0.28, 0.32, 0.18, 0.1, 0.07, 0.06, 0.14, 0.3, 0.2, 1.0, 0.28, 0.19, 0.0, 0.39,
        0.11, 0.27, 0.23, 0.22, 0.29, 0.1, 0.07, 0.05, 0.06, 0.09, 0.25, 0.18, 0.28, 1.0, 0.13, 0.0, 0.26,
        0.02, 0.2, 0.16, 0.11, 0.13, 0.08, -0.02, 0.02, 0.01, 0.03, 0.18, 0.11, 0.19, 0.13, 1.0, 0.0, 0.21,
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
        0.17, 0.64, 0.54, 0.58, 0.59, 0.28, 0.13, 0.19, 0.16, 0.11, 0.37, 0.26, 0.39, 0.26, 0.21, 0.0, 1.0
    };
    interBucketCorrelation_[RiskType::Commodity] = Matrix(17, 17, temp.begin(), temp.end());

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
   intraBucketCorrelation_[RiskType::Equity] = { 0.18, 0.2, 0.28, 0.24, 0.25, 0.35, 0.35, 0.37, 0.23, 0.26, 0.44, 0.44 };

    // Commodity intra-bucket correlations
   intraBucketCorrelation_[RiskType::Commodity] = { 0.83, 0.97, 0.93, 0.97, 0.98, 0.9, 0.98, 0.49, 0.8, 0.46, 0.58, 0.53, 0.62, 0.16, 0.18, 0, 0.38 };

    // Initialise the single, ad-hoc type, correlations
    xccyCorr_ = 0.04;
    infCorr_ = 0.24;
    infVolCorr_ = 0.24;
    irSubCurveCorr_ = 0.993;
    irInterCurrencyCorr_ = 0.32;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.93;
    crqDiffIntraCorr_ = 0.46;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.83;
    crnqDiffIntraCorr_ = 0.32;
    crnqInterCorr_ = 0.43;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.29;

    // clang-format on
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 8 section 11(d)
of the ISDA-SIMM-v2.6 documentation).
*/
QuantLib::Real SimmConfiguration_ISDA_V2_6::curvatureMarginScaling() const { return pow(hvr_ir_, -2.0); }

void SimmConfiguration_ISDA_V2_6::addLabels2(const RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V2_6::labels2(const boost::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::labels2(irIndex);
}

} // namespace analytics
} // namespace ore
