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

using RiskType = CrifRecord::RiskType;

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

SimmConfiguration_ISDA_V2_3::SimmConfiguration_ISDA_V2_3(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const QuantLib::Size& mporDays, const std::string& name,
                                                         const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentration_ISDA_V2_3>(simmBucketMapper_);
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
            {RiskType::CreditQ,
             {{{"1", "", ""}, 75.0},
              {{"2", "", ""}, 89.0},
              {{"3", "", ""}, 68.0},
              {{"4", "", ""}, 51.0},
              {{"5", "", ""}, 50.0},
              {{"6", "", ""}, 47.0},
              {{"7", "", ""}, 157.0},
              {{"8", "", ""}, 333.0},
              {{"9", "", ""}, 142.0},
              {{"10", "", ""}, 214.0},
              {{"11", "", ""}, 143.0},
              {{"12", "", ""}, 160.0},
              {{"Residual", "", ""}, 333.0}}},
            {RiskType::CreditNonQ,
             {{{"1", "", ""}, 240.0},
              {{"2", "", ""}, 1000.0},
              {{"Residual", "", ""}, 1000.0}}},
            {RiskType::Equity,
             {{{"1", "", ""}, 23.0},
              {{"2", "", ""}, 25.0},
              {{"3", "", ""}, 29.0},
              {{"4", "", ""}, 26.0},
              {{"5", "", ""}, 20.0},
              {{"6", "", ""}, 21.0},
              {{"7", "", ""}, 24.0},
              {{"8", "", ""}, 24.0},
              {{"9", "", ""}, 29.0},
              {{"10", "", ""}, 28.0},
              {{"11", "", ""}, 15.0},
              {{"12", "", ""}, 15.0},
              {{"Residual", "", ""}, 29.0}}},
            {RiskType::Commodity,
             {{{"1", "", ""}, 16.0},
              {{"2", "", ""}, 20.0},
              {{"3", "", ""}, 23.0},
              {{"4", "", ""}, 18.0},
              {{"5", "", ""}, 28.0},
              {{"6", "", ""}, 18.0},
              {{"7", "", ""}, 17.0},
              {{"8", "", ""}, 57.0},
              {{"9", "", ""}, 21.0},
              {{"10", "", ""}, 39.0},
              {{"11", "", ""}, 20.0},
              {{"12", "", ""}, 20.0},
              {{"13", "", ""}, 15.0},
              {{"14", "", ""}, 15.0},
              {{"15", "", ""}, 11.0},
              {{"16", "", ""}, 57.0},
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
              {{"12", "", ""}, 0.67},
              {{"Residual", "", ""}, 0.26}}}
        };
        
        rwLabel_1_ = {
            {RiskType::IRCurve,
                {{{"1", "2w", ""}, 114.0},
                 {{"1", "1m", ""}, 107.0},
                 {{"1", "3m", ""}, 95.0},
                 {{"1", "6m", ""}, 71.0},
                 {{"1", "1y", ""}, 56.0},
                 {{"1", "2y", ""}, 53.0},
                 {{"1", "3y", ""}, 50.0},
                 {{"1", "5y", ""}, 51.0},
                 {{"1", "10y", ""}, 53.0},
                 {{"1", "15y", ""}, 50.0},
                 {{"1", "20y", ""}, 54.0},
                 {{"1", "30y", ""}, 63.0},
                 {{"2", "2w", ""}, 15.0},
                 {{"2", "1m", ""}, 21.0},
                 {{"2", "3m", ""}, 10.0},
                 {{"2", "6m", ""}, 10.0},
                 {{"2", "1y", ""}, 11.0},
                 {{"2", "2y", ""}, 15.0},
                 {{"2", "3y", ""}, 18.0},
                 {{"2", "5y", ""}, 19.0},
                 {{"2", "10y", ""}, 19.0},
                 {{"2", "15y", ""}, 18.0},
                 {{"2", "20y", ""}, 20.0},
                 {{"2", "30y", ""}, 22.0},
                 {{"3", "2w", ""}, 103.0},
                 {{"3", "1m", ""}, 96.0},
                 {{"3", "3m", ""}, 84.0},
                 {{"3", "6m", ""}, 84.0},
                 {{"3", "1y", ""}, 89.0},
                 {{"3", "2y", ""}, 87.0},
                 {{"3", "3y", ""}, 90.0},
                 {{"3", "5y", ""}, 89.0},
                 {{"3", "10y", ""}, 90.0},
                 {{"3", "15y", ""}, 99.0},
                 {{"3", "20y", ""}, 100.0},
                 {{"3", "30y", ""}, 96.0}}
            }
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
            {RiskType::CreditQ,
             {{{"1", "", ""}, 17.0},
              {{"2", "", ""}, 22.0},
              {{"3", "", ""}, 14.0},
              {{"4", "", ""}, 11.0},
              {{"5", "", ""}, 11.0},
              {{"6", "", ""}, 9.5},
              {{"7", "", ""}, 39.0},
              {{"8", "", ""}, 63.0},
              {{"9", "", ""}, 28.0},
              {{"10", "", ""}, 45.0},
              {{"11", "", ""}, 32.0},
              {{"12", "", ""}, 35.0},
              {{"Residual", "", ""}, 63.0}}},
            {RiskType::CreditNonQ,
             {{{"1", "", ""}, 83.0},
              {{"2", "", ""}, 220.0},
              {{"Residual", "", ""}, 220.0}}},
            {RiskType::Equity,
             {{{"1", "", ""}, 7.6},
              {{"2", "", ""}, 8.1},
              {{"3", "", ""}, 9.4},
              {{"4", "", ""}, 8.3},
              {{"5", "", ""}, 6.8},
              {{"6", "", ""}, 7.4},
              {{"7", "", ""}, 8.5},
              {{"8", "", ""}, 8.6},
              {{"9", "", ""}, 9.4},
              {{"10", "", ""}, 9.3},
              {{"11", "", ""}, 5.5},
              {{"12", "", ""}, 5.5},
              {{"Residual", "", ""}, 9.4}}},
            {RiskType::Commodity,
             {{{"1", "", ""}, 5.7},
              {{"2", "", ""}, 7.0},
              {{"3", "", ""}, 7.0},
              {{"4", "", ""}, 5.5},
              {{"5", "", ""}, 8.9},
              {{"6", "", ""}, 6.6},
              {{"7", "", ""}, 6.5},
              {{"8", "", ""}, 13.0},
              {{"9", "", ""}, 6.8},
              {{"10", "", ""}, 13},
              {{"11", "", ""}, 6.0},
              {{"12", "", ""}, 5.7},
              {{"13", "", ""}, 4.9},
              {{"14", "", ""}, 4.6},
              {{"15", "", ""}, 3.0},
              {{"16", "", ""}, 13.0},
              {{"17", "", ""}, 4.8}}},
            {RiskType::EquityVol,
             {{{"1", "", ""}, 0.065},
              {{"2", "", ""}, 0.065},
              {{"3", "", ""}, 0.065},
              {{"4", "", ""}, 0.065},
              {{"5", "", ""}, 0.065},
              {{"6", "", ""}, 0.065},
              {{"7", "", ""}, 0.065},
              {{"8", "", ""}, 0.065},
              {{"9", "", ""}, 0.065},
              {{"10", "", ""}, 0.065},
              {{"11", "", ""}, 0.065},
              {{"12", "", ""}, 0.22},
              {{"Residual", "", ""}, 0.065}}}
        };
        
        rwLabel_1_ = {
            {RiskType::IRCurve,
                {{{"1", "2w", ""}, 18.0},
                 {{"1", "1m", ""}, 15.0},
                 {{"1", "3m", ""}, 10.0},
                 {{"1", "6m", ""}, 11.0},
                 {{"1", "1y", ""}, 13.0},
                 {{"1", "2y", ""}, 15.0},
                 {{"1", "3y", ""}, 16.0},
                 {{"1", "5y", ""}, 16.0},
                 {{"1", "10y", ""}, 16.0},
                 {{"1", "15y", ""}, 16.0},
                 {{"1", "20y", ""}, 16.0},
                 {{"1", "30y", ""}, 17.0},
                 {{"2", "2w", ""}, 1.7},
                 {{"2", "1m", ""}, 3.4},
                 {{"2", "3m", ""}, 1.9},
                 {{"2", "6m", ""}, 1.5},
                 {{"2", "1y", ""}, 2.8},
                 {{"2", "2y", ""}, 4.5},
                 {{"2", "3y", ""}, 4.8},
                 {{"2", "5y", ""}, 6.1},
                 {{"2", "10y", ""}, 6.3},
                 {{"2", "15y", ""}, 6.8},
                 {{"2", "20y", ""}, 7.3},
                 {{"2", "30y", ""}, 7.5},
                 {{"3", "2w", ""}, 29.0},
                 {{"3", "1m", ""}, 35.0},
                 {{"3", "3m", ""}, 18.0},
                 {{"3", "6m", ""}, 20.0},
                 {{"3", "1y", ""}, 21.0},
                 {{"3", "2y", ""}, 23.0},
                 {{"3", "3y", ""}, 33.0},
                 {{"3", "5y", ""}, 32.0},
                 {{"3", "10y", ""}, 31.0},
                 {{"3", "15y", ""}, 26.0},
                 {{"3", "20y", ""}, 27.0},
                 {{"3", "30y", ""}, 26.0}}
            }
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
    riskClassCorrelation_ = {
        {{"", "InterestRate", "CreditQualifying"}, 0.29},
        {{"", "InterestRate", "CreditNonQualifying"}, 0.28},
        {{"", "InterestRate", "Equity"}, 0.31},
        {{"", "InterestRate", "Commodity"}, 0.35},
        {{"", "InterestRate", "FX"}, 0.18},
        {{"", "CreditQualifying", "InterestRate"}, 0.29},
        {{"", "CreditQualifying", "CreditNonQualifying"}, 0.51},
        {{"", "CreditQualifying", "Equity"}, 0.61},
        {{"", "CreditQualifying", "Commodity"}, 0.43},
        {{"", "CreditQualifying", "FX"}, 0.35},
        {{"", "CreditNonQualifying", "InterestRate"}, 0.28},
        {{"", "CreditNonQualifying", "CreditQualifying"}, 0.51},
        {{"", "CreditNonQualifying", "Equity"}, 0.47},
        {{"", "CreditNonQualifying", "Commodity"}, 0.34},
        {{"", "CreditNonQualifying", "FX"}, 0.18},
        {{"", "Equity", "InterestRate"}, 0.31},
        {{"", "Equity", "CreditQualifying"}, 0.61},
        {{"", "Equity", "CreditNonQualifying"}, 0.47},
        {{"", "Equity", "Commodity"}, 0.47},
        {{"", "Equity", "FX"}, 0.3},
        {{"", "Commodity", "InterestRate"}, 0.35},
        {{"", "Commodity", "CreditQualifying"}, 0.43},
        {{"", "Commodity", "CreditNonQualifying"}, 0.34},
        {{"", "Commodity", "Equity"}, 0.47},
        {{"", "Commodity", "FX"}, 0.31},
        {{"", "FX", "InterestRate"}, 0.18},
        {{"", "FX", "CreditQualifying"}, 0.35},
        {{"", "FX", "CreditNonQualifying"}, 0.18},
        {{"", "FX", "Equity"}, 0.3},
        {{"", "FX", "Commodity"}, 0.31},
    };

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
    intraBucketCorrelation_[RiskType::IRCurve] = {
        {{"", "2w", "1m"}, 0.73},
        {{"", "2w", "3m"}, 0.64},
        {{"", "2w", "6m"}, 0.57},
        {{"", "2w", "1y"}, 0.44},
        {{"", "2w", "2y"}, 0.34},
        {{"", "2w", "3y"}, 0.29},
        {{"", "2w", "5y"}, 0.24},
        {{"", "2w", "10y"}, 0.18},
        {{"", "2w", "15y"}, 0.13},
        {{"", "2w", "20y"}, 0.11},
        {{"", "2w", "30y"}, 0.09},
        {{"", "1m", "2w"}, 0.73},
        {{"", "1m", "3m"}, 0.78},
        {{"", "1m", "6m"}, 0.67},
        {{"", "1m", "1y"}, 0.5},
        {{"", "1m", "2y"}, 0.37},
        {{"", "1m", "3y"}, 0.3},
        {{"", "1m", "5y"}, 0.24},
        {{"", "1m", "10y"}, 0.18},
        {{"", "1m", "15y"}, 0.13},
        {{"", "1m", "20y"}, 0.11},
        {{"", "1m", "30y"}, 0.1},
        {{"", "3m", "2w"}, 0.64},
        {{"", "3m", "1m"}, 0.78},
        {{"", "3m", "6m"}, 0.85},
        {{"", "3m", "1y"}, 0.66},
        {{"", "3m", "2y"}, 0.52},
        {{"", "3m", "3y"}, 0.43},
        {{"", "3m", "5y"}, 0.35},
        {{"", "3m", "10y"}, 0.27},
        {{"", "3m", "15y"}, 0.2},
        {{"", "3m", "20y"}, 0.17},
        {{"", "3m", "30y"}, 0.17},
        {{"", "6m", "2w"}, 0.57},
        {{"", "6m", "1m"}, 0.67},
        {{"", "6m", "3m"}, 0.85},
        {{"", "6m", "1y"}, 0.81},
        {{"", "6m", "2y"}, 0.68},
        {{"", "6m", "3y"}, 0.59},
        {{"", "6m", "5y"}, 0.5},
        {{"", "6m", "10y"}, 0.41},
        {{"", "6m", "15y"}, 0.35},
        {{"", "6m", "20y"}, 0.33},
        {{"", "6m", "30y"}, 0.31},
        {{"", "1y", "2w"}, 0.44},
        {{"", "1y", "1m"}, 0.5},
        {{"", "1y", "3m"}, 0.66},
        {{"", "1y", "6m"}, 0.81},
        {{"", "1y", "2y"}, 0.94},
        {{"", "1y", "3y"}, 0.85},
        {{"", "1y", "5y"}, 0.76},
        {{"", "1y", "10y"}, 0.65},
        {{"", "1y", "15y"}, 0.59},
        {{"", "1y", "20y"}, 0.56},
        {{"", "1y", "30y"}, 0.54},
        {{"", "2y", "2w"}, 0.34},
        {{"", "2y", "1m"}, 0.37},
        {{"", "2y", "3m"}, 0.52},
        {{"", "2y", "6m"}, 0.68},
        {{"", "2y", "1y"}, 0.94},
        {{"", "2y", "3y"}, 0.95},
        {{"", "2y", "5y"}, 0.89},
        {{"", "2y", "10y"}, 0.79},
        {{"", "2y", "15y"}, 0.75},
        {{"", "2y", "20y"}, 0.72},
        {{"", "2y", "30y"}, 0.7},
        {{"", "3y", "2w"}, 0.29},
        {{"", "3y", "1m"}, 0.3},
        {{"", "3y", "3m"}, 0.43},
        {{"", "3y", "6m"}, 0.59},
        {{"", "3y", "1y"}, 0.85},
        {{"", "3y", "2y"}, 0.95},
        {{"", "3y", "5y"}, 0.96},
        {{"", "3y", "10y"}, 0.88},
        {{"", "3y", "15y"}, 0.83},
        {{"", "3y", "20y"}, 0.8},
        {{"", "3y", "30y"}, 0.78},
        {{"", "5y", "2w"}, 0.24},
        {{"", "5y", "1m"}, 0.24},
        {{"", "5y", "3m"}, 0.35},
        {{"", "5y", "6m"}, 0.5},
        {{"", "5y", "1y"}, 0.76},
        {{"", "5y", "2y"}, 0.89},
        {{"", "5y", "3y"}, 0.96},
        {{"", "5y", "10y"}, 0.95},
        {{"", "5y", "15y"}, 0.91},
        {{"", "5y", "20y"}, 0.88},
        {{"", "5y", "30y"}, 0.87},
        {{"", "10y", "2w"}, 0.18},
        {{"", "10y", "1m"}, 0.18},
        {{"", "10y", "3m"}, 0.27},
        {{"", "10y", "6m"}, 0.41},
        {{"", "10y", "1y"}, 0.65},
        {{"", "10y", "2y"}, 0.79},
        {{"", "10y", "3y"}, 0.88},
        {{"", "10y", "5y"}, 0.95},
        {{"", "10y", "15y"}, 0.97},
        {{"", "10y", "20y"}, 0.95},
        {{"", "10y", "30y"}, 0.95},
        {{"", "15y", "2w"}, 0.13},
        {{"", "15y", "1m"}, 0.13},
        {{"", "15y", "3m"}, 0.2},
        {{"", "15y", "6m"}, 0.35},
        {{"", "15y", "1y"}, 0.59},
        {{"", "15y", "2y"}, 0.75},
        {{"", "15y", "3y"}, 0.83},
        {{"", "15y", "5y"}, 0.91},
        {{"", "15y", "10y"}, 0.97},
        {{"", "15y", "20y"}, 0.98},
        {{"", "15y", "30y"}, 0.98},
        {{"", "20y", "2w"}, 0.11},
        {{"", "20y", "1m"}, 0.11},
        {{"", "20y", "3m"}, 0.17},
        {{"", "20y", "6m"}, 0.33},
        {{"", "20y", "1y"}, 0.56},
        {{"", "20y", "2y"}, 0.72},
        {{"", "20y", "3y"}, 0.8},
        {{"", "20y", "5y"}, 0.88},
        {{"", "20y", "10y"}, 0.95},
        {{"", "20y", "15y"}, 0.98},
        {{"", "20y", "30y"}, 0.99},
        {{"", "30y", "2w"}, 0.09},
        {{"", "30y", "1m"}, 0.1},
        {{"", "30y", "3m"}, 0.17},
        {{"", "30y", "6m"}, 0.31},
        {{"", "30y", "1y"}, 0.54},
        {{"", "30y", "2y"}, 0.7},
        {{"", "30y", "3y"}, 0.78},
        {{"", "30y", "5y"}, 0.87},
        {{"", "30y", "10y"}, 0.95},
        {{"", "30y", "15y"}, 0.98},
        {{"", "30y", "20y"}, 0.99}
    };

    // CreditQ inter-bucket correlations 
    interBucketCorrelation_[RiskType::CreditQ] = {
        {{"", "1", "2"}, 0.39},
        {{"", "1", "3"}, 0.39},
        {{"", "1", "4"}, 0.4},
        {{"", "1", "5"}, 0.4},
        {{"", "1", "6"}, 0.37},
        {{"", "1", "7"}, 0.39},
        {{"", "1", "8"}, 0.32},
        {{"", "1", "9"}, 0.35},
        {{"", "1", "10"}, 0.33},
        {{"", "1", "11"}, 0.33},
        {{"", "1", "12"}, 0.3},
        {{"", "2", "1"}, 0.39},
        {{"", "2", "3"}, 0.43},
        {{"", "2", "4"}, 0.45},
        {{"", "2", "5"}, 0.45},
        {{"", "2", "6"}, 0.43},
        {{"", "2", "7"}, 0.32},
        {{"", "2", "8"}, 0.34},
        {{"", "2", "9"}, 0.39},
        {{"", "2", "10"}, 0.36},
        {{"", "2", "11"}, 0.36},
        {{"", "2", "12"}, 0.31},
        {{"", "3", "1"}, 0.39},
        {{"", "3", "2"}, 0.43},
        {{"", "3", "4"}, 0.47},
        {{"", "3", "5"}, 0.48},
        {{"", "3", "6"}, 0.45},
        {{"", "3", "7"}, 0.33},
        {{"", "3", "8"}, 0.33},
        {{"", "3", "9"}, 0.41},
        {{"", "3", "10"}, 0.38},
        {{"", "3", "11"}, 0.39},
        {{"", "3", "12"}, 0.35},
        {{"", "4", "1"}, 0.4},
        {{"", "4", "2"}, 0.45},
        {{"", "4", "3"}, 0.47},
        {{"", "4", "5"}, 0.49},
        {{"", "4", "6"}, 0.47},
        {{"", "4", "7"}, 0.33},
        {{"", "4", "8"}, 0.34},
        {{"", "4", "9"}, 0.41},
        {{"", "4", "10"}, 0.4},
        {{"", "4", "11"}, 0.39},
        {{"", "4", "12"}, 0.34},
        {{"", "5", "1"}, 0.4},
        {{"", "5", "2"}, 0.45},
        {{"", "5", "3"}, 0.48},
        {{"", "5", "4"}, 0.49},
        {{"", "5", "6"}, 0.48},
        {{"", "5", "7"}, 0.33},
        {{"", "5", "8"}, 0.34},
        {{"", "5", "9"}, 0.41},
        {{"", "5", "10"}, 0.39},
        {{"", "5", "11"}, 0.4},
        {{"", "5", "12"}, 0.34},
        {{"", "6", "1"}, 0.37},
        {{"", "6", "2"}, 0.43},
        {{"", "6", "3"}, 0.45},
        {{"", "6", "4"}, 0.47},
        {{"", "6", "5"}, 0.48},
        {{"", "6", "7"}, 0.31},
        {{"", "6", "8"}, 0.32},
        {{"", "6", "9"}, 0.38},
        {{"", "6", "10"}, 0.36},
        {{"", "6", "11"}, 0.37},
        {{"", "6", "12"}, 0.32},
        {{"", "7", "1"}, 0.39},
        {{"", "7", "2"}, 0.32},
        {{"", "7", "3"}, 0.33},
        {{"", "7", "4"}, 0.33},
        {{"", "7", "5"}, 0.33},
        {{"", "7", "6"}, 0.31},
        {{"", "7", "8"}, 0.26},
        {{"", "7", "9"}, 0.31},
        {{"", "7", "10"}, 0.28},
        {{"", "7", "11"}, 0.27},
        {{"", "7", "12"}, 0.25},
        {{"", "8", "1"}, 0.32},
        {{"", "8", "2"}, 0.34},
        {{"", "8", "3"}, 0.33},
        {{"", "8", "4"}, 0.34},
        {{"", "8", "5"}, 0.34},
        {{"", "8", "6"}, 0.32},
        {{"", "8", "7"}, 0.26},
        {{"", "8", "9"}, 0.31},
        {{"", "8", "10"}, 0.28},
        {{"", "8", "11"}, 0.28},
        {{"", "8", "12"}, 0.25},
        {{"", "9", "1"}, 0.35},
        {{"", "9", "2"}, 0.39},
        {{"", "9", "3"}, 0.41},
        {{"", "9", "4"}, 0.41},
        {{"", "9", "5"}, 0.41},
        {{"", "9", "6"}, 0.38},
        {{"", "9", "7"}, 0.31},
        {{"", "9", "8"}, 0.31},
        {{"", "9", "10"}, 0.35},
        {{"", "9", "11"}, 0.34},
        {{"", "9", "12"}, 0.32},
        {{"", "10", "1"}, 0.33},
        {{"", "10", "2"}, 0.36},
        {{"", "10", "3"}, 0.38},
        {{"", "10", "4"}, 0.4},
        {{"", "10", "5"}, 0.39},
        {{"", "10", "6"}, 0.36},
        {{"", "10", "7"}, 0.28},
        {{"", "10", "8"}, 0.28},
        {{"", "10", "9"}, 0.35},
        {{"", "10", "11"}, 0.34},
        {{"", "10", "12"}, 0.29},
        {{"", "11", "1"}, 0.33},
        {{"", "11", "2"}, 0.36},
        {{"", "11", "3"}, 0.39},
        {{"", "11", "4"}, 0.39},
        {{"", "11", "5"}, 0.4},
        {{"", "11", "6"}, 0.37},
        {{"", "11", "7"}, 0.27},
        {{"", "11", "8"}, 0.28},
        {{"", "11", "9"}, 0.34},
        {{"", "11", "10"}, 0.34},
        {{"", "11", "12"}, 0.3},
        {{"", "12", "1"}, 0.3},
        {{"", "12", "2"}, 0.31},
        {{"", "12", "3"}, 0.35},
        {{"", "12", "4"}, 0.34},
        {{"", "12", "5"}, 0.34},
        {{"", "12", "6"}, 0.32},
        {{"", "12", "7"}, 0.25},
        {{"", "12", "8"}, 0.25},
        {{"", "12", "9"}, 0.32},
        {{"", "12", "10"}, 0.29},
        {{"", "12", "11"}, 0.3}
    };

    // Equity inter-bucket correlations
    interBucketCorrelation_[RiskType::Equity] = {
        {{"", "1", "2"}, 0.17},
        {{"", "1", "3"}, 0.18},
        {{"", "1", "4"}, 0.17},
        {{"", "1", "5"}, 0.11},
        {{"", "1", "6"}, 0.15},
        {{"", "1", "7"}, 0.15},
        {{"", "1", "8"}, 0.15},
        {{"", "1", "9"}, 0.15},
        {{"", "1", "10"}, 0.11},
        {{"", "1", "11"}, 0.19},
        {{"", "1", "12"}, 0.19},
        {{"", "2", "1"}, 0.17},
        {{"", "2", "3"}, 0.23},
        {{"", "2", "4"}, 0.21},
        {{"", "2", "5"}, 0.13},
        {{"", "2", "6"}, 0.17},
        {{"", "2", "7"}, 0.18},
        {{"", "2", "8"}, 0.17},
        {{"", "2", "9"}, 0.19},
        {{"", "2", "10"}, 0.13},
        {{"", "2", "11"}, 0.22},
        {{"", "2", "12"}, 0.22},
        {{"", "3", "1"}, 0.18},
        {{"", "3", "2"}, 0.23},
        {{"", "3", "4"}, 0.24},
        {{"", "3", "5"}, 0.14},
        {{"", "3", "6"}, 0.19},
        {{"", "3", "7"}, 0.22},
        {{"", "3", "8"}, 0.19},
        {{"", "3", "9"}, 0.2},
        {{"", "3", "10"}, 0.15},
        {{"", "3", "11"}, 0.25},
        {{"", "3", "12"}, 0.25},
        {{"", "4", "1"}, 0.17},
        {{"", "4", "2"}, 0.21},
        {{"", "4", "3"}, 0.24},
        {{"", "4", "5"}, 0.14},
        {{"", "4", "6"}, 0.19},
        {{"", "4", "7"}, 0.2},
        {{"", "4", "8"}, 0.19},
        {{"", "4", "9"}, 0.2},
        {{"", "4", "10"}, 0.15},
        {{"", "4", "11"}, 0.24},
        {{"", "4", "12"}, 0.24},
        {{"", "5", "1"}, 0.11},
        {{"", "5", "2"}, 0.13},
        {{"", "5", "3"}, 0.14},
        {{"", "5", "4"}, 0.14},
        {{"", "5", "6"}, 0.22},
        {{"", "5", "7"}, 0.21},
        {{"", "5", "8"}, 0.22},
        {{"", "5", "9"}, 0.13},
        {{"", "5", "10"}, 0.16},
        {{"", "5", "11"}, 0.24},
        {{"", "5", "12"}, 0.24},
        {{"", "6", "1"}, 0.15},
        {{"", "6", "2"}, 0.17},
        {{"", "6", "3"}, 0.19},
        {{"", "6", "4"}, 0.19},
        {{"", "6", "5"}, 0.22},
        {{"", "6", "7"}, 0.29},
        {{"", "6", "8"}, 0.29},
        {{"", "6", "9"}, 0.17},
        {{"", "6", "10"}, 0.21},
        {{"", "6", "11"}, 0.32},
        {{"", "6", "12"}, 0.32},
        {{"", "7", "1"}, 0.15},
        {{"", "7", "2"}, 0.18},
        {{"", "7", "3"}, 0.22},
        {{"", "7", "4"}, 0.2},
        {{"", "7", "5"}, 0.21},
        {{"", "7", "6"}, 0.29},
        {{"", "7", "8"}, 0.28},
        {{"", "7", "9"}, 0.17},
        {{"", "7", "10"}, 0.21},
        {{"", "7", "11"}, 0.34},
        {{"", "7", "12"}, 0.34},
        {{"", "8", "1"}, 0.15},
        {{"", "8", "2"}, 0.17},
        {{"", "8", "3"}, 0.19},
        {{"", "8", "4"}, 0.19},
        {{"", "8", "5"}, 0.22},
        {{"", "8", "6"}, 0.29},
        {{"", "8", "7"}, 0.28},
        {{"", "8", "9"}, 0.17},
        {{"", "8", "10"}, 0.21},
        {{"", "8", "11"}, 0.33},
        {{"", "8", "12"}, 0.33},
        {{"", "9", "1"}, 0.15},
        {{"", "9", "2"}, 0.19},
        {{"", "9", "3"}, 0.2},
        {{"", "9", "4"}, 0.2},
        {{"", "9", "5"}, 0.13},
        {{"", "9", "6"}, 0.17},
        {{"", "9", "7"}, 0.17},
        {{"", "9", "8"}, 0.17},
        {{"", "9", "10"}, 0.13},
        {{"", "9", "11"}, 0.21},
        {{"", "9", "12"}, 0.21},
        {{"", "10", "1"}, 0.11},
        {{"", "10", "2"}, 0.13},
        {{"", "10", "3"}, 0.15},
        {{"", "10", "4"}, 0.15},
        {{"", "10", "5"}, 0.16},
        {{"", "10", "6"}, 0.21},
        {{"", "10", "7"}, 0.21},
        {{"", "10", "8"}, 0.21},
        {{"", "10", "9"}, 0.13},
        {{"", "10", "11"}, 0.22},
        {{"", "10", "12"}, 0.22},
        {{"", "11", "1"}, 0.19},
        {{"", "11", "2"}, 0.22},
        {{"", "11", "3"}, 0.25},
        {{"", "11", "4"}, 0.24},
        {{"", "11", "5"}, 0.24},
        {{"", "11", "6"}, 0.32},
        {{"", "11", "7"}, 0.34},
        {{"", "11", "8"}, 0.33},
        {{"", "11", "9"}, 0.21},
        {{"", "11", "10"}, 0.22},
        {{"", "11", "12"}, 0.41},
        {{"", "12", "1"}, 0.19},
        {{"", "12", "2"}, 0.22},
        {{"", "12", "3"}, 0.25},
        {{"", "12", "4"}, 0.24},
        {{"", "12", "5"}, 0.24},
        {{"", "12", "6"}, 0.32},
        {{"", "12", "7"}, 0.34},
        {{"", "12", "8"}, 0.33},
        {{"", "12", "9"}, 0.21},
        {{"", "12", "10"}, 0.22},
        {{"", "12", "11"}, 0.41}
    };

    // Commodity inter-bucket correlations
    interBucketCorrelation_[RiskType::Commodity] = {
        {{"", "1", "2"}, 0.15},
        {{"", "1", "3"}, 0.15},
        {{"", "1", "4"}, 0.21},
        {{"", "1", "5"}, 0.16},
        {{"", "1", "6"}, 0.03},
        {{"", "1", "7"}, 0.11},
        {{"", "1", "8"}, 0.02},
        {{"", "1", "9"}, 0.12},
        {{"", "1", "10"}, 0.15},
        {{"", "1", "11"}, 0.15},
        {{"", "1", "12"}, 0.06},
        {{"", "1", "13"}, 0.0},
        {{"", "1", "14"}, 0.04},
        {{"", "1", "15"}, 0.06},
        {{"", "1", "16"}, 0.0},
        {{"", "1", "17"}, 0.11},
        {{"", "2", "1"}, 0.15},
        {{"", "2", "3"}, 0.74},
        {{"", "2", "4"}, 0.92},
        {{"", "2", "5"}, 0.89},
        {{"", "2", "6"}, 0.34},
        {{"", "2", "7"}, 0.23},
        {{"", "2", "8"}, 0.16},
        {{"", "2", "9"}, 0.22},
        {{"", "2", "10"}, 0.26},
        {{"", "2", "11"}, 0.31},
        {{"", "2", "12"}, 0.32},
        {{"", "2", "13"}, 0.22},
        {{"", "2", "14"}, 0.25},
        {{"", "2", "15"}, 0.19},
        {{"", "2", "16"}, 0.0},
        {{"", "2", "17"}, 0.57},
        {{"", "3", "1"}, 0.15},
        {{"", "3", "2"}, 0.74},
        {{"", "3", "4"}, 0.73},
        {{"", "3", "5"}, 0.69},
        {{"", "3", "6"}, 0.15},
        {{"", "3", "7"}, 0.22},
        {{"", "3", "8"}, 0.08},
        {{"", "3", "9"}, 0.14},
        {{"", "3", "10"}, 0.16},
        {{"", "3", "11"}, 0.21},
        {{"", "3", "12"}, 0.15},
        {{"", "3", "13"}, -0.03},
        {{"", "3", "14"}, 0.16},
        {{"", "3", "15"}, 0.14},
        {{"", "3", "16"}, 0.0},
        {{"", "3", "17"}, 0.42},
        {{"", "4", "1"}, 0.21},
        {{"", "4", "2"}, 0.92},
        {{"", "4", "3"}, 0.73},
        {{"", "4", "5"}, 0.83},
        {{"", "4", "6"}, 0.3},
        {{"", "4", "7"}, 0.26},
        {{"", "4", "8"}, 0.07},
        {{"", "4", "9"}, 0.19},
        {{"", "4", "10"}, 0.22},
        {{"", "4", "11"}, 0.28},
        {{"", "4", "12"}, 0.31},
        {{"", "4", "13"}, 0.13},
        {{"", "4", "14"}, 0.22},
        {{"", "4", "15"}, 0.11},
        {{"", "4", "16"}, 0.0},
        {{"", "4", "17"}, 0.48},
        {{"", "5", "1"}, 0.16},
        {{"", "5", "2"}, 0.89},
        {{"", "5", "3"}, 0.69},
        {{"", "5", "4"}, 0.83},
        {{"", "5", "6"}, 0.12},
        {{"", "5", "7"}, 0.14},
        {{"", "5", "8"}, 0.0},
        {{"", "5", "9"}, 0.06},
        {{"", "5", "10"}, 0.1},
        {{"", "5", "11"}, 0.24},
        {{"", "5", "12"}, 0.2},
        {{"", "5", "13"}, 0.06},
        {{"", "5", "14"}, 0.2},
        {{"", "5", "15"}, 0.09},
        {{"", "5", "16"}, 0.0},
        {{"", "5", "17"}, 0.49},
        {{"", "6", "1"}, 0.03},
        {{"", "6", "2"}, 0.34},
        {{"", "6", "3"}, 0.15},
        {{"", "6", "4"}, 0.3},
        {{"", "6", "5"}, 0.12},
        {{"", "6", "7"}, 0.25},
        {{"", "6", "8"}, 0.58},
        {{"", "6", "9"}, 0.21},
        {{"", "6", "10"}, 0.14},
        {{"", "6", "11"}, 0.23},
        {{"", "6", "12"}, 0.15},
        {{"", "6", "13"}, 0.25},
        {{"", "6", "14"}, 0.15},
        {{"", "6", "15"}, 0.12},
        {{"", "6", "16"}, 0.0},
        {{"", "6", "17"}, 0.37},
        {{"", "7", "1"}, 0.11},
        {{"", "7", "2"}, 0.23},
        {{"", "7", "3"}, 0.22},
        {{"", "7", "4"}, 0.26},
        {{"", "7", "5"}, 0.14},
        {{"", "7", "6"}, 0.25},
        {{"", "7", "8"}, 0.19},
        {{"", "7", "9"}, 0.64},
        {{"", "7", "10"}, 0.19},
        {{"", "7", "11"}, 0.03},
        {{"", "7", "12"}, -0.03},
        {{"", "7", "13"}, 0.04},
        {{"", "7", "14"}, 0.05},
        {{"", "7", "15"}, 0.06},
        {{"", "7", "16"}, 0.0},
        {{"", "7", "17"}, 0.17},
        {{"", "8", "1"}, 0.02},
        {{"", "8", "2"}, 0.16},
        {{"", "8", "3"}, 0.08},
        {{"", "8", "4"}, 0.07},
        {{"", "8", "5"}, 0.0},
        {{"", "8", "6"}, 0.58},
        {{"", "8", "7"}, 0.19},
        {{"", "8", "9"}, 0.17},
        {{"", "8", "10"}, -0.01},
        {{"", "8", "11"}, 0.08},
        {{"", "8", "12"}, 0.01},
        {{"", "8", "13"}, 0.11},
        {{"", "8", "14"}, 0.08},
        {{"", "8", "15"}, 0.08},
        {{"", "8", "16"}, 0.0},
        {{"", "8", "17"}, 0.15},
        {{"", "9", "1"}, 0.12},
        {{"", "9", "2"}, 0.22},
        {{"", "9", "3"}, 0.14},
        {{"", "9", "4"}, 0.19},
        {{"", "9", "5"}, 0.06},
        {{"", "9", "6"}, 0.21},
        {{"", "9", "7"}, 0.64},
        {{"", "9", "8"}, 0.17},
        {{"", "9", "10"}, 0.1},
        {{"", "9", "11"}, 0.05},
        {{"", "9", "12"}, -0.04},
        {{"", "9", "13"}, 0.05},
        {{"", "9", "14"}, 0.03},
        {{"", "9", "15"}, 0.05},
        {{"", "9", "16"}, 0.0},
        {{"", "9", "17"}, 0.17},
        {{"", "10", "1"}, 0.15},
        {{"", "10", "2"}, 0.26},
        {{"", "10", "3"}, 0.16},
        {{"", "10", "4"}, 0.22},
        {{"", "10", "5"}, 0.1},
        {{"", "10", "6"}, 0.14},
        {{"", "10", "7"}, 0.19},
        {{"", "10", "8"}, -0.01},
        {{"", "10", "9"}, 0.1},
        {{"", "10", "11"}, 0.12},
        {{"", "10", "12"}, 0.13},
        {{"", "10", "13"}, 0.12},
        {{"", "10", "14"}, 0.1},
        {{"", "10", "15"}, 0.12},
        {{"", "10", "16"}, 0.0},
        {{"", "10", "17"}, 0.17},
        {{"", "11", "1"}, 0.15},
        {{"", "11", "2"}, 0.31},
        {{"", "11", "3"}, 0.21},
        {{"", "11", "4"}, 0.28},
        {{"", "11", "5"}, 0.24},
        {{"", "11", "6"}, 0.23},
        {{"", "11", "7"}, 0.03},
        {{"", "11", "8"}, 0.08},
        {{"", "11", "9"}, 0.05},
        {{"", "11", "10"}, 0.12},
        {{"", "11", "12"}, 0.34},
        {{"", "11", "13"}, 0.23},
        {{"", "11", "14"}, 0.17},
        {{"", "11", "15"}, 0.14},
        {{"", "11", "16"}, 0.0},
        {{"", "11", "17"}, 0.3},
        {{"", "12", "1"}, 0.06},
        {{"", "12", "2"}, 0.32},
        {{"", "12", "3"}, 0.15},
        {{"", "12", "4"}, 0.31},
        {{"", "12", "5"}, 0.2},
        {{"", "12", "6"}, 0.15},
        {{"", "12", "7"}, -0.03},
        {{"", "12", "8"}, 0.01},
        {{"", "12", "9"}, -0.04},
        {{"", "12", "10"}, 0.13},
        {{"", "12", "11"}, 0.34},
        {{"", "12", "13"}, 0.15},
        {{"", "12", "14"}, 0.19},
        {{"", "12", "15"}, 0.11},
        {{"", "12", "16"}, 0.0},
        {{"", "12", "17"}, 0.27},
        {{"", "13", "1"}, 0.0},
        {{"", "13", "2"}, 0.22},
        {{"", "13", "3"}, -0.03},
        {{"", "13", "4"}, 0.13},
        {{"", "13", "5"}, 0.06},
        {{"", "13", "6"}, 0.25},
        {{"", "13", "7"}, 0.04},
        {{"", "13", "8"}, 0.11},
        {{"", "13", "9"}, 0.05},
        {{"", "13", "10"}, 0.12},
        {{"", "13", "11"}, 0.23},
        {{"", "13", "12"}, 0.15},
        {{"", "13", "14"}, 0.26},
        {{"", "13", "15"}, 0.14},
        {{"", "13", "16"}, 0.0},
        {{"", "13", "17"}, 0.26},
        {{"", "14", "1"}, 0.04},
        {{"", "14", "2"}, 0.25},
        {{"", "14", "3"}, 0.16},
        {{"", "14", "4"}, 0.22},
        {{"", "14", "5"}, 0.2},
        {{"", "14", "6"}, 0.15},
        {{"", "14", "7"}, 0.05},
        {{"", "14", "8"}, 0.08},
        {{"", "14", "9"}, 0.03},
        {{"", "14", "10"}, 0.1},
        {{"", "14", "11"}, 0.17},
        {{"", "14", "12"}, 0.19},
        {{"", "14", "13"}, 0.26},
        {{"", "14", "15"}, 0.1},
        {{"", "14", "16"}, 0.0},
        {{"", "14", "17"}, 0.22},
        {{"", "15", "1"}, 0.06},
        {{"", "15", "2"}, 0.19},
        {{"", "15", "3"}, 0.14},
        {{"", "15", "4"}, 0.11},
        {{"", "15", "5"}, 0.09},
        {{"", "15", "6"}, 0.12},
        {{"", "15", "7"}, 0.06},
        {{"", "15", "8"}, 0.08},
        {{"", "15", "9"}, 0.05},
        {{"", "15", "10"}, 0.12},
        {{"", "15", "11"}, 0.14},
        {{"", "15", "12"}, 0.11},
        {{"", "15", "13"}, 0.14},
        {{"", "15", "14"}, 0.1},
        {{"", "15", "16"}, 0.0},
        {{"", "15", "17"}, 0.2},
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
        {{"", "17", "1"}, 0.11},
        {{"", "17", "2"}, 0.57},
        {{"", "17", "3"}, 0.42},
        {{"", "17", "4"}, 0.48},
        {{"", "17", "5"}, 0.49},
        {{"", "17", "6"}, 0.37},
        {{"", "17", "7"}, 0.17},
        {{"", "17", "8"}, 0.15},
        {{"", "17", "9"}, 0.17},
        {{"", "17", "10"}, 0.17},
        {{"", "17", "11"}, 0.3},
        {{"", "17", "12"}, 0.27},
        {{"", "17", "13"}, 0.26},
        {{"", "17", "14"}, 0.22},
        {{"", "17", "15"}, 0.2},
        {{"", "17", "16"}, 0.0}
    };

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[RiskType::Equity] = {
        {{"1", "", ""}, 0.14},
        {{"2", "", ""}, 0.20},
        {{"3", "", ""}, 0.28},
        {{"4", "", ""}, 0.23},
        {{"5", "", ""}, 0.18},
        {{"6", "", ""}, 0.29},
        {{"7", "", ""}, 0.34},
        {{"8", "", ""}, 0.30},
        {{"9", "", ""}, 0.19},
        {{"10", "", ""}, 0.17},
        {{"11", "", ""}, 0.41},
        {{"12", "", ""}, 0.41}
    };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[RiskType::Commodity] = {
        {{"1", "", ""}, 0.16},
        {{"2", "", ""}, 0.98},
        {{"3", "", ""}, 0.77},
        {{"4", "", ""}, 0.82},
        {{"5", "", ""}, 0.98},
        {{"6", "", ""}, 0.89},
        {{"7", "", ""}, 0.96},
        {{"8", "", ""}, 0.48},
        {{"9", "", ""}, 0.64}, 
        {{"10", "", ""}, 0.39},
        {{"11", "", ""}, 0.45},
        {{"12", "", ""}, 0.53},
        {{"13", "", ""}, 0.65},
        {{"14", "", ""}, 0.12},
        {{"15", "", ""}, 0.21},
        {{"16", "", ""}, 0.00},
        {{"17", "", ""}, 0.35}};

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

string SimmConfiguration_ISDA_V2_3::label2(const QuantLib::ext::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::label2(irIndex);
}

} // namespace analytics
} // namespace ore
