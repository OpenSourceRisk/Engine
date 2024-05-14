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

using RiskType = CrifRecord::RiskType;

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

SimmConfiguration_ISDA_V2_2::SimmConfiguration_ISDA_V2_2(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const QuantLib::Size& mporDays, const std::string& name,
                                                         const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentration_ISDA_V2_2>(simmBucketMapper_);
    } else {
        // SIMM:Technical Paper, Section I.4: "The Concentration Risk feature is disabled"
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentrationBase>();
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
            {RiskType::CreditQ,
             {{{"1", "", ""}, 72.0},
              {{"2", "", ""}, 97.0},
              {{"3", "", ""}, 75.0},
              {{"4", "", ""}, 53.0},
              {{"5", "", ""}, 45.0},
              {{"6", "", ""}, 53.0},
              {{"7", "", ""}, 165.0},
              {{"8", "", ""}, 250.0},
              {{"9", "", ""}, 191.0},
              {{"10", "", ""}, 179.0},
              {{"11", "", ""}, 132.0},
              {{"12", "", ""}, 129.0},
              {{"Residual", "", ""}, 250.0}}},
            {RiskType::CreditNonQ, {{{"1", "", ""}, 100.0}, {{"2", "", ""}, 1600.0}, {{"Residual", "", ""}, 1600.0}}},
            {RiskType::Equity,
             {{{"1", "", ""}, 22.0},
              {{"2", "", ""}, 26.0},
              {{"3", "", ""}, 29.0},
              {{"4", "", ""}, 26.0},
              {{"5", "", ""}, 19.0},
              {{"6", "", ""}, 21.0},
              {{"7", "", ""}, 25.0},
              {{"8", "", ""}, 24.0},
              {{"9", "", ""}, 30.0},
              {{"10", "", ""}, 29.0},
              {{"11", "", ""}, 17.0},
              {{"12", "", ""}, 17.0},
              {{"Residual", "", ""}, 30.0}}},
            {RiskType::Commodity,
             {{{"1", "", ""}, 19.0},
              {{"2", "", ""}, 20.0},
              {{"3", "", ""}, 16.0},
              {{"4", "", ""}, 19.0},
              {{"5", "", ""}, 24.0},
              {{"6", "", ""}, 24.0},
              {{"7", "", ""}, 28.0},
              {{"8", "", ""}, 42.0},
              {{"9", "", ""}, 28.0},
              {{"10", "", ""}, 53.0},
              {{"11", "", ""}, 20.0},
              {{"12", "", ""}, 19.0},
              {{"13", "", ""}, 15.0},
              {{"14", "", ""}, 16.0},
              {{"15", "", ""}, 11.0},
              {{"16", "", ""}, 53.0},
              {{"17", "", ""}, 16.0}}},
            {RiskType::EquityVol,
             {{{"1", "", ""}, 0.26},
              {{"2", "", ""}, 0.26},
              {{"3", "", ""}, 0.26},
              {{"4", "", ""}, 0.26},
              {{"5", "", ""}, 0.26},
              {{"6", "", ""}, 0.26},
              {{"7", "", ""}, 0.26},
              {{"8", "", ""}, 0.26},
              {{"9", "", ""}, 0.26},
              {{"10", "", ""}, 0.26},
              {{"11", "", ""}, 0.26},
              {{"12", "", ""}, 0.62},
              {{"Residual", "", ""}, 0.26}}}
        };

        rwLabel_1_ = {
            {RiskType::IRCurve,
                {{{"1", "2w", ""}, 116.0},
                 {{"1", "1m", ""}, 106.0},
                 {{"1", "3m", ""}, 94.0},
                 {{"1", "6m", ""}, 71.0},
                 {{"1", "1y", ""}, 59.0},
                 {{"1", "2y", ""}, 52.0},
                 {{"1", "3y", ""}, 49.0},
                 {{"1", "5y", ""}, 51.0},
                 {{"1", "10y", ""}, 51.0},
                 {{"1", "15y", ""}, 51.0},
                 {{"1", "20y", ""}, 54.0},
                 {{"1", "30y", ""}, 62.0},
                 {{"2", "2w", ""}, 14.0},
                 {{"2", "1m", ""}, 20.0},
                 {{"2", "3m", ""}, 10.0},
                 {{"2", "6m", ""}, 10.0},
                 {{"2", "1y", ""}, 14.0},
                 {{"2", "2y", ""}, 20.0},
                 {{"2", "3y", ""}, 22.0},
                 {{"2", "5y", ""}, 20.0},
                 {{"2", "10y", ""}, 20.0},
                 {{"2", "15y", ""}, 20.0},
                 {{"2", "20y", ""}, 22.0},
                 {{"2", "30y", ""}, 27.0},
                 {{"3", "2w", ""}, 85.0},
                 {{"3", "1m", ""}, 80.0},
                 {{"3", "3m", ""}, 79.0},
                 {{"3", "6m", ""}, 86.0},
                 {{"3", "1y", ""}, 97.0},
                 {{"3", "2y", ""}, 102.0},
                 {{"3", "3y", ""}, 104.0},
                 {{"3", "5y", ""}, 102.0},
                 {{"3", "10y", ""}, 103.0},
                 {{"3", "15y", ""}, 99.0},
                 {{"3", "20y", ""}, 99.0},
                 {{"3", "30y", ""}, 100.0}}
            }
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
            {RiskType::CreditQ,
             {{{"1", "", ""}, 16.0},
              {{"2", "", ""}, 21.0},
              {{"3", "", ""}, 15.0},
              {{"4", "", ""}, 11.0},
              {{"5", "", ""}, 10.0},
              {{"6", "", ""}, 12.0},
              {{"7", "", ""}, 37.0},
              {{"8", "", ""}, 42.0},
              {{"9", "", ""}, 31.0},
              {{"10", "", ""}, 33.0},
              {{"11", "", ""}, 29.0},
              {{"12", "", ""}, 27.0},
              {{"Residual", "", ""}, 42.0}}},
            {RiskType::CreditNonQ, {{{"1", "", ""}, 20}, {{"2", "", ""}, 320}, {{"Residual", "", ""}, 320.0}}},
            {RiskType::Equity,
             {{{"1", "", ""}, 7.6},
              {{"2", "", ""}, 8.4},
              {{"3", "", ""}, 9.7},
              {{"4", "", ""}, 8.4},
              {{"5", "", ""}, 6.8},
              {{"6", "", ""}, 7.4},
              {{"7", "", ""}, 8.7},
              {{"8", "", ""}, 8.7},
              {{"9", "", ""}, 9.9},
              {{"10", "", ""}, 9.6},
              {{"11", "", ""}, 7.0},
              {{"12", "", ""}, 7.0},
              {{"Residual", "", ""}, 9.9}}},
            {RiskType::Commodity,
             {{{"1", "", ""}, 5.6},
              {{"2", "", ""}, 8.2},
              {{"3", "", ""}, 5.8},
              {{"4", "", ""}, 6.4},
              {{"5", "", ""}, 10.0},
              {{"6", "", ""}, 9.1},
              {{"7", "", ""}, 8.0},
              {{"8", "", ""}, 12.3},
              {{"9", "", ""}, 6.9},
              {{"10", "", ""}, 12.4},
              {{"11", "", ""}, 6.4},
              {{"12", "", ""}, 5.7},
              {{"13", "", ""}, 5.0},
              {{"14", "", ""}, 4.8},
              {{"15", "", ""}, 3.9},
              {{"16", "", ""}, 12.4},
              {{"17", "", ""}, 4.7}}},
            {RiskType::EquityVol,
             {{{"1", "", ""}, 0.066},
              {{"2", "", ""}, 0.066},
              {{"3", "", ""}, 0.066},
              {{"4", "", ""}, 0.066},
              {{"5", "", ""}, 0.066},
              {{"6", "", ""}, 0.066},
              {{"7", "", ""}, 0.066},
              {{"8", "", ""}, 0.066},
              {{"9", "", ""}, 0.066},
              {{"10", "", ""}, 0.066},
              {{"11", "", ""}, 0.066},
              {{"12", "", ""}, 0.21},
              {{"Residual", "", ""}, 0.066}}}
        };
        rwLabel_1_ = {
            {RiskType::IRCurve,
                {{{"1", "2w", ""}, 18.0},
                 {{"1", "1m", ""}, 16.0},
                 {{"1", "3m", ""}, 11.0},
                 {{"1", "6m", ""}, 11.0},
                 {{"1", "1y", ""}, 13.0},
                 {{"1", "2y", ""}, 14.0},
                 {{"1", "3y", ""}, 14.0},
                 {{"1", "5y", ""}, 16.0},
                 {{"1", "10y", ""}, 16.0},
                 {{"1", "15y", ""},16.0 },
                 {{"1", "20y", ""}, 16.0},
                 {{"1", "30y", ""}, 16.0},
                 {{"2", "2w", ""}, 3.5},
                 {{"2", "1m", ""}, 3.4},
                 {{"2", "3m", ""}, 1.7},
                 {{"2", "6m", ""}, 1.5},
                 {{"2", "1y", ""}, 3.2},
                 {{"2", "2y", ""}, 4.4},
                 {{"2", "3y", ""}, 5.1},
                 {{"2", "5y", ""}, 6.6},
                 {{"2", "10y", ""}, 7.3},
                 {{"2", "15y", ""}, 7.1},
                 {{"2", "20y", ""}, 7.8},
                 {{"2", "30y", ""}, 8.6},
                 {{"3", "2w", ""}, 41.0},
                 {{"3", "1m", ""}, 24.0},
                 {{"3", "3m", ""}, 17.0},
                 {{"3", "6m", ""}, 17.0},
                 {{"3", "1y", ""}, 20.0},
                 {{"3", "2y", ""}, 23.0},
                 {{"3", "3y", ""}, 24.0},
                 {{"3", "5y", ""}, 27.0},
                 {{"3", "10y", ""}, 30.0},
                 {{"3", "15y", ""}, 28.0},
                 {{"3", "20y", ""}, 29.0},
                 {{"3", "30y", ""}, 29.0}}
            }
        };

        // Historical volatility ratios 
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.55;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.69;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.82;
        hvr_ir_ = 0.59;
    // clang-format on

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
    riskClassCorrelation_ = {
        {{"", "InterestRate", "CreditQualifying"}, 0.27},
        {{"", "InterestRate", "CreditNonQualifying"}, 0.26},
        {{"", "InterestRate", "Equity"}, 0.29},
        {{"", "InterestRate", "Commodity"}, 0.31},
        {{"", "InterestRate", "FX"}, 0.14},
        {{"", "CreditQualifying", "InterestRate"}, 0.27},
        {{"", "CreditQualifying", "CreditNonQualifying"}, 0.19},
        {{"", "CreditQualifying", "Equity"}, 0.63},
        {{"", "CreditQualifying", "Commodity"}, 0.42},
        {{"", "CreditQualifying", "FX"}, 0.25},
        {{"", "CreditNonQualifying", "InterestRate"}, 0.26},
        {{"", "CreditNonQualifying", "CreditQualifying"}, 0.19},
        {{"", "CreditNonQualifying", "Equity"}, 0.19},
        {{"", "CreditNonQualifying", "Commodity"}, 0.2},
        {{"", "CreditNonQualifying", "FX"}, 0.14},
        {{"", "Equity", "InterestRate"}, 0.29},
        {{"", "Equity", "CreditQualifying"}, 0.63},
        {{"", "Equity", "CreditNonQualifying"}, 0.19},
        {{"", "Equity", "Commodity"}, 0.43},
        {{"", "Equity", "FX"}, 0.25},
        {{"", "Commodity", "InterestRate"}, 0.31},
        {{"", "Commodity", "CreditQualifying"}, 0.42},
        {{"", "Commodity", "CreditNonQualifying"}, 0.2},
        {{"", "Commodity", "Equity"}, 0.43},
        {{"", "Commodity", "FX"}, 0.3},
        {{"", "FX", "InterestRate"}, 0.14},
        {{"", "FX", "CreditQualifying"}, 0.25},
        {{"", "FX", "CreditNonQualifying"}, 0.14},
        {{"", "FX", "Equity"}, 0.25},
        {{"", "FX", "Commodity"}, 0.3}
    };

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
    intraBucketCorrelation_[RiskType::IRCurve] = {
        {{"", "2w", "1m"}, 0.69},
        {{"", "2w", "3m"}, 0.64},
        {{"", "2w", "6m"}, 0.55},
        {{"", "2w", "1y"}, 0.34},
        {{"", "2w", "2y"}, 0.21},
        {{"", "2w", "3y"}, 0.16},
        {{"", "2w", "5y"}, 0.11},
        {{"", "2w", "10y"}, 0.06},
        {{"", "2w", "15y"}, 0.01},
        {{"", "2w", "20y"}, 0.0},
        {{"", "2w", "30y"}, -0.02},
        {{"", "1m", "2w"}, 0.69},
        {{"", "1m", "3m"}, 0.79},
        {{"", "1m", "6m"}, 0.65},
        {{"", "1m", "1y"}, 0.5},
        {{"", "1m", "2y"}, 0.39},
        {{"", "1m", "3y"}, 0.33},
        {{"", "1m", "5y"}, 0.26},
        {{"", "1m", "10y"}, 0.2},
        {{"", "1m", "15y"}, 0.14},
        {{"", "1m", "20y"}, 0.11},
        {{"", "1m", "30y"}, 0.09},
        {{"", "3m", "2w"}, 0.64},
        {{"", "3m", "1m"}, 0.79},
        {{"", "3m", "6m"}, 0.85},
        {{"", "3m", "1y"}, 0.67},
        {{"", "3m", "2y"}, 0.53},
        {{"", "3m", "3y"}, 0.46},
        {{"", "3m", "5y"}, 0.37},
        {{"", "3m", "10y"}, 0.29},
        {{"", "3m", "15y"}, 0.22},
        {{"", "3m", "20y"}, 0.19},
        {{"", "3m", "30y"}, 0.16},
        {{"", "6m", "2w"}, 0.55},
        {{"", "6m", "1m"}, 0.65},
        {{"", "6m", "3m"}, 0.85},
        {{"", "6m", "1y"}, 0.85},
        {{"", "6m", "2y"}, 0.72},
        {{"", "6m", "3y"}, 0.65},
        {{"", "6m", "5y"}, 0.56},
        {{"", "6m", "10y"}, 0.46},
        {{"", "6m", "15y"}, 0.38},
        {{"", "6m", "20y"}, 0.34},
        {{"", "6m", "30y"}, 0.3},
        {{"", "1y", "2w"}, 0.34},
        {{"", "1y", "1m"}, 0.5},
        {{"", "1y", "3m"}, 0.67},
        {{"", "1y", "6m"}, 0.85},
        {{"", "1y", "2y"}, 0.93},
        {{"", "1y", "3y"}, 0.88},
        {{"", "1y", "5y"}, 0.78},
        {{"", "1y", "10y"}, 0.66},
        {{"", "1y", "15y"}, 0.6},
        {{"", "1y", "20y"}, 0.56},
        {{"", "1y", "30y"}, 0.52},
        {{"", "2y", "2w"}, 0.21},
        {{"", "2y", "1m"}, 0.39},
        {{"", "2y", "3m"}, 0.53},
        {{"", "2y", "6m"}, 0.72},
        {{"", "2y", "1y"}, 0.93},
        {{"", "2y", "3y"}, 0.98},
        {{"", "2y", "5y"}, 0.91},
        {{"", "2y", "10y"}, 0.81},
        {{"", "2y", "15y"}, 0.75},
        {{"", "2y", "20y"}, 0.72},
        {{"", "2y", "30y"}, 0.68},
        {{"", "3y", "2w"}, 0.16},
        {{"", "3y", "1m"}, 0.33},
        {{"", "3y", "3m"}, 0.46},
        {{"", "3y", "6m"}, 0.65},
        {{"", "3y", "1y"}, 0.88},
        {{"", "3y", "2y"}, 0.98},
        {{"", "3y", "5y"}, 0.96},
        {{"", "3y", "10y"}, 0.87},
        {{"", "3y", "15y"}, 0.83},
        {{"", "3y", "20y"}, 0.8},
        {{"", "3y", "30y"}, 0.76},
        {{"", "5y", "2w"}, 0.11},
        {{"", "5y", "1m"}, 0.26},
        {{"", "5y", "3m"}, 0.37},
        {{"", "5y", "6m"}, 0.56},
        {{"", "5y", "1y"}, 0.78},
        {{"", "5y", "2y"}, 0.91},
        {{"", "5y", "3y"}, 0.96},
        {{"", "5y", "10y"}, 0.95},
        {{"", "5y", "15y"}, 0.92},
        {{"", "5y", "20y"}, 0.89},
        {{"", "5y", "30y"}, 0.86},
        {{"", "10y", "2w"}, 0.06},
        {{"", "10y", "1m"}, 0.2},
        {{"", "10y", "3m"}, 0.29},
        {{"", "10y", "6m"}, 0.46},
        {{"", "10y", "1y"}, 0.66},
        {{"", "10y", "2y"}, 0.81},
        {{"", "10y", "3y"}, 0.87},
        {{"", "10y", "5y"}, 0.95},
        {{"", "10y", "15y"}, 0.98},
        {{"", "10y", "20y"}, 0.97},
        {{"", "10y", "30y"}, 0.95},
        {{"", "15y", "2w"}, 0.01},
        {{"", "15y", "1m"}, 0.14},
        {{"", "15y", "3m"}, 0.22},
        {{"", "15y", "6m"}, 0.38},
        {{"", "15y", "1y"}, 0.6},
        {{"", "15y", "2y"}, 0.75},
        {{"", "15y", "3y"}, 0.83},
        {{"", "15y", "5y"}, 0.92},
        {{"", "15y", "10y"}, 0.98},
        {{"", "15y", "20y"}, 0.99},
        {{"", "15y", "30y"}, 0.98},
        {{"", "20y", "2w"}, 0.0},
        {{"", "20y", "1m"}, 0.11},
        {{"", "20y", "3m"}, 0.19},
        {{"", "20y", "6m"}, 0.34},
        {{"", "20y", "1y"}, 0.56},
        {{"", "20y", "2y"}, 0.72},
        {{"", "20y", "3y"}, 0.8},
        {{"", "20y", "5y"}, 0.89},
        {{"", "20y", "10y"}, 0.97},
        {{"", "20y", "15y"}, 0.99},
        {{"", "20y", "30y"}, 0.99},
        {{"", "30y", "2w"}, -0.02},
        {{"", "30y", "1m"}, 0.09},
        {{"", "30y", "3m"}, 0.16},
        {{"", "30y", "6m"}, 0.3},
        {{"", "30y", "1y"}, 0.52},
        {{"", "30y", "2y"}, 0.68},
        {{"", "30y", "3y"}, 0.76},
        {{"", "30y", "5y"}, 0.86},
        {{"", "30y", "10y"}, 0.95},
        {{"", "30y", "15y"}, 0.98},
        {{"", "30y", "20y"}, 0.99}
    };

    // CreditQ inter-bucket correlations 
    interBucketCorrelation_[RiskType::CreditQ] = {
        {{"", "1", "2"}, 0.43},
        {{"", "1", "3"}, 0.42},
        {{"", "1", "4"}, 0.41},
        {{"", "1", "5"}, 0.43},
        {{"", "1", "6"}, 0.39},
        {{"", "1", "7"}, 0.4},
        {{"", "1", "8"}, 0.35},
        {{"", "1", "9"}, 0.37},
        {{"", "1", "10"}, 0.37},
        {{"", "1", "11"}, 0.37},
        {{"", "1", "12"}, 0.32},
        {{"", "2", "1"}, 0.43},
        {{"", "2", "3"}, 0.46},
        {{"", "2", "4"}, 0.46},
        {{"", "2", "5"}, 0.47},
        {{"", "2", "6"}, 0.44},
        {{"", "2", "7"}, 0.34},
        {{"", "2", "8"}, 0.39},
        {{"", "2", "9"}, 0.41},
        {{"", "2", "10"}, 0.41},
        {{"", "2", "11"}, 0.41},
        {{"", "2", "12"}, 0.35},
        {{"", "3", "1"}, 0.42},
        {{"", "3", "2"}, 0.46},
        {{"", "3", "4"}, 0.47},
        {{"", "3", "5"}, 0.48},
        {{"", "3", "6"}, 0.45},
        {{"", "3", "7"}, 0.34},
        {{"", "3", "8"}, 0.38},
        {{"", "3", "9"}, 0.42},
        {{"", "3", "10"}, 0.42},
        {{"", "3", "11"}, 0.41},
        {{"", "3", "12"}, 0.36},
        {{"", "4", "1"}, 0.41},
        {{"", "4", "2"}, 0.46},
        {{"", "4", "3"}, 0.47},
        {{"", "4", "5"}, 0.48},
        {{"", "4", "6"}, 0.45},
        {{"", "4", "7"}, 0.34},
        {{"", "4", "8"}, 0.37},
        {{"", "4", "9"}, 0.4},
        {{"", "4", "10"}, 0.42},
        {{"", "4", "11"}, 0.41},
        {{"", "4", "12"}, 0.35},
        {{"", "5", "1"}, 0.43},
        {{"", "5", "2"}, 0.47},
        {{"", "5", "3"}, 0.48},
        {{"", "5", "4"}, 0.48},
        {{"", "5", "6"}, 0.47},
        {{"", "5", "7"}, 0.34},
        {{"", "5", "8"}, 0.37},
        {{"", "5", "9"}, 0.4},
        {{"", "5", "10"}, 0.42},
        {{"", "5", "11"}, 0.43},
        {{"", "5", "12"}, 0.36},
        {{"", "6", "1"}, 0.39},
        {{"", "6", "2"}, 0.44},
        {{"", "6", "3"}, 0.45},
        {{"", "6", "4"}, 0.45},
        {{"", "6", "5"}, 0.47},
        {{"", "6", "7"}, 0.32},
        {{"", "6", "8"}, 0.35},
        {{"", "6", "9"}, 0.38},
        {{"", "6", "10"}, 0.39},
        {{"", "6", "11"}, 0.39},
        {{"", "6", "12"}, 0.34},
        {{"", "7", "1"}, 0.4},
        {{"", "7", "2"}, 0.34},
        {{"", "7", "3"}, 0.34},
        {{"", "7", "4"}, 0.34},
        {{"", "7", "5"}, 0.34},
        {{"", "7", "6"}, 0.32},
        {{"", "7", "8"}, 0.26},
        {{"", "7", "9"}, 0.31},
        {{"", "7", "10"}, 0.3},
        {{"", "7", "11"}, 0.29},
        {{"", "7", "12"}, 0.26},
        {{"", "8", "1"}, 0.35},
        {{"", "8", "2"}, 0.39},
        {{"", "8", "3"}, 0.38},
        {{"", "8", "4"}, 0.37},
        {{"", "8", "5"}, 0.37},
        {{"", "8", "6"}, 0.35},
        {{"", "8", "7"}, 0.26},
        {{"", "8", "9"}, 0.34},
        {{"", "8", "10"}, 0.33},
        {{"", "8", "11"}, 0.33},
        {{"", "8", "12"}, 0.3},
        {{"", "9", "1"}, 0.37},
        {{"", "9", "2"}, 0.41},
        {{"", "9", "3"}, 0.42},
        {{"", "9", "4"}, 0.4},
        {{"", "9", "5"}, 0.4},
        {{"", "9", "6"}, 0.38},
        {{"", "9", "7"}, 0.31},
        {{"", "9", "8"}, 0.34},
        {{"", "9", "10"}, 0.37},
        {{"", "9", "11"}, 0.36},
        {{"", "9", "12"}, 0.33},
        {{"", "10", "1"}, 0.37},
        {{"", "10", "2"}, 0.41},
        {{"", "10", "3"}, 0.42},
        {{"", "10", "4"}, 0.42},
        {{"", "10", "5"}, 0.42},
        {{"", "10", "6"}, 0.39},
        {{"", "10", "7"}, 0.3},
        {{"", "10", "8"}, 0.33},
        {{"", "10", "9"}, 0.37},
        {{"", "10", "11"}, 0.38},
        {{"", "10", "12"}, 0.32},
        {{"", "11", "1"}, 0.37},
        {{"", "11", "2"}, 0.41},
        {{"", "11", "3"}, 0.41},
        {{"", "11", "4"}, 0.41},
        {{"", "11", "5"}, 0.43},
        {{"", "11", "6"}, 0.39},
        {{"", "11", "7"}, 0.29},
        {{"", "11", "8"}, 0.33},
        {{"", "11", "9"}, 0.36},
        {{"", "11", "10"}, 0.38},
        {{"", "11", "12"}, 0.32},
        {{"", "12", "1"}, 0.32},
        {{"", "12", "2"}, 0.35},
        {{"", "12", "3"}, 0.36},
        {{"", "12", "4"}, 0.35},
        {{"", "12", "5"}, 0.36},
        {{"", "12", "6"}, 0.34},
        {{"", "12", "7"}, 0.26},
        {{"", "12", "8"}, 0.3},
        {{"", "12", "9"}, 0.33},
        {{"", "12", "10"}, 0.32},
        {{"", "12", "11"}, 0.32}
    };

    // Equity inter-bucket correlations
    interBucketCorrelation_[RiskType::Equity] = {
        {{"", "1", "2"}, 0.17},
        {{"", "1", "3"}, 0.18},
        {{"", "1", "4"}, 0.19},
        {{"", "1", "5"}, 0.12},
        {{"", "1", "6"}, 0.14},
        {{"", "1", "7"}, 0.15},
        {{"", "1", "8"}, 0.15},
        {{"", "1", "9"}, 0.15},
        {{"", "1", "10"}, 0.12},
        {{"", "1", "11"}, 0.18},
        {{"", "1", "12"}, 0.18},
        {{"", "2", "1"}, 0.17},
        {{"", "2", "3"}, 0.22},
        {{"", "2", "4"}, 0.22},
        {{"", "2", "5"}, 0.14},
        {{"", "2", "6"}, 0.17},
        {{"", "2", "7"}, 0.18},
        {{"", "2", "8"}, 0.18},
        {{"", "2", "9"}, 0.18},
        {{"", "2", "10"}, 0.14},
        {{"", "2", "11"}, 0.22},
        {{"", "2", "12"}, 0.22},
        {{"", "3", "1"}, 0.18},
        {{"", "3", "2"}, 0.22},
        {{"", "3", "4"}, 0.24},
        {{"", "3", "5"}, 0.14},
        {{"", "3", "6"}, 0.2},
        {{"", "3", "7"}, 0.23},
        {{"", "3", "8"}, 0.2},
        {{"", "3", "9"}, 0.19},
        {{"", "3", "10"}, 0.16},
        {{"", "3", "11"}, 0.25},
        {{"", "3", "12"}, 0.25},
        {{"", "4", "1"}, 0.19},
        {{"", "4", "2"}, 0.22},
        {{"", "4", "3"}, 0.24},
        {{"", "4", "5"}, 0.16},
        {{"", "4", "6"}, 0.21},
        {{"", "4", "7"}, 0.21},
        {{"", "4", "8"}, 0.22},
        {{"", "4", "9"}, 0.19},
        {{"", "4", "10"}, 0.16},
        {{"", "4", "11"}, 0.25},
        {{"", "4", "12"}, 0.25},
        {{"", "5", "1"}, 0.12},
        {{"", "5", "2"}, 0.14},
        {{"", "5", "3"}, 0.14},
        {{"", "5", "4"}, 0.16},
        {{"", "5", "6"}, 0.23},
        {{"", "5", "7"}, 0.21},
        {{"", "5", "8"}, 0.24},
        {{"", "5", "9"}, 0.13},
        {{"", "5", "10"}, 0.17},
        {{"", "5", "11"}, 0.26},
        {{"", "5", "12"}, 0.26},
        {{"", "6", "1"}, 0.14},
        {{"", "6", "2"}, 0.17},
        {{"", "6", "3"}, 0.2},
        {{"", "6", "4"}, 0.21},
        {{"", "6", "5"}, 0.23},
        {{"", "6", "7"}, 0.29},
        {{"", "6", "8"}, 0.3},
        {{"", "6", "9"}, 0.16},
        {{"", "6", "10"}, 0.22},
        {{"", "6", "11"}, 0.32},
        {{"", "6", "12"}, 0.32},
        {{"", "7", "1"}, 0.15},
        {{"", "7", "2"}, 0.18},
        {{"", "7", "3"}, 0.23},
        {{"", "7", "4"}, 0.21},
        {{"", "7", "5"}, 0.21},
        {{"", "7", "6"}, 0.29},
        {{"", "7", "8"}, 0.29},
        {{"", "7", "9"}, 0.16},
        {{"", "7", "10"}, 0.22},
        {{"", "7", "11"}, 0.33},
        {{"", "7", "12"}, 0.33},
        {{"", "8", "1"}, 0.15},
        {{"", "8", "2"}, 0.18},
        {{"", "8", "3"}, 0.2},
        {{"", "8", "4"}, 0.22},
        {{"", "8", "5"}, 0.24},
        {{"", "8", "6"}, 0.3},
        {{"", "8", "7"}, 0.29},
        {{"", "8", "9"}, 0.17},
        {{"", "8", "10"}, 0.22},
        {{"", "8", "11"}, 0.33},
        {{"", "8", "12"}, 0.33},
        {{"", "9", "1"}, 0.15},
        {{"", "9", "2"}, 0.18},
        {{"", "9", "3"}, 0.19},
        {{"", "9", "4"}, 0.19},
        {{"", "9", "5"}, 0.13},
        {{"", "9", "6"}, 0.16},
        {{"", "9", "7"}, 0.16},
        {{"", "9", "8"}, 0.17},
        {{"", "9", "10"}, 0.13},
        {{"", "9", "11"}, 0.2},
        {{"", "9", "12"}, 0.2},
        {{"", "10", "1"}, 0.12},
        {{"", "10", "2"}, 0.14},
        {{"", "10", "3"}, 0.16},
        {{"", "10", "4"}, 0.16},
        {{"", "10", "5"}, 0.17},
        {{"", "10", "6"}, 0.22},
        {{"", "10", "7"}, 0.22},
        {{"", "10", "8"}, 0.22},
        {{"", "10", "9"}, 0.13},
        {{"", "10", "11"}, 0.23},
        {{"", "10", "12"}, 0.23},
        {{"", "11", "1"}, 0.18},
        {{"", "11", "2"}, 0.22},
        {{"", "11", "3"}, 0.25},
        {{"", "11", "4"}, 0.25},
        {{"", "11", "5"}, 0.26},
        {{"", "11", "6"}, 0.32},
        {{"", "11", "7"}, 0.33},
        {{"", "11", "8"}, 0.33},
        {{"", "11", "9"}, 0.2},
        {{"", "11", "10"}, 0.23},
        {{"", "11", "12"}, 0.4},
        {{"", "12", "1"}, 0.18},
        {{"", "12", "2"}, 0.22},
        {{"", "12", "3"}, 0.25},
        {{"", "12", "4"}, 0.25},
        {{"", "12", "5"}, 0.26},
        {{"", "12", "6"}, 0.32},
        {{"", "12", "7"}, 0.33},
        {{"", "12", "8"}, 0.33},
        {{"", "12", "9"}, 0.2},
        {{"", "12", "10"}, 0.23},
        {{"", "12", "11"}, 0.4}
    };

    // Commodity inter-bucket correlations
    interBucketCorrelation_[RiskType::Commodity] = {
        {{"", "1", "2"}, 0.15},
        {{"", "1", "3"}, 0.13},
        {{"", "1", "4"}, 0.17},
        {{"", "1", "5"}, 0.16},
        {{"", "1", "6"}, 0.02},
        {{"", "1", "7"}, 0.19},
        {{"", "1", "8"}, -0.02},
        {{"", "1", "9"}, 0.19},
        {{"", "1", "10"}, 0.01},
        {{"", "1", "11"}, 0.12},
        {{"", "1", "12"}, 0.12},
        {{"", "1", "13"}, -0.01},
        {{"", "1", "14"}, 0.11},
        {{"", "1", "15"}, 0.02},
        {{"", "1", "16"}, 0.0},
        {{"", "1", "17"}, 0.09},
        {{"", "2", "1"}, 0.15},
        {{"", "2", "3"}, 0.93},
        {{"", "2", "4"}, 0.95},
        {{"", "2", "5"}, 0.93},
        {{"", "2", "6"}, 0.23},
        {{"", "2", "7"}, 0.2},
        {{"", "2", "8"}, 0.2},
        {{"", "2", "9"}, 0.2},
        {{"", "2", "10"}, 0.06},
        {{"", "2", "11"}, 0.36},
        {{"", "2", "12"}, 0.23},
        {{"", "2", "13"}, 0.3},
        {{"", "2", "14"}, 0.28},
        {{"", "2", "15"}, 0.1},
        {{"", "2", "16"}, 0.0},
        {{"", "2", "17"}, 0.63},
        {{"", "3", "1"}, 0.13},
        {{"", "3", "2"}, 0.93},
        {{"", "3", "4"}, 0.9},
        {{"", "3", "5"}, 0.89},
        {{"", "3", "6"}, 0.15},
        {{"", "3", "7"}, 0.18},
        {{"", "3", "8"}, 0.13},
        {{"", "3", "9"}, 0.14},
        {{"", "3", "10"}, 0.02},
        {{"", "3", "11"}, 0.22},
        {{"", "3", "12"}, 0.08},
        {{"", "3", "13"}, 0.21},
        {{"", "3", "14"}, 0.13},
        {{"", "3", "15"}, 0.02},
        {{"", "3", "16"}, 0.0},
        {{"", "3", "17"}, 0.58},
        {{"", "4", "1"}, 0.17},
        {{"", "4", "2"}, 0.95},
        {{"", "4", "3"}, 0.9},
        {{"", "4", "5"}, 0.9},
        {{"", "4", "6"}, 0.18},
        {{"", "4", "7"}, 0.22},
        {{"", "4", "8"}, 0.16},
        {{"", "4", "9"}, 0.2},
        {{"", "4", "10"}, 0.0},
        {{"", "4", "11"}, 0.35},
        {{"", "4", "12"}, 0.18},
        {{"", "4", "13"}, 0.27},
        {{"", "4", "14"}, 0.25},
        {{"", "4", "15"}, 0.05},
        {{"", "4", "16"}, 0.0},
        {{"", "4", "17"}, 0.6},
        {{"", "5", "1"}, 0.16},
        {{"", "5", "2"}, 0.93},
        {{"", "5", "3"}, 0.89},
        {{"", "5", "4"}, 0.9},
        {{"", "5", "6"}, 0.23},
        {{"", "5", "7"}, 0.21},
        {{"", "5", "8"}, 0.2},
        {{"", "5", "9"}, 0.19},
        {{"", "5", "10"}, 0.09},
        {{"", "5", "11"}, 0.38},
        {{"", "5", "12"}, 0.25},
        {{"", "5", "13"}, 0.33},
        {{"", "5", "14"}, 0.27},
        {{"", "5", "15"}, 0.09},
        {{"", "5", "16"}, 0.0},
        {{"", "5", "17"}, 0.62},
        {{"", "6", "1"}, 0.02},
        {{"", "6", "2"}, 0.23},
        {{"", "6", "3"}, 0.15},
        {{"", "6", "4"}, 0.18},
        {{"", "6", "5"}, 0.23},
        {{"", "6", "7"}, 0.18},
        {{"", "6", "8"}, 0.63},
        {{"", "6", "9"}, 0.16},
        {{"", "6", "10"}, 0.02},
        {{"", "6", "11"}, 0.13},
        {{"", "6", "12"}, -0.02},
        {{"", "6", "13"}, 0.18},
        {{"", "6", "14"}, 0.1},
        {{"", "6", "15"}, -0.03},
        {{"", "6", "16"}, 0.0},
        {{"", "6", "17"}, 0.27},
        {{"", "7", "1"}, 0.19},
        {{"", "7", "2"}, 0.2},
        {{"", "7", "3"}, 0.18},
        {{"", "7", "4"}, 0.22},
        {{"", "7", "5"}, 0.21},
        {{"", "7", "6"}, 0.18},
        {{"", "7", "8"}, 0.08},
        {{"", "7", "9"}, 0.62},
        {{"", "7", "10"}, 0.11},
        {{"", "7", "11"}, 0.11},
        {{"", "7", "12"}, 0.01},
        {{"", "7", "13"}, 0.07},
        {{"", "7", "14"}, 0.1},
        {{"", "7", "15"}, 0.02},
        {{"", "7", "16"}, 0.0},
        {{"", "7", "17"}, 0.15},
        {{"", "8", "1"}, -0.02},
        {{"", "8", "2"}, 0.2},
        {{"", "8", "3"}, 0.13},
        {{"", "8", "4"}, 0.16},
        {{"", "8", "5"}, 0.2},
        {{"", "8", "6"}, 0.63},
        {{"", "8", "7"}, 0.08},
        {{"", "8", "9"}, 0.16},
        {{"", "8", "10"}, -0.04},
        {{"", "8", "11"}, 0.09},
        {{"", "8", "12"}, 0.02},
        {{"", "8", "13"}, 0.1},
        {{"", "8", "14"}, 0.06},
        {{"", "8", "15"}, 0.03},
        {{"", "8", "16"}, 0.0},
        {{"", "8", "17"}, 0.2},
        {{"", "9", "1"}, 0.19},
        {{"", "9", "2"}, 0.2},
        {{"", "9", "3"}, 0.14},
        {{"", "9", "4"}, 0.2},
        {{"", "9", "5"}, 0.19},
        {{"", "9", "6"}, 0.16},
        {{"", "9", "7"}, 0.62},
        {{"", "9", "8"}, 0.16},
        {{"", "9", "10"}, 0.01},
        {{"", "9", "11"}, 0.11},
        {{"", "9", "12"}, 0.0},
        {{"", "9", "13"}, 0.1},
        {{"", "9", "14"}, 0.09},
        {{"", "9", "15"}, 0.05},
        {{"", "9", "16"}, 0.0},
        {{"", "9", "17"}, 0.13},
        {{"", "10", "1"}, 0.01},
        {{"", "10", "2"}, 0.06},
        {{"", "10", "3"}, 0.02},
        {{"", "10", "4"}, 0.0},
        {{"", "10", "5"}, 0.09},
        {{"", "10", "6"}, 0.02},
        {{"", "10", "7"}, 0.11},
        {{"", "10", "8"}, -0.04},
        {{"", "10", "9"}, 0.01},
        {{"", "10", "11"}, 0.07},
        {{"", "10", "12"}, 0.05},
        {{"", "10", "13"}, 0.07},
        {{"", "10", "14"}, 0.07},
        {{"", "10", "15"}, 0.01},
        {{"", "10", "16"}, 0.0},
        {{"", "10", "17"}, 0.01},
        {{"", "11", "1"}, 0.12},
        {{"", "11", "2"}, 0.36},
        {{"", "11", "3"}, 0.22},
        {{"", "11", "4"}, 0.35},
        {{"", "11", "5"}, 0.38},
        {{"", "11", "6"}, 0.13},
        {{"", "11", "7"}, 0.11},
        {{"", "11", "8"}, 0.09},
        {{"", "11", "9"}, 0.11},
        {{"", "11", "10"}, 0.07},
        {{"", "11", "12"}, 0.35},
        {{"", "11", "13"}, 0.21},
        {{"", "11", "14"}, 0.2},
        {{"", "11", "15"}, 0.1},
        {{"", "11", "16"}, 0.0},
        {{"", "11", "17"}, 0.31},
        {{"", "12", "1"}, 0.12},
        {{"", "12", "2"}, 0.23},
        {{"", "12", "3"}, 0.08},
        {{"", "12", "4"}, 0.18},
        {{"", "12", "5"}, 0.25},
        {{"", "12", "6"}, -0.02},
        {{"", "12", "7"}, 0.01},
        {{"", "12", "8"}, 0.02},
        {{"", "12", "9"}, 0.0},
        {{"", "12", "10"}, 0.05},
        {{"", "12", "11"}, 0.35},
        {{"", "12", "13"}, 0.11},
        {{"", "12", "14"}, 0.24},
        {{"", "12", "15"}, 0.05},
        {{"", "12", "16"}, 0.0},
        {{"", "12", "17"}, 0.16},
        {{"", "13", "1"}, -0.01},
        {{"", "13", "2"}, 0.3},
        {{"", "13", "3"}, 0.21},
        {{"", "13", "4"}, 0.27},
        {{"", "13", "5"}, 0.33},
        {{"", "13", "6"}, 0.18},
        {{"", "13", "7"}, 0.07},
        {{"", "13", "8"}, 0.1},
        {{"", "13", "9"}, 0.1},
        {{"", "13", "10"}, 0.07},
        {{"", "13", "11"}, 0.21},
        {{"", "13", "12"}, 0.11},
        {{"", "13", "14"}, 0.27},
        {{"", "13", "15"}, 0.13},
        {{"", "13", "16"}, 0.0},
        {{"", "13", "17"}, 0.35},
        {{"", "14", "1"}, 0.11},
        {{"", "14", "2"}, 0.28},
        {{"", "14", "3"}, 0.13},
        {{"", "14", "4"}, 0.25},
        {{"", "14", "5"}, 0.27},
        {{"", "14", "6"}, 0.1},
        {{"", "14", "7"}, 0.1},
        {{"", "14", "8"}, 0.06},
        {{"", "14", "9"}, 0.09},
        {{"", "14", "10"}, 0.07},
        {{"", "14", "11"}, 0.2},
        {{"", "14", "12"}, 0.24},
        {{"", "14", "13"}, 0.27},
        {{"", "14", "15"}, 0.08},
        {{"", "14", "16"}, 0.0},
        {{"", "14", "17"}, 0.21},
        {{"", "15", "1"}, 0.02},
        {{"", "15", "2"}, 0.1},
        {{"", "15", "3"}, 0.02},
        {{"", "15", "4"}, 0.05},
        {{"", "15", "5"}, 0.09},
        {{"", "15", "6"}, -0.03},
        {{"", "15", "7"}, 0.02},
        {{"", "15", "8"}, 0.03},
        {{"", "15", "9"}, 0.05},
        {{"", "15", "10"}, 0.01},
        {{"", "15", "11"}, 0.1},
        {{"", "15", "12"}, 0.05},
        {{"", "15", "13"}, 0.13},
        {{"", "15", "14"}, 0.08},
        {{"", "15", "16"}, 0.0},
        {{"", "15", "17"}, 0.12},
        {{"", "16", "1"}, 0.0},
        {{"", "16", "2"}, 0.0},
        {{"", "16", "3"}, 0.0},
        {{"", "16", "4"}, 0.0},
        {{"", "16", "5"}, 0.0},
        {{"", "16", "6"}, 0.0},
        {{"", "16", "7"}, 0.0},
        {{"", "16", "8"}, 0.0},
        {{"", "16", "9"}, 0.0},
        {{"", "16", "10"}, 0.0},
        {{"", "16", "11"}, 0.0},
        {{"", "16", "12"}, 0.0},
        {{"", "16", "13"}, 0.0},
        {{"", "16", "14"}, 0.0},
        {{"", "16", "15"}, 0.0},
        {{"", "16", "17"}, 0.0},
        {{"", "17", "1"}, 0.09},
        {{"", "17", "2"}, 0.63},
        {{"", "17", "3"}, 0.58},
        {{"", "17", "4"}, 0.6},
        {{"", "17", "5"}, 0.62},
        {{"", "17", "6"}, 0.27},
        {{"", "17", "7"}, 0.15},
        {{"", "17", "8"}, 0.2},
        {{"", "17", "9"}, 0.13},
        {{"", "17", "10"}, 0.01},
        {{"", "17", "11"}, 0.31},
        {{"", "17", "12"}, 0.16},
        {{"", "17", "13"}, 0.35},
        {{"", "17", "14"}, 0.21},
        {{"", "17", "15"}, 0.12},
        {{"", "17", "16"}, 0.0}
    };

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[RiskType::Equity] = {
        {{"1", "", ""}, 0.16},
        {{"2", "", ""}, 0.21},
        {{"3", "", ""}, 0.28},
        {{"4", "", ""}, 0.25},
        {{"5", "", ""}, 0.20},
        {{"6", "", ""}, 0.30},
        {{"7", "", ""}, 0.33},
        {{"8", "", ""}, 0.31},
        {{"9", "", ""}, 0.18},
        {{"10", "", ""}, 0.18},
        {{"11", "", ""}, 0.40},
        {{"12", "", ""}, 0.40}
    };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[RiskType::Commodity] = {
        {{"1", "", ""}, 0.28},
        {{"2", "", ""}, 0.98},
        {{"3", "", ""}, 0.92},
        {{"4", "", ""}, 0.97},
        {{"5", "", ""}, 0.99},
        {{"6", "", ""}, 0.74},
        {{"7", "", ""}, 0.87},
        {{"8", "", ""}, 0.35},
        {{"9", "", ""}, 0.69}, 
        {{"10", "", ""}, 0.14},
        {{"11", "", ""}, 0.53},
        {{"12", "", ""}, 0.63},
        {{"13", "", ""}, 0.61},
        {{"14", "", ""}, 0.18},
        {{"15", "", ""}, 0.15},
        {{"16", "", ""}, 0.00},
        {{"17", "", ""}, 0.36}
    };

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

    // clang-format ond
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

string SimmConfiguration_ISDA_V2_2::label2(const QuantLib::ext::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::label2(irIndex);
}

} // namespace analytics
} // namespace ore
