/*
 Copyright (C) 2021 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav2_3_8.hpp>
#include <orea/simm/simmconfigurationisdav2_3_8.hpp>
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

QuantLib::Size SimmConfiguration_ISDA_V2_3_8::group(const string& qualifier, const std::map<QuantLib::Size,
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

QuantLib::Real SimmConfiguration_ISDA_V2_3_8::weight(const CrifRecord::RiskType& rt, boost::optional<string> qualifier,
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

QuantLib::Real SimmConfiguration_ISDA_V2_3_8::correlation(const CrifRecord::RiskType& firstRt, const string& firstQualifier,
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

SimmConfiguration_ISDA_V2_3_8::SimmConfiguration_ISDA_V2_3_8(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                             const QuantLib::Size& mporDays, const std::string& name,
                                                             const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentration_ISDA_V2_3_8>(simmBucketMapper_);
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
    // The groups consists of High Vol Currencies (ARS, BRL, MXN, TRY, ZAR) & regular vol currencies (all others)
    ccyGroups_ = {
        { 1, { "ARS", "BRL", "MXN", "TRY", "ZAR" } },
        { 0, { } },
    };

    vector<Real> temp;

    if (mporDays_ == 10) {
        // Risk weights
        temp = {
            7.3,  13.0, 
            13.0, 10.2
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { CrifRecord::RiskType::Inflation, 64 },
            { CrifRecord::RiskType::XCcyBasis, 21 },
            { CrifRecord::RiskType::IRVol, 0.18 },
            { CrifRecord::RiskType::InflationVol, 0.18 },   
            { CrifRecord::RiskType::CreditVol, 0.73 },
            { CrifRecord::RiskType::CreditVolNonQ, 0.73 },
            { CrifRecord::RiskType::CommodityVol, 0.61 },
            { CrifRecord::RiskType::FXVol, 0.47 },
            { CrifRecord::RiskType::BaseCorr, 11.0 }
        };

        rwBucket_ = {
            {CrifRecord::RiskType::CreditQ,
             {{{"1", "", ""}, 81.0},
              {{"2", "", ""}, 96.0},
              {{"3", "", ""}, 86.0},
              {{"4", "", ""}, 53.0},
              {{"5", "", ""}, 59.0},
              {{"6", "", ""}, 47.0},
              {{"7", "", ""}, 181.0},
              {{"8", "", ""}, 452.0},
              {{"9", "", ""}, 252.0},
              {{"10", "", ""}, 261.0},
              {{"11", "", ""}, 218.0},
              {{"12", "", ""}, 195.0},
              {{"Residual", "", ""}, 452.0}}},
            {CrifRecord::RiskType::CreditNonQ,
             {{{"1", "", ""}, 280.0},
              {{"2", "", ""}, 1200.0},
              {{"Residual", "", ""}, 1200.0}}},
            {CrifRecord::RiskType::Equity,
             {{{"1", "", ""}, 25.0},
              {{"2", "", ""}, 28.0},
              {{"3", "", ""}, 30.0},
              {{"4", "", ""}, 28.0},
              {{"5", "", ""}, 23.0},
              {{"6", "", ""}, 24.0},
              {{"7", "", ""}, 29.0},
              {{"8", "", ""}, 27.0},
              {{"9", "", ""}, 31.0},
              {{"10", "", ""}, 33.0},
              {{"11", "", ""}, 19.0},
              {{"12", "", ""}, 19.0},
              {{"Residual", "", ""}, 33.0}}},
            {CrifRecord::RiskType::Commodity,
             {{{"1", "", ""}, 22.0},
              {{"2", "", ""}, 29.0},
              {{"3", "", ""}, 33.0},
              {{"4", "", ""}, 25.0},
              {{"5", "", ""}, 35.0},
              {{"6", "", ""}, 24.0},
              {{"7", "", ""}, 22.0},
              {{"8", "", ""}, 49.0},
              {{"9", "", ""}, 24.0},
              {{"10", "", ""}, 53.0},
              {{"11", "", ""}, 20.0},
              {{"12", "", ""}, 21.0},
              {{"13", "", ""}, 13.0},
              {{"14", "", ""}, 15.0},
              {{"15", "", ""}, 13.0},
              {{"16", "", ""}, 53.0},
              {{"17", "", ""}, 17.0}}},
            {CrifRecord::RiskType::EquityVol,
             {{{"1", "", ""}, 0.50},
              {{"2", "", ""}, 0.50},
              {{"3", "", ""}, 0.50},
              {{"4", "", ""}, 0.50},
              {{"5", "", ""}, 0.50},
              {{"6", "", ""}, 0.50},
              {{"7", "", ""}, 0.50},
              {{"8", "", ""}, 0.50},
              {{"9", "", ""}, 0.50},
              {{"10", "", ""}, 0.50},
              {{"11", "", ""}, 0.50},
              {{"12", "", ""}, 0.98},
              {{"Residual", "", ""}, 0.50}}}, 
        };
        
        rwLabel_1_ = {
            {CrifRecord::RiskType::IRCurve,
                {{{"1", "2w", ""}, 114.0},
                 {{"1", "1m", ""}, 106.0},
                 {{"1", "3m", ""}, 95.0},
                 {{"1", "6m", ""}, 74.0},
                 {{"1", "1y", ""}, 66.0},
                 {{"1", "2y", ""}, 61.0},
                 {{"1", "3y", ""}, 56.0},
                 {{"1", "5y", ""}, 52.0},
                 {{"1", "10y", ""}, 53.0},
                 {{"1", "15y", ""}, 57.0},
                 {{"1", "20y", ""}, 60.0},
                 {{"1", "30y", ""}, 66.0},
                 {{"2", "2w", ""}, 15.0},
                 {{"2", "1m", ""}, 18.0},
                 {{"2", "3m", ""}, 8.6},
                 {{"2", "6m", ""}, 11.0},
                 {{"2", "1y", ""}, 13.0},
                 {{"2", "2y", ""}, 15.0},
                 {{"2", "3y", ""}, 18.0},
                 {{"2", "5y", ""}, 20.0},
                 {{"2", "10y", ""}, 19.0},
                 {{"2", "15y", ""}, 19.0},
                 {{"2", "20y", ""}, 20.0},
                 {{"2", "30y", ""}, 23.0},
                 {{"3", "2w", ""}, 101.0},
                 {{"3", "1m", ""}, 91.0},
                 {{"3", "3m", ""}, 78.0},
                 {{"3", "6m", ""}, 80.0},
                 {{"3", "1y", ""}, 90.0},
                 {{"3", "2y", ""}, 89.0},
                 {{"3", "3y", ""}, 94.0},
                 {{"3", "5y", ""}, 94.0},
                 {{"3", "10y", ""}, 92.0},
                 {{"3", "15y", ""}, 101.0},
                 {{"3", "20y", ""}, 104.0},
                 {{"3", "30y", ""}, 102.0}}
            }
        };
        
        // Historical volatility ratios 
        historicalVolatilityRatios_[CrifRecord::RiskType::EquityVol] = 0.54;
        historicalVolatilityRatios_[CrifRecord::RiskType::CommodityVol] = 0.64;
        historicalVolatilityRatios_[CrifRecord::RiskType::FXVol] = 0.55;
        hvr_ir_ = 0.44;
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
            1.8,  3.0, 
            3.0,  2.9
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { CrifRecord::RiskType::Inflation, 15 },
            { CrifRecord::RiskType::XCcyBasis, 5.6 },
            { CrifRecord::RiskType::IRVol, 0.046 },
            { CrifRecord::RiskType::InflationVol, 0.046 },
            { CrifRecord::RiskType::CreditVol, 0.1 },
            { CrifRecord::RiskType::CreditVolNonQ, 0.1 },
            { CrifRecord::RiskType::CommodityVol, 0.13 },
            { CrifRecord::RiskType::FXVol, 0.099 },
            { CrifRecord::RiskType::BaseCorr, 2.7 }
        };

        rwBucket_ = {
            {CrifRecord::RiskType::CreditQ,
             {{{"1", "", ""}, 23.0},
              {{"2", "", ""}, 27.0},
              {{"3", "", ""}, 18.0},
              {{"4", "", ""}, 12.0},
              {{"5", "", ""}, 13.0},
              {{"6", "", ""}, 12.0},
              {{"7", "", ""}, 49.0},
              {{"8", "", ""}, 92.0},
              {{"9", "", ""}, 48.0},
              {{"10", "", ""}, 59.0},
              {{"11", "", ""}, 41.0},
              {{"12", "", ""}, 41.0},
              {{"Residual", "", ""}, 92.0}}},
            {CrifRecord::RiskType::CreditNonQ, {{{"1", "", ""}, 74.0}, {{"2", "", ""}, 240.0}, {{"Residual", "", ""}, 240.0}}},
            {CrifRecord::RiskType::Equity,
             {{{"1", "", ""}, 8.3},
              {{"2", "", ""}, 9.1},
              {{"3", "", ""}, 9.8},
              {{"4", "", ""}, 9.0},
              {{"5", "", ""}, 7.7},
              {{"6", "", ""}, 8.4},
              {{"7", "", ""}, 9.3},
              {{"8", "", ""}, 9.4},
              {{"9", "", ""}, 9.9},
              {{"10", "", ""}, 11.0},
              {{"11", "", ""}, 6.0},
              {{"12", "", ""}, 6.0},
              {{"Residual", "", ""}, 11.0}}},
            {CrifRecord::RiskType::Commodity,
             {{{"1", "", ""}, 6.3},
              {{"2", "", ""}, 9.1},
              {{"3", "", ""}, 8.1},
              {{"4", "", ""}, 7.2},
              {{"5", "", ""}, 10.0},
              {{"6", "", ""}, 8.0},
              {{"7", "", ""}, 7.1},
              {{"8", "", ""}, 11.0},
              {{"9", "", ""}, 8.1},
              {{"10", "", ""}, 16.0},
              {{"11", "", ""}, 6.2},
              {{"12", "", ""}, 6.2},
              {{"13", "", ""}, 4.7},
              {{"14", "", ""}, 4.8},
              {{"15", "", ""}, 3.8},
              {{"16", "", ""}, 16.0},
              {{"17", "", ""}, 5.1}}},
            {CrifRecord::RiskType::EquityVol,
             {{{"1", "", ""}, 0.094},
              {{"2", "", ""}, 0.094},
              {{"3", "", ""}, 0.094},
              {{"4", "", ""}, 0.094},
              {{"5", "", ""}, 0.094},
              {{"6", "", ""}, 0.094},
              {{"7", "", ""}, 0.094},
              {{"8", "", ""}, 0.094},
              {{"9", "", ""}, 0.094},
              {{"10", "", ""}, 0.094},
              {{"11", "", ""}, 0.094},
              {{"12", "", ""}, 0.27},
              {{"Residual", "", ""}, 0.094}}},
        };
        
        rwLabel_1_ = {
            {CrifRecord::RiskType::IRCurve,
                {{{"1", "2w", ""}, 18.0},
                 {{"1", "1m", ""}, 15.0},
                 {{"1", "3m", ""}, 12.0},
                 {{"1", "6m", ""}, 12.0},
                 {{"1", "1y", ""}, 13.0},
                 {{"1", "2y", ""}, 15.0},
                 {{"1", "3y", ""}, 16.0},
                 {{"1", "5y", ""}, 16.0},
                 {{"1", "10y", ""}, 16.0},
                 {{"1", "15y", ""}, 17.0},
                 {{"1", "20y", ""}, 16.0},
                 {{"1", "30y", ""}, 17.0},
                 {{"2", "2w", ""}, 1.7},
                 {{"2", "1m", ""}, 3.4},
                 {{"2", "3m", ""}, 1.6},
                 {{"2", "6m", ""}, 2.0},
                 {{"2", "1y", ""}, 3.0},
                 {{"2", "2y", ""}, 4.8},
                 {{"2", "3y", ""}, 5.8},
                 {{"2", "5y", ""}, 6.8},
                 {{"2", "10y", ""}, 6.5},
                 {{"2", "15y", ""}, 7.0},
                 {{"2", "20y", ""}, 7.5},
                 {{"2", "30y", ""}, 8.3},
                 {{"3", "2w", ""}, 36.0},
                 {{"3", "1m", ""}, 27.0},
                 {{"3", "3m", ""}, 16.0},
                 {{"3", "6m", ""}, 19.0},
                 {{"3", "1y", ""}, 23.0},
                 {{"3", "2y", ""}, 23.0},
                 {{"3", "3y", ""}, 32.0},
                 {{"3", "5y", ""}, 31.0},
                 {{"3", "10y", ""}, 32.0},
                 {{"3", "15y", ""}, 30.0},
                 {{"3", "20y", ""}, 32.0},
                 {{"3", "30y", ""}, 27.0}}
            }
        };

        // Historical volatility ratios 
        historicalVolatilityRatios_[CrifRecord::RiskType::EquityVol] = 0.5;
        historicalVolatilityRatios_[CrifRecord::RiskType::CommodityVol] = 0.67;
        historicalVolatilityRatios_[CrifRecord::RiskType::FXVol] = 0.73;
        hvr_ir_ = 0.5;

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
        {{"", "InterestRate", "CreditQualifying"}, 0.32},
        {{"", "InterestRate", "CreditNonQualifying"}, 0.19},
        {{"", "InterestRate", "Equity"}, 0.33},
        {{"", "InterestRate", "Commodity"}, 0.41},
        {{"", "InterestRate", "FX"}, 0.28},
        {{"", "CreditQualifying", "InterestRate"}, 0.32},
        {{"", "CreditQualifying", "CreditNonQualifying"}, 0.45},
        {{"", "CreditQualifying", "Equity"}, 0.69},
        {{"", "CreditQualifying", "Commodity"}, 0.52},
        {{"", "CreditQualifying", "FX"}, 0.42},
        {{"", "CreditNonQualifying", "InterestRate"}, 0.19},
        {{"", "CreditNonQualifying", "CreditQualifying"}, 0.45},
        {{"", "CreditNonQualifying", "Equity"}, 0.48},
        {{"", "CreditNonQualifying", "Commodity"}, 0.4},
        {{"", "CreditNonQualifying", "FX"}, 0.14},
        {{"", "Equity", "InterestRate"}, 0.33},
        {{"", "Equity", "CreditQualifying"}, 0.69},
        {{"", "Equity", "CreditNonQualifying"}, 0.48},
        {{"", "Equity", "Commodity"}, 0.52},
        {{"", "Equity", "FX"}, 0.34},
        {{"", "Commodity", "InterestRate"}, 0.41},
        {{"", "Commodity", "CreditQualifying"}, 0.52},
        {{"", "Commodity", "CreditNonQualifying"}, 0.4},
        {{"", "Commodity", "Equity"}, 0.52},
        {{"", "Commodity", "FX"}, 0.38},
        {{"", "FX", "InterestRate"}, 0.28},
        {{"", "FX", "CreditQualifying"}, 0.42},
        {{"", "FX", "CreditNonQualifying"}, 0.14},
        {{"", "FX", "Equity"}, 0.34},
        {{"", "FX", "Commodity"}, 0.38}
    };

    // FX correlations
    temp = {
        0.500, 0.280,
        0.280, 0.690
    };
    fxRegVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    temp = {
        0.850, 0.390,
        0.390, 0.500
    };
    fxHighVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations) 
    intraBucketCorrelation_[CrifRecord::RiskType::IRCurve] = {
        {{"", "2w", "1m"}, 0.75},
        {{"", "2w", "3m"}, 0.63},
        {{"", "2w", "6m"}, 0.55},
        {{"", "2w", "1y"}, 0.44},
        {{"", "2w", "2y"}, 0.35},
        {{"", "2w", "3y"}, 0.31},
        {{"", "2w", "5y"}, 0.26},
        {{"", "2w", "10y"}, 0.21},
        {{"", "2w", "15y"}, 0.17},
        {{"", "2w", "20y"}, 0.15},
        {{"", "2w", "30y"}, 0.14},
        {{"", "1m", "2w"}, 0.75},
        {{"", "1m", "3m"}, 0.79},
        {{"", "1m", "6m"}, 0.68},
        {{"", "1m", "1y"}, 0.51},
        {{"", "1m", "2y"}, 0.4},
        {{"", "1m", "3y"}, 0.33},
        {{"", "1m", "5y"}, 0.28},
        {{"", "1m", "10y"}, 0.22},
        {{"", "1m", "15y"}, 0.17},
        {{"", "1m", "20y"}, 0.15},
        {{"", "1m", "30y"}, 0.15},
        {{"", "3m", "2w"}, 0.63},
        {{"", "3m", "1m"}, 0.79},
        {{"", "3m", "6m"}, 0.85},
        {{"", "3m", "1y"}, 0.67},
        {{"", "3m", "2y"}, 0.53},
        {{"", "3m", "3y"}, 0.45},
        {{"", "3m", "5y"}, 0.38},
        {{"", "3m", "10y"}, 0.31},
        {{"", "3m", "15y"}, 0.23},
        {{"", "3m", "20y"}, 0.21},
        {{"", "3m", "30y"}, 0.22},
        {{"", "6m", "2w"}, 0.55},
        {{"", "6m", "1m"}, 0.68},
        {{"", "6m", "3m"}, 0.85},
        {{"", "6m", "1y"}, 0.82},
        {{"", "6m", "2y"}, 0.7},
        {{"", "6m", "3y"}, 0.61},
        {{"", "6m", "5y"}, 0.53},
        {{"", "6m", "10y"}, 0.44},
        {{"", "6m", "15y"}, 0.36},
        {{"", "6m", "20y"}, 0.35},
        {{"", "6m", "30y"}, 0.33},
        {{"", "1y", "2w"}, 0.44},
        {{"", "1y", "1m"}, 0.51},
        {{"", "1y", "3m"}, 0.67},
        {{"", "1y", "6m"}, 0.82},
        {{"", "1y", "2y"}, 0.94},
        {{"", "1y", "3y"}, 0.86},
        {{"", "1y", "5y"}, 0.78},
        {{"", "1y", "10y"}, 0.66},
        {{"", "1y", "15y"}, 0.6},
        {{"", "1y", "20y"}, 0.58},
        {{"", "1y", "30y"}, 0.56},
        {{"", "2y", "2w"}, 0.35},
        {{"", "2y", "1m"}, 0.4},
        {{"", "2y", "3m"}, 0.53},
        {{"", "2y", "6m"}, 0.7},
        {{"", "2y", "1y"}, 0.94},
        {{"", "2y", "3y"}, 0.96},
        {{"", "2y", "5y"}, 0.9},
        {{"", "2y", "10y"}, 0.8},
        {{"", "2y", "15y"}, 0.75},
        {{"", "2y", "20y"}, 0.72},
        {{"", "2y", "30y"}, 0.71},
        {{"", "3y", "2w"}, 0.31},
        {{"", "3y", "1m"}, 0.33},
        {{"", "3y", "3m"}, 0.45},
        {{"", "3y", "6m"}, 0.61},
        {{"", "3y", "1y"}, 0.86},
        {{"", "3y", "2y"}, 0.96},
        {{"", "3y", "5y"}, 0.97},
        {{"", "3y", "10y"}, 0.88},
        {{"", "3y", "15y"}, 0.83},
        {{"", "3y", "20y"}, 0.8},
        {{"", "3y", "30y"}, 0.78},
        {{"", "5y", "2w"}, 0.26},
        {{"", "5y", "1m"}, 0.28},
        {{"", "5y", "3m"}, 0.38},
        {{"", "5y", "6m"}, 0.53},
        {{"", "5y", "1y"}, 0.78},
        {{"", "5y", "2y"}, 0.9},
        {{"", "5y", "3y"}, 0.97},
        {{"", "5y", "10y"}, 0.95},
        {{"", "5y", "15y"}, 0.91},
        {{"", "5y", "20y"}, 0.88},
        {{"", "5y", "30y"}, 0.87},
        {{"", "10y", "2w"}, 0.21},
        {{"", "10y", "1m"}, 0.22},
        {{"", "10y", "3m"}, 0.31},
        {{"", "10y", "6m"}, 0.44},
        {{"", "10y", "1y"}, 0.66},
        {{"", "10y", "2y"}, 0.8},
        {{"", "10y", "3y"}, 0.88},
        {{"", "10y", "5y"}, 0.95},
        {{"", "10y", "15y"}, 0.97},
        {{"", "10y", "20y"}, 0.95},
        {{"", "10y", "30y"}, 0.95},
        {{"", "15y", "2w"}, 0.17},
        {{"", "15y", "1m"}, 0.17},
        {{"", "15y", "3m"}, 0.23},
        {{"", "15y", "6m"}, 0.36},
        {{"", "15y", "1y"}, 0.6},
        {{"", "15y", "2y"}, 0.75},
        {{"", "15y", "3y"}, 0.83},
        {{"", "15y", "5y"}, 0.91},
        {{"", "15y", "10y"}, 0.97},
        {{"", "15y", "20y"}, 0.98},
        {{"", "15y", "30y"}, 0.98},
        {{"", "20y", "2w"}, 0.15},
        {{"", "20y", "1m"}, 0.15},
        {{"", "20y", "3m"}, 0.21},
        {{"", "20y", "6m"}, 0.35},
        {{"", "20y", "1y"}, 0.58},
        {{"", "20y", "2y"}, 0.72},
        {{"", "20y", "3y"}, 0.8},
        {{"", "20y", "5y"}, 0.88},
        {{"", "20y", "10y"}, 0.95},
        {{"", "20y", "15y"}, 0.98},
        {{"", "20y", "30y"}, 0.99},
        {{"", "30y", "2w"}, 0.14},
        {{"", "30y", "1m"}, 0.15},
        {{"", "30y", "3m"}, 0.22},
        {{"", "30y", "6m"}, 0.33},
        {{"", "30y", "1y"}, 0.56},
        {{"", "30y", "2y"}, 0.71},
        {{"", "30y", "3y"}, 0.78},
        {{"", "30y", "5y"}, 0.87},
        {{"", "30y", "10y"}, 0.95},
        {{"", "30y", "15y"}, 0.98},
        {{"", "30y", "20y"}, 0.99}
    };

    // CreditQ inter-bucket correlations 
    interBucketCorrelation_[CrifRecord::RiskType::CreditQ] = {
        {{"", "1", "2"}, 0.35},
        {{"", "1", "3"}, 0.37},
        {{"", "1", "4"}, 0.35},
        {{"", "1", "5"}, 0.37},
        {{"", "1", "6"}, 0.34},
        {{"", "1", "7"}, 0.38},
        {{"", "1", "8"}, 0.31},
        {{"", "1", "9"}, 0.34},
        {{"", "1", "10"}, 0.33},
        {{"", "1", "11"}, 0.3},
        {{"", "1", "12"}, 0.31},
        {{"", "2", "1"}, 0.35},
        {{"", "2", "3"}, 0.44},
        {{"", "2", "4"}, 0.43},
        {{"", "2", "5"}, 0.45},
        {{"", "2", "6"}, 0.42},
        {{"", "2", "7"}, 0.32},
        {{"", "2", "8"}, 0.34},
        {{"", "2", "9"}, 0.38},
        {{"", "2", "10"}, 0.38},
        {{"", "2", "11"}, 0.35},
        {{"", "2", "12"}, 0.35},
        {{"", "3", "1"}, 0.37},
        {{"", "3", "2"}, 0.44},
        {{"", "3", "4"}, 0.48},
        {{"", "3", "5"}, 0.49},
        {{"", "3", "6"}, 0.47},
        {{"", "3", "7"}, 0.34},
        {{"", "3", "8"}, 0.35},
        {{"", "3", "9"}, 0.42},
        {{"", "3", "10"}, 0.42},
        {{"", "3", "11"}, 0.4},
        {{"", "3", "12"}, 0.39},
        {{"", "4", "1"}, 0.35},
        {{"", "4", "2"}, 0.43},
        {{"", "4", "3"}, 0.48},
        {{"", "4", "5"}, 0.48},
        {{"", "4", "6"}, 0.48},
        {{"", "4", "7"}, 0.32},
        {{"", "4", "8"}, 0.34},
        {{"", "4", "9"}, 0.4},
        {{"", "4", "10"}, 0.41},
        {{"", "4", "11"}, 0.39},
        {{"", "4", "12"}, 0.37},
        {{"", "5", "1"}, 0.37},
        {{"", "5", "2"}, 0.45},
        {{"", "5", "3"}, 0.49},
        {{"", "5", "4"}, 0.48},
        {{"", "5", "6"}, 0.48},
        {{"", "5", "7"}, 0.34},
        {{"", "5", "8"}, 0.35},
        {{"", "5", "9"}, 0.41},
        {{"", "5", "10"}, 0.41},
        {{"", "5", "11"}, 0.4},
        {{"", "5", "12"}, 0.39},
        {{"", "6", "1"}, 0.34},
        {{"", "6", "2"}, 0.42},
        {{"", "6", "3"}, 0.47},
        {{"", "6", "4"}, 0.48},
        {{"", "6", "5"}, 0.48},
        {{"", "6", "7"}, 0.31},
        {{"", "6", "8"}, 0.33},
        {{"", "6", "9"}, 0.37},
        {{"", "6", "10"}, 0.38},
        {{"", "6", "11"}, 0.38},
        {{"", "6", "12"}, 0.36},
        {{"", "7", "1"}, 0.38},
        {{"", "7", "2"}, 0.32},
        {{"", "7", "3"}, 0.34},
        {{"", "7", "4"}, 0.32},
        {{"", "7", "5"}, 0.34},
        {{"", "7", "6"}, 0.31},
        {{"", "7", "8"}, 0.28},
        {{"", "7", "9"}, 0.32},
        {{"", "7", "10"}, 0.3},
        {{"", "7", "11"}, 0.27},
        {{"", "7", "12"}, 0.28},
        {{"", "8", "1"}, 0.31},
        {{"", "8", "2"}, 0.34},
        {{"", "8", "3"}, 0.35},
        {{"", "8", "4"}, 0.34},
        {{"", "8", "5"}, 0.35},
        {{"", "8", "6"}, 0.33},
        {{"", "8", "7"}, 0.28},
        {{"", "8", "9"}, 0.32},
        {{"", "8", "10"}, 0.32},
        {{"", "8", "11"}, 0.29},
        {{"", "8", "12"}, 0.29},
        {{"", "9", "1"}, 0.34},
        {{"", "9", "2"}, 0.38},
        {{"", "9", "3"}, 0.42},
        {{"", "9", "4"}, 0.4},
        {{"", "9", "5"}, 0.41},
        {{"", "9", "6"}, 0.37},
        {{"", "9", "7"}, 0.32},
        {{"", "9", "8"}, 0.32},
        {{"", "9", "10"}, 0.38},
        {{"", "9", "11"}, 0.35},
        {{"", "9", "12"}, 0.35},
        {{"", "10", "1"}, 0.33},
        {{"", "10", "2"}, 0.38},
        {{"", "10", "3"}, 0.42},
        {{"", "10", "4"}, 0.41},
        {{"", "10", "5"}, 0.41},
        {{"", "10", "6"}, 0.38},
        {{"", "10", "7"}, 0.3},
        {{"", "10", "8"}, 0.32},
        {{"", "10", "9"}, 0.38},
        {{"", "10", "11"}, 0.35},
        {{"", "10", "12"}, 0.34},
        {{"", "11", "1"}, 0.3},
        {{"", "11", "2"}, 0.35},
        {{"", "11", "3"}, 0.4},
        {{"", "11", "4"}, 0.39},
        {{"", "11", "5"}, 0.4},
        {{"", "11", "6"}, 0.38},
        {{"", "11", "7"}, 0.27},
        {{"", "11", "8"}, 0.29},
        {{"", "11", "9"}, 0.35},
        {{"", "11", "10"}, 0.35},
        {{"", "11", "12"}, 0.33},
        {{"", "12", "1"}, 0.31},
        {{"", "12", "2"}, 0.35},
        {{"", "12", "3"}, 0.39},
        {{"", "12", "4"}, 0.37},
        {{"", "12", "5"}, 0.39},
        {{"", "12", "6"}, 0.36},
        {{"", "12", "7"}, 0.28},
        {{"", "12", "8"}, 0.29},
        {{"", "12", "9"}, 0.35},
        {{"", "12", "10"}, 0.34},
        {{"", "12", "11"}, 0.33}
    };

    // Equity inter-bucket correlations
    interBucketCorrelation_[CrifRecord::RiskType::Equity] = {
        {{"", "1", "3"}, 0.21},
        {{"", "1", "4"}, 0.21},
        {{"", "1", "5"}, 0.15},
        {{"", "1", "6"}, 0.19},
        {{"", "1", "7"}, 0.19},
        {{"", "1", "8"}, 0.19},
        {{"", "1", "9"}, 0.18},
        {{"", "1", "10"}, 0.14},
        {{"", "1", "11"}, 0.24},
        {{"", "1", "12"}, 0.24},
        {{"", "2", "1"}, 0.2},
        {{"", "2", "3"}, 0.25},
        {{"", "2", "4"}, 0.24},
        {{"", "2", "5"}, 0.16},
        {{"", "2", "6"}, 0.21},
        {{"", "2", "7"}, 0.22},
        {{"", "2", "8"}, 0.21},
        {{"", "2", "9"}, 0.21},
        {{"", "2", "10"}, 0.16},
        {{"", "2", "11"}, 0.27},
        {{"", "2", "12"}, 0.27},
        {{"", "3", "1"}, 0.21},
        {{"", "3", "2"}, 0.25},
        {{"", "3", "4"}, 0.26},
        {{"", "3", "5"}, 0.17},
        {{"", "3", "6"}, 0.22},
        {{"", "3", "7"}, 0.24},
        {{"", "3", "8"}, 0.22},
        {{"", "3", "9"}, 0.23},
        {{"", "3", "10"}, 0.17},
        {{"", "3", "11"}, 0.28},
        {{"", "3", "12"}, 0.28},
        {{"", "4", "1"}, 0.21},
        {{"", "4", "2"}, 0.24},
        {{"", "4", "3"}, 0.26},
        {{"", "4", "5"}, 0.18},
        {{"", "4", "6"}, 0.24},
        {{"", "4", "7"}, 0.25},
        {{"", "4", "8"}, 0.25},
        {{"", "4", "9"}, 0.23},
        {{"", "4", "10"}, 0.19},
        {{"", "4", "11"}, 0.31},
        {{"", "4", "12"}, 0.31},
        {{"", "5", "1"}, 0.15},
        {{"", "5", "2"}, 0.16},
        {{"", "5", "3"}, 0.17},
        {{"", "5", "4"}, 0.18},
        {{"", "5", "6"}, 0.27},
        {{"", "5", "7"}, 0.27},
        {{"", "5", "8"}, 0.27},
        {{"", "5", "9"}, 0.15},
        {{"", "5", "10"}, 0.2},
        {{"", "5", "11"}, 0.32},
        {{"", "5", "12"}, 0.32},
        {{"", "6", "1"}, 0.19},
        {{"", "6", "2"}, 0.21},
        {{"", "6", "3"}, 0.22},
        {{"", "6", "4"}, 0.24},
        {{"", "6", "5"}, 0.27},
        {{"", "6", "7"}, 0.36},
        {{"", "6", "8"}, 0.35},
        {{"", "6", "9"}, 0.2},
        {{"", "6", "10"}, 0.25},
        {{"", "6", "11"}, 0.42},
        {{"", "6", "12"}, 0.42},
        {{"", "7", "1"}, 0.19},
        {{"", "7", "2"}, 0.22},
        {{"", "7", "3"}, 0.24},
        {{"", "7", "4"}, 0.25},
        {{"", "7", "5"}, 0.27},
        {{"", "7", "6"}, 0.36},
        {{"", "7", "8"}, 0.34},
        {{"", "7", "9"}, 0.2},
        {{"", "7", "10"}, 0.26},
        {{"", "7", "11"}, 0.43},
        {{"", "7", "12"}, 0.43},
        {{"", "8", "1"}, 0.19},
        {{"", "8", "2"}, 0.21},
        {{"", "8", "3"}, 0.22},
        {{"", "8", "4"}, 0.25},
        {{"", "8", "5"}, 0.27},
        {{"", "8", "6"}, 0.35},
        {{"", "8", "7"}, 0.34},
        {{"", "8", "9"}, 0.2},
        {{"", "8", "10"}, 0.25},
        {{"", "8", "11"}, 0.41},
        {{"", "8", "12"}, 0.41},
        {{"", "9", "1"}, 0.18},
        {{"", "9", "2"}, 0.21},
        {{"", "9", "3"}, 0.23},
        {{"", "9", "4"}, 0.23},
        {{"", "9", "5"}, 0.15},
        {{"", "9", "6"}, 0.2},
        {{"", "9", "7"}, 0.2},
        {{"", "9", "8"}, 0.2},
        {{"", "9", "10"}, 0.16},
        {{"", "9", "11"}, 0.26},
        {{"", "9", "12"}, 0.26},
        {{"", "10", "1"}, 0.14},
        {{"", "10", "2"}, 0.16},
        {{"", "10", "3"}, 0.17},
        {{"", "10", "4"}, 0.19},
        {{"", "10", "5"}, 0.2},
        {{"", "10", "6"}, 0.25},
        {{"", "10", "7"}, 0.26},
        {{"", "10", "8"}, 0.25},
        {{"", "10", "9"}, 0.16},
        {{"", "10", "11"}, 0.29},
        {{"", "10", "12"}, 0.29},
        {{"", "11", "1"}, 0.24},
        {{"", "11", "2"}, 0.27},
        {{"", "11", "3"}, 0.28},
        {{"", "11", "4"}, 0.31},
        {{"", "11", "5"}, 0.32},
        {{"", "11", "6"}, 0.42},
        {{"", "11", "7"}, 0.43},
        {{"", "11", "8"}, 0.41},
        {{"", "11", "9"}, 0.26},
        {{"", "11", "10"}, 0.29},
        {{"", "11", "12"}, 0.54},
        {{"", "12", "1"}, 0.24},
        {{"", "12", "2"}, 0.27},
        {{"", "12", "3"}, 0.28},
        {{"", "12", "4"}, 0.31},
        {{"", "12", "5"}, 0.32},
        {{"", "12", "6"}, 0.42},
        {{"", "12", "7"}, 0.43},
        {{"", "12", "8"}, 0.41},
        {{"", "12", "9"}, 0.26},
        {{"", "12", "10"}, 0.29},
        {{"", "12", "11"}, 0.54}
    };

    // Commodity inter-bucket correlations
    interBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        {{"", "1", "2"}, 0.36},
        {{"", "1", "3"}, 0.23},
        {{"", "1", "4"}, 0.3},
        {{"", "1", "5"}, 0.3},
        {{"", "1", "6"}, 0.07},
        {{"", "1", "7"}, 0.32},
        {{"", "1", "8"}, 0.02},
        {{"", "1", "9"}, 0.26},
        {{"", "1", "10"}, 0.2},
        {{"", "1", "11"}, 0.17},
        {{"", "1", "12"}, 0.15},
        {{"", "1", "13"}, 0.21},
        {{"", "1", "14"}, 0.15},
        {{"", "1", "15"}, 0.19},
        {{"", "1", "16"}, 0.0},
        {{"", "1", "17"}, 0.24},
        {{"", "2", "1"}, 0.36},
        {{"", "2", "3"}, 0.93},
        {{"", "2", "4"}, 0.94},
        {{"", "2", "5"}, 0.88},
        {{"", "2", "6"}, 0.16},
        {{"", "2", "7"}, 0.21},
        {{"", "2", "8"}, 0.09},
        {{"", "2", "9"}, 0.21},
        {{"", "2", "10"}, 0.2},
        {{"", "2", "11"}, 0.4},
        {{"", "2", "12"}, 0.3},
        {{"", "2", "13"}, 0.24},
        {{"", "2", "14"}, 0.29},
        {{"", "2", "15"}, 0.23},
        {{"", "2", "16"}, 0.0},
        {{"", "2", "17"}, 0.56},
        {{"", "3", "1"}, 0.23},
        {{"", "3", "2"}, 0.93},
        {{"", "3", "4"}, 0.91},
        {{"", "3", "5"}, 0.85},
        {{"", "3", "6"}, 0.06},
        {{"", "3", "7"}, 0.21},
        {{"", "3", "8"}, 0.04},
        {{"", "3", "9"}, 0.21},
        {{"", "3", "10"}, 0.19},
        {{"", "3", "11"}, 0.33},
        {{"", "3", "12"}, 0.23},
        {{"", "3", "13"}, 0.14},
        {{"", "3", "14"}, 0.23},
        {{"", "3", "15"}, 0.25},
        {{"", "3", "16"}, 0.0},
        {{"", "3", "17"}, 0.5},
        {{"", "4", "1"}, 0.3},
        {{"", "4", "2"}, 0.94},
        {{"", "4", "3"}, 0.91},
        {{"", "4", "5"}, 0.83},
        {{"", "4", "6"}, 0.06},
        {{"", "4", "7"}, 0.24},
        {{"", "4", "8"}, 0.04},
        {{"", "4", "9"}, 0.21},
        {{"", "4", "10"}, 0.17},
        {{"", "4", "11"}, 0.36},
        {{"", "4", "12"}, 0.25},
        {{"", "4", "13"}, 0.14},
        {{"", "4", "14"}, 0.25},
        {{"", "4", "15"}, 0.2},
        {{"", "4", "16"}, 0.0},
        {{"", "4", "17"}, 0.53},
        {{"", "5", "1"}, 0.3},
        {{"", "5", "2"}, 0.88},
        {{"", "5", "3"}, 0.85},
        {{"", "5", "4"}, 0.83},
        {{"", "5", "6"}, 0.1},
        {{"", "5", "7"}, 0.17},
        {{"", "5", "8"}, 0.04},
        {{"", "5", "9"}, 0.16},
        {{"", "5", "10"}, 0.17},
        {{"", "5", "11"}, 0.4},
        {{"", "5", "12"}, 0.33},
        {{"", "5", "13"}, 0.25},
        {{"", "5", "14"}, 0.3},
        {{"", "5", "15"}, 0.19},
        {{"", "5", "16"}, 0.0},
        {{"", "5", "17"}, 0.53},
        {{"", "6", "1"}, 0.07},
        {{"", "6", "2"}, 0.16},
        {{"", "6", "3"}, 0.06},
        {{"", "6", "4"}, 0.06},
        {{"", "6", "5"}, 0.1},
        {{"", "6", "7"}, 0.27},
        {{"", "6", "8"}, 0.5},
        {{"", "6", "9"}, 0.2},
        {{"", "6", "10"}, 0.04},
        {{"", "6", "11"}, 0.17},
        {{"", "6", "12"}, 0.08},
        {{"", "6", "13"}, 0.12},
        {{"", "6", "14"}, 0.08},
        {{"", "6", "15"}, 0.14},
        {{"", "6", "16"}, 0.0},
        {{"", "6", "17"}, 0.25},
        {{"", "7", "1"}, 0.32},
        {{"", "7", "2"}, 0.21},
        {{"", "7", "3"}, 0.21},
        {{"", "7", "4"}, 0.24},
        {{"", "7", "5"}, 0.17},
        {{"", "7", "6"}, 0.27},
        {{"", "7", "8"}, 0.27},
        {{"", "7", "9"}, 0.61},
        {{"", "7", "10"}, 0.18},
        {{"", "7", "11"}, 0.06},
        {{"", "7", "12"}, -0.11},
        {{"", "7", "13"}, 0.12},
        {{"", "7", "14"}, 0.08},
        {{"", "7", "15"}, 0.08},
        {{"", "7", "16"}, 0.0},
        {{"", "7", "17"}, 0.22},
        {{"", "8", "1"}, 0.02},
        {{"", "8", "2"}, 0.09},
        {{"", "8", "3"}, 0.04},
        {{"", "8", "4"}, 0.04},
        {{"", "8", "5"}, 0.04},
        {{"", "8", "6"}, 0.5},
        {{"", "8", "7"}, 0.27},
        {{"", "8", "9"}, 0.19},
        {{"", "8", "10"}, 0.0},
        {{"", "8", "11"}, 0.12},
        {{"", "8", "12"}, -0.03},
        {{"", "8", "13"}, 0.09},
        {{"", "8", "14"}, 0.05},
        {{"", "8", "15"}, 0.07},
        {{"", "8", "16"}, 0.0},
        {{"", "8", "17"}, 0.14},
        {{"", "9", "1"}, 0.26},
        {{"", "9", "2"}, 0.21},
        {{"", "9", "3"}, 0.21},
        {{"", "9", "4"}, 0.21},
        {{"", "9", "5"}, 0.16},
        {{"", "9", "6"}, 0.2},
        {{"", "9", "7"}, 0.61},
        {{"", "9", "8"}, 0.19},
        {{"", "9", "10"}, 0.14},
        {{"", "9", "11"}, 0.13},
        {{"", "9", "12"}, -0.07},
        {{"", "9", "13"}, 0.07},
        {{"", "9", "14"}, 0.06},
        {{"", "9", "15"}, 0.12},
        {{"", "9", "16"}, 0.0},
        {{"", "9", "17"}, 0.19},
        {{"", "10", "1"}, 0.2},
        {{"", "10", "2"}, 0.2},
        {{"", "10", "3"}, 0.19},
        {{"", "10", "4"}, 0.17},
        {{"", "10", "5"}, 0.17},
        {{"", "10", "6"}, 0.04},
        {{"", "10", "7"}, 0.18},
        {{"", "10", "8"}, 0.0},
        {{"", "10", "9"}, 0.4},
        {{"", "10", "11"}, 0.11},
        {{"", "10", "12"}, 0.13},
        {{"", "10", "13"}, 0.07},
        {{"", "10", "14"}, 0.06},
        {{"", "10", "15"}, 0.06},
        {{"", "10", "16"}, 0.0},
        {{"", "10", "17"}, 0.11},
        {{"", "11", "1"}, 0.17},
        {{"", "11", "2"}, 0.4},
        {{"", "11", "3"}, 0.33},
        {{"", "11", "4"}, 0.36},
        {{"", "11", "5"}, 0.4},
        {{"", "11", "6"}, 0.17},
        {{"", "11", "7"}, 0.06},
        {{"", "11", "8"}, 0.12},
        {{"", "11", "9"}, 0.13},
        {{"", "11", "10"}, 0.11},
        {{"", "11", "12"}, 0.31},
        {{"", "11", "13"}, 0.27},
        {{"", "11", "14"}, 0.21},
        {{"", "11", "15"}, 0.2},
        {{"", "11", "16"}, 0.0},
        {{"", "11", "17"}, 0.37},
        {{"", "12", "1"}, 0.15},
        {{"", "12", "2"}, 0.3},
        {{"", "12", "3"}, 0.23},
        {{"", "12", "4"}, 0.25},
        {{"", "12", "5"}, 0.33},
        {{"", "12", "6"}, 0.08},
        {{"", "12", "7"}, -0.11},
        {{"", "12", "8"}, -0.03},
        {{"", "12", "9"}, -0.07},
        {{"", "12", "10"}, 0.13},
        {{"", "12", "11"}, 0.31},
        {{"", "12", "13"}, 0.15},
        {{"", "12", "14"}, 0.19},
        {{"", "12", "15"}, 0.1},
        {{"", "12", "16"}, 0.0},
        {{"", "12", "17"}, 0.23},
        {{"", "13", "1"}, 0.21},
        {{"", "13", "2"}, 0.24},
        {{"", "13", "3"}, 0.14},
        {{"", "13", "4"}, 0.14},
        {{"", "13", "5"}, 0.25},
        {{"", "13", "6"}, 0.12},
        {{"", "13", "7"}, 0.12},
        {{"", "13", "8"}, 0.09},
        {{"", "13", "9"}, 0.07},
        {{"", "13", "10"}, 0.07},
        {{"", "13", "11"}, 0.27},
        {{"", "13", "12"}, 0.15},
        {{"", "13", "14"}, 0.28},
        {{"", "13", "15"}, 0.2},
        {{"", "13", "16"}, 0.0},
        {{"", "13", "17"}, 0.27},
        {{"", "14", "1"}, 0.15},
        {{"", "14", "2"}, 0.29},
        {{"", "14", "3"}, 0.23},
        {{"", "14", "4"}, 0.25},
        {{"", "14", "5"}, 0.3},
        {{"", "14", "6"}, 0.08},
        {{"", "14", "7"}, 0.08},
        {{"", "14", "8"}, 0.05},
        {{"", "14", "9"}, 0.06},
        {{"", "14", "10"}, 0.06},
        {{"", "14", "11"}, 0.21},
        {{"", "14", "12"}, 0.19},
        {{"", "14", "13"}, 0.28},
        {{"", "14", "15"}, 0.15},
        {{"", "14", "16"}, 0.0},
        {{"", "14", "17"}, 0.25},
        {{"", "15", "1"}, 0.19},
        {{"", "15", "2"}, 0.23},
        {{"", "15", "3"}, 0.25},
        {{"", "15", "4"}, 0.2},
        {{"", "15", "5"}, 0.19},
        {{"", "15", "6"}, 0.14},
        {{"", "15", "7"}, 0.08},
        {{"", "15", "8"}, 0.07},
        {{"", "15", "9"}, 0.12},
        {{"", "15", "10"}, 0.06},
        {{"", "15", "11"}, 0.2},
        {{"", "15", "12"}, 0.1},
        {{"", "15", "13"}, 0.2},
        {{"", "15", "14"}, 0.15},
        {{"", "15", "16"}, 0.0},
        {{"", "15", "17"}, 0.23},
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
        {{"", "17", "1"}, 0.24},
        {{"", "17", "2"}, 0.56},
        {{"", "17", "3"}, 0.5},
        {{"", "17", "4"}, 0.53},
        {{"", "17", "5"}, 0.53},
        {{"", "17", "6"}, 0.25},
        {{"", "17", "7"}, 0.22},
        {{"", "17", "8"}, 0.14},
        {{"", "17", "9"}, 0.19},
        {{"", "17", "10"}, 0.11},
        {{"", "17", "11"}, 0.37},
        {{"", "17", "12"}, 0.23},
        {{"", "17", "13"}, 0.27},
        {{"", "17", "14"}, 0.25},
        {{"", "17", "15"}, 0.23},
        {{"", "17", "16"}, 0.0}
    };

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[CrifRecord::RiskType::Equity] = {
        {{"1", "", ""}, 0.18},
        {{"2", "", ""}, 0.23},
        {{"3", "", ""}, 0.28},
        {{"4", "", ""}, 0.27},
        {{"5", "", ""}, 0.23},
        {{"6", "", ""}, 0.36},
        {{"7", "", ""}, 0.38},
        {{"8", "", ""}, 0.35},
        {{"9", "", ""}, 0.21},
        {{"10", "", ""}, 0.20},
        {{"11", "", ""}, 0.54},
        {{"12", "", ""}, 0.54}
    };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        {{"1", "", ""}, 0.79},
        {{"2", "", ""}, 0.98},
        {{"3", "", ""}, 0.96},
        {{"4", "", ""}, 0.97},
        {{"5", "", ""}, 0.98},
        {{"6", "", ""}, 0.88},
        {{"7", "", ""}, 0.97},
        {{"8", "", ""}, 0.42},
        {{"9", "", ""}, 0.70}, 
        {{"10", "", ""}, 0.38},
        {{"11", "", ""}, 0.54},
        {{"12", "", ""}, 0.48},
        {{"13", "", ""}, 0.67},
        {{"14", "", ""}, 0.15},
        {{"15", "", ""}, 0.23},
        {{"16", "", ""}, 0.00},
        {{"17", "", ""}, 0.33}
    };

    // Initialise the single, ad-hoc type, correlations 
    xccyCorr_ = 0.07;
    infCorr_ = 0.41;
    infVolCorr_ = 0.41;
    irSubCurveCorr_ = 0.986;
    irInterCurrencyCorr_ = 0.22;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.92;
    crqDiffIntraCorr_ = 0.41;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.86;
    crnqDiffIntraCorr_ = 0.33;
    crnqInterCorr_ = 0.36;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.25;

    // clang-format on
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 8 section 11(d)
of the ISDA-SIMM-v2.3.8 documentation).
*/
QuantLib::Real SimmConfiguration_ISDA_V2_3_8::curvatureMarginScaling() const { return pow(hvr_ir_, -2.0); }

void SimmConfiguration_ISDA_V2_3_8::addLabels2(const CrifRecord::RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V2_3_8::label2(const QuantLib::ext::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::label2(irIndex);
}

} // namespace analytics
} // namespace ore
