/*
 Copyright (C) 2022 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav2_5.hpp>
#include <orea/simm/simmconfigurationisdav2_5.hpp>
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

QuantLib::Size SimmConfiguration_ISDA_V2_5::group(const string& qualifier, const std::map<QuantLib::Size,
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

QuantLib::Real SimmConfiguration_ISDA_V2_5::weight(const RiskType& rt, boost::optional<string> qualifier,
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

QuantLib::Real SimmConfiguration_ISDA_V2_5::correlation(const RiskType& firstRt, const string& firstQualifier,
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

SimmConfiguration_ISDA_V2_5::SimmConfiguration_ISDA_V2_5(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const QuantLib::Size& mporDays, const std::string& name,
                                                         const std::string version)
     : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentration_ISDA_V2_5>(simmBucketMapper_);
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
    // The groups consists of High Vol Currencies & regular vol currencies
    ccyGroups_ = {
        { 1, { "BRL", "RUB", "TRY", "ZAR" } },
        { 0, {  } }
    };

    vector<Real> temp;

    if (mporDays_ == 10) {
        // Risk weights
        temp = {
           7.4,  13.6,
           13.6, 14.6
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 63 },
            { RiskType::XCcyBasis, 21 },
            { RiskType::IRVol, 0.18 },
            { RiskType::InflationVol, 0.18 },
            { RiskType::CreditVol, 0.74 },
            { RiskType::CreditVolNonQ, 0.74 },
            { RiskType::CommodityVol, 0.6 },
            { RiskType::FXVol, 0.47 },
            { RiskType::BaseCorr, 10 }
        };

        rwBucket_ = {
            {RiskType::CreditQ,
             {{{"1", "", ""}, 75},
              {{"2", "", ""}, 91},
              {{"3", "", ""}, 78},
              {{"4", "", ""}, 55},
              {{"5", "", ""}, 67},
              {{"6", "", ""}, 47},
              {{"7", "", ""}, 187},
              {{"8", "", ""}, 665},
              {{"9", "", ""}, 262},
              {{"10", "", ""}, 251},
              {{"11", "", ""}, 172},
              {{"12", "", ""}, 247},
              {{"Residual", "", ""}, 665}}},
            {RiskType::CreditNonQ, {{{"1", "", ""}, 280}, {{"2", "", ""}, 1300}, {{"Residual", "", ""}, 1300}}},
            {RiskType::Equity,
             {{{"1", "", ""}, 26},
              {{"2", "", ""}, 28},
              {{"3", "", ""}, 34},
              {{"4", "", ""}, 28},
              {{"5", "", ""}, 23},
              {{"6", "", ""}, 25},
              {{"7", "", ""}, 29},
              {{"8", "", ""}, 27},
              {{"9", "", ""}, 32},
              {{"10", "", ""}, 32},
              {{"11", "", ""}, 18},
              {{"12", "", ""}, 18},
              {{"Residual", "", ""}, 34}}},
            {RiskType::Commodity,
             {{{"1", "", ""}, 27},
              {{"2", "", ""}, 29},
              {{"3", "", ""}, 33},
              {{"4", "", ""}, 25},
              {{"5", "", ""}, 35},
              {{"6", "", ""}, 24},
              {{"7", "", ""}, 40},
              {{"8", "", ""}, 53},
              {{"9", "", ""}, 44},
              {{"10", "", ""}, 58},
              {{"11", "", ""}, 20},
              {{"12", "", ""}, 21},
              {{"13", "", ""}, 13},
              {{"14", "", ""}, 16},
              {{"15", "", ""}, 13},
              {{"16", "", ""}, 58},
              {{"17", "", ""}, 17}}},
            {RiskType::EquityVol,
             {{{"1", "", ""}, 0.45},
              {{"2", "", ""}, 0.45},
              {{"3", "", ""}, 0.45},
              {{"4", "", ""}, 0.45},
              {{"5", "", ""}, 0.45},
              {{"6", "", ""}, 0.45},
              {{"7", "", ""}, 0.45},
              {{"8", "", ""}, 0.45},
              {{"9", "", ""}, 0.45},
              {{"10", "", ""}, 0.45},
              {{"11", "", ""}, 0.45},
              {{"12", "", ""}, 0.96},
              {{"Residual", "", ""}, 0.45}}},
        };

        rwLabel_1_ = {
            {RiskType::IRCurve,
             {{{"1", "2w", ""}, 115},
              {{"1", "1m", ""}, 112},
              {{"1", "3m", ""}, 96},
              {{"1", "6m", ""}, 74},
              {{"1", "1y", ""}, 66},
              {{"1", "2y", ""}, 61},
              {{"1", "3y", ""}, 56},
              {{"1", "5y", ""}, 52},
              {{"1", "10y", ""}, 53},
              {{"1", "15y", ""}, 57},
              {{"1", "20y", ""}, 60},
              {{"1", "30y", ""}, 66},
              {{"2", "2w", ""}, 15},
              {{"2", "1m", ""}, 18},
              {{"2", "3m", ""}, 9.0},
              {{"2", "6m", ""}, 11},
              {{"2", "1y", ""}, 13},
              {{"2", "2y", ""}, 15},
              {{"2", "3y", ""}, 18},
              {{"2", "5y", ""}, 20},
              {{"2", "10y", ""}, 19},
              {{"2", "15y", ""}, 19},
              {{"2", "20y", ""}, 20},
              {{"2", "30y", ""}, 23},
              {{"3", "2w", ""}, 119},
              {{"3", "1m", ""}, 93},
              {{"3", "3m", ""}, 80},
              {{"3", "6m", ""}, 82},
              {{"3", "1y", ""}, 90},
              {{"3", "2y", ""}, 92},
              {{"3", "3y", ""}, 95},
              {{"3", "5y", ""}, 95},
              {{"3", "10y", ""}, 94},
              {{"3", "15y", ""}, 108},
              {{"3", "20y", ""}, 105},
              {{"3", "30y", ""}, 101}}},
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.58;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.69;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.52;
        hvr_ir_ = 0.44;

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
            1.8,  3.2, 
           3.2, 3.4
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 15 },
            { RiskType::XCcyBasis, 5.9 },
            { RiskType::IRVol, 0.047 },
            { RiskType::InflationVol, 0.047 },
            { RiskType::CreditVol, 0.085 },
            { RiskType::CreditVolNonQ, 0.085 },
            { RiskType::CommodityVol, 0.16 },
            { RiskType::FXVol, 0.096 },
            { RiskType::BaseCorr, 2.5 }
        };

        rwBucket_ = {
            {RiskType::CreditQ,
             {{{"1", "", ""}, 21},
              {{"2", "", ""}, 27},
              {{"3", "", ""}, 16},
              {{"4", "", ""}, 12},
              {{"5", "", ""}, 14},
              {{"6", "", ""}, 12},
              {{"7", "", ""}, 48},
              {{"8", "", ""}, 144},
              {{"9", "", ""}, 51},
              {{"10", "", ""}, 53},
              {{"11", "", ""}, 38},
              {{"12", "", ""}, 57},
              {{"Residual", "", ""}, 144}}},
            {RiskType::CreditNonQ, {{{"1", "", ""}, 66}, {{"2", "", ""}, 250}, {{"Residual", "", ""}, 250}}},
            {RiskType::Equity,
             {{{"1", "", ""}, 9.3},
              {{"2", "", ""}, 9.7},
              {{"3", "", ""}, 10.0},
              {{"4", "", ""}, 9.2},
              {{"5", "", ""}, 7.7},
              {{"6", "", ""}, 8.5},
              {{"7", "", ""}, 9.5},
              {{"8", "", ""}, 9.6},
              {{"9", "", ""}, 10.0},
              {{"10", "", ""}, 10},
              {{"11", "", ""}, 5.9},
              {{"12", "", ""}, 5.9},
              {{"Residual", "", ""}, 10.0}}},
            {RiskType::Commodity,
             {{{"1", "", ""}, 9.0},
              {{"2", "", ""}, 9.1},
              {{"3", "", ""}, 8.1},
              {{"4", "", ""}, 7.2},
              {{"5", "", ""}, 10},
              {{"6", "", ""}, 8.2},
              {{"7", "", ""}, 9.7},
              {{"8", "", ""}, 10},
              {{"9", "", ""}, 10},
              {{"10", "", ""}, 16},
              {{"11", "", ""}, 6.2},
              {{"12", "", ""}, 6.5},
              {{"13", "", ""}, 4.6},
              {{"14", "", ""}, 4.6},
              {{"15", "", ""}, 4.0},
              {{"16", "", ""}, 16},
              {{"17", "", ""}, 5.1}}},
            {RiskType::EquityVol,
             {{{"1", "", ""}, 0.093},
              {{"2", "", ""}, 0.093},
              {{"3", "", ""}, 0.093},
              {{"4", "", ""}, 0.093},
              {{"5", "", ""}, 0.093},
              {{"6", "", ""}, 0.093},
              {{"7", "", ""}, 0.093},
              {{"8", "", ""}, 0.093},
              {{"9", "", ""}, 0.093},
              {{"10", "", ""}, 0.093},
              {{"11", "", ""}, 0.093},
              {{"12", "", ""}, 0.25},
              {{"Residual", "", ""}, 0.093}}},
        };

        rwLabel_1_ = {
            {RiskType::IRCurve,
             {{{"1", "2w", ""}, 19},
              {{"1", "1m", ""}, 16},
              {{"1", "3m", ""}, 12},
              {{"1", "6m", ""}, 12},
              {{"1", "1y", ""}, 13},
              {{"1", "2y", ""}, 16},
              {{"1", "3y", ""}, 16},
              {{"1", "5y", ""}, 16},
              {{"1", "10y", ""}, 16},
              {{"1", "15y", ""}, 17},
              {{"1", "20y", ""}, 16},
              {{"1", "30y", ""}, 17},
              {{"2", "2w", ""}, 1.7},
              {{"2", "1m", ""}, 3.4},
              {{"2", "3m", ""}, 1.8},
              {{"2", "6m", ""}, 2.0},
              {{"2", "1y", ""}, 3.3},
              {{"2", "2y", ""}, 4.8},
              {{"2", "3y", ""}, 5.8},
              {{"2", "5y", ""}, 6.8},
              {{"2", "10y", ""}, 6.5},
              {{"2", "15y", ""}, 7.0},
              {{"2", "20y", ""}, 7.5},
              {{"2", "30y", ""}, 8.3},
              {{"3", "2w", ""}, 49},
              {{"3", "1m", ""}, 24},
              {{"3", "3m", ""}, 16},
              {{"3", "6m", ""}, 20},
              {{"3", "1y", ""}, 23},
              {{"3", "2y", ""}, 23},
              {{"3", "3y", ""}, 33},
              {{"3", "5y", ""}, 31},
              {{"3", "10y", ""}, 34},
              {{"3", "15y", ""}, 33},
              {{"3", "20y", ""}, 33},
              {{"3", "30y", ""}, 27}}}
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.54;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.69;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.7;
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
    riskClassCorrelation_ = {
        {{"", "InterestRate", "CreditQualifying"}, 0.29},
        {{"", "InterestRate", "CreditNonQualifying"}, 0.13},
        {{"", "InterestRate", "Equity"}, 0.28},
        {{"", "InterestRate", "Commodity"}, 0.46},
        {{"", "InterestRate", "FX"}, 0.32},
        {{"", "CreditQualifying", "InterestRate"}, 0.29},
        {{"", "CreditQualifying", "CreditNonQualifying"}, 0.54},
        {{"", "CreditQualifying", "Equity"}, 0.71},
        {{"", "CreditQualifying", "Commodity"}, 0.52},
        {{"", "CreditQualifying", "FX"}, 0.38},
        {{"", "CreditNonQualifying", "InterestRate"}, 0.13},
        {{"", "CreditNonQualifying", "CreditQualifying"}, 0.54},
        {{"", "CreditNonQualifying", "Equity"}, 0.46},
        {{"", "CreditNonQualifying", "Commodity"}, 0.41},
        {{"", "CreditNonQualifying", "FX"}, 0.12},
        {{"", "Equity", "InterestRate"}, 0.28},
        {{"", "Equity", "CreditQualifying"}, 0.71},
        {{"", "Equity", "CreditNonQualifying"}, 0.46},
        {{"", "Equity", "Commodity"}, 0.49},
        {{"", "Equity", "FX"}, 0.35},
        {{"", "Commodity", "InterestRate"}, 0.46},
        {{"", "Commodity", "CreditQualifying"}, 0.52},
        {{"", "Commodity", "CreditNonQualifying"}, 0.41},
        {{"", "Commodity", "Equity"}, 0.49},
        {{"", "Commodity", "FX"}, 0.41},
        {{"", "FX", "InterestRate"}, 0.32},
        {{"", "FX", "CreditQualifying"}, 0.38},
        {{"", "FX", "CreditNonQualifying"}, 0.12},
        {{"", "FX", "Equity"}, 0.35},
        {{"", "FX", "Commodity"}, 0.41}
    };

    // FX correlations
    temp = {
        0.5, 0.27,
        0.27, 0.42
    };
    fxRegVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    temp = {
        0.85, 0.54,
        0.54, 0.5
    };
    fxHighVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations)
    intraBucketCorrelation_[RiskType::IRCurve] = {
        {{"", "2w", "1m"}, 0.74},
        {{"", "2w", "3m"}, 0.63},
        {{"", "2w", "6m"}, 0.55},
        {{"", "2w", "1y"}, 0.45},
        {{"", "2w", "2y"}, 0.36},
        {{"", "2w", "3y"}, 0.32},
        {{"", "2w", "5y"}, 0.28},
        {{"", "2w", "10y"}, 0.23},
        {{"", "2w", "15y"}, 0.2},
        {{"", "2w", "20y"}, 0.18},
        {{"", "2w", "30y"}, 0.16},
        {{"", "1m", "2w"}, 0.74},
        {{"", "1m", "3m"}, 0.8},
        {{"", "1m", "6m"}, 0.69},
        {{"", "1m", "1y"}, 0.52},
        {{"", "1m", "2y"}, 0.41},
        {{"", "1m", "3y"}, 0.35},
        {{"", "1m", "5y"}, 0.29},
        {{"", "1m", "10y"}, 0.24},
        {{"", "1m", "15y"}, 0.18},
        {{"", "1m", "20y"}, 0.17},
        {{"", "1m", "30y"}, 0.16},
        {{"", "3m", "2w"}, 0.63},
        {{"", "3m", "1m"}, 0.8},
        {{"", "3m", "6m"}, 0.85},
        {{"", "3m", "1y"}, 0.67},
        {{"", "3m", "2y"}, 0.53},
        {{"", "3m", "3y"}, 0.45},
        {{"", "3m", "5y"}, 0.39},
        {{"", "3m", "10y"}, 0.32},
        {{"", "3m", "15y"}, 0.24},
        {{"", "3m", "20y"}, 0.22},
        {{"", "3m", "30y"}, 0.22},
        {{"", "6m", "2w"}, 0.55},
        {{"", "6m", "1m"}, 0.69},
        {{"", "6m", "3m"}, 0.85},
        {{"", "6m", "1y"}, 0.83},
        {{"", "6m", "2y"}, 0.71},
        {{"", "6m", "3y"}, 0.62},
        {{"", "6m", "5y"}, 0.54},
        {{"", "6m", "10y"}, 0.45},
        {{"", "6m", "15y"}, 0.36},
        {{"", "6m", "20y"}, 0.35},
        {{"", "6m", "30y"}, 0.33},
        {{"", "1y", "2w"}, 0.45},
        {{"", "1y", "1m"}, 0.52},
        {{"", "1y", "3m"}, 0.67},
        {{"", "1y", "6m"}, 0.83},
        {{"", "1y", "2y"}, 0.94},
        {{"", "1y", "3y"}, 0.86},
        {{"", "1y", "5y"}, 0.78},
        {{"", "1y", "10y"}, 0.65},
        {{"", "1y", "15y"}, 0.58},
        {{"", "1y", "20y"}, 0.55},
        {{"", "1y", "30y"}, 0.53},
        {{"", "2y", "2w"}, 0.36},
        {{"", "2y", "1m"}, 0.41},
        {{"", "2y", "3m"}, 0.53},
        {{"", "2y", "6m"}, 0.71},
        {{"", "2y", "1y"}, 0.94},
        {{"", "2y", "3y"}, 0.95},
        {{"", "2y", "5y"}, 0.89},
        {{"", "2y", "10y"}, 0.78},
        {{"", "2y", "15y"}, 0.72},
        {{"", "2y", "20y"}, 0.68},
        {{"", "2y", "30y"}, 0.67},
        {{"", "3y", "2w"}, 0.32},
        {{"", "3y", "1m"}, 0.35},
        {{"", "3y", "3m"}, 0.45},
        {{"", "3y", "6m"}, 0.62},
        {{"", "3y", "1y"}, 0.86},
        {{"", "3y", "2y"}, 0.95},
        {{"", "3y", "5y"}, 0.96},
        {{"", "3y", "10y"}, 0.87},
        {{"", "3y", "15y"}, 0.8},
        {{"", "3y", "20y"}, 0.77},
        {{"", "3y", "30y"}, 0.74},
        {{"", "5y", "2w"}, 0.28},
        {{"", "5y", "1m"}, 0.29},
        {{"", "5y", "3m"}, 0.39},
        {{"", "5y", "6m"}, 0.54},
        {{"", "5y", "1y"}, 0.78},
        {{"", "5y", "2y"}, 0.89},
        {{"", "5y", "3y"}, 0.96},
        {{"", "5y", "10y"}, 0.94},
        {{"", "5y", "15y"}, 0.89},
        {{"", "5y", "20y"}, 0.86},
        {{"", "5y", "30y"}, 0.84},
        {{"", "10y", "2w"}, 0.23},
        {{"", "10y", "1m"}, 0.24},
        {{"", "10y", "3m"}, 0.32},
        {{"", "10y", "6m"}, 0.45},
        {{"", "10y", "1y"}, 0.65},
        {{"", "10y", "2y"}, 0.78},
        {{"", "10y", "3y"}, 0.87},
        {{"", "10y", "5y"}, 0.94},
        {{"", "10y", "15y"}, 0.97},
        {{"", "10y", "20y"}, 0.95},
        {{"", "10y", "30y"}, 0.94},
        {{"", "15y", "2w"}, 0.2},
        {{"", "15y", "1m"}, 0.18},
        {{"", "15y", "3m"}, 0.24},
        {{"", "15y", "6m"}, 0.36},
        {{"", "15y", "1y"}, 0.58},
        {{"", "15y", "2y"}, 0.72},
        {{"", "15y", "3y"}, 0.8},
        {{"", "15y", "5y"}, 0.89},
        {{"", "15y", "10y"}, 0.97},
        {{"", "15y", "20y"}, 0.98},
        {{"", "15y", "30y"}, 0.98},
        {{"", "20y", "2w"}, 0.18},
        {{"", "20y", "1m"}, 0.17},
        {{"", "20y", "3m"}, 0.22},
        {{"", "20y", "6m"}, 0.35},
        {{"", "20y", "1y"}, 0.55},
        {{"", "20y", "2y"}, 0.68},
        {{"", "20y", "3y"}, 0.77},
        {{"", "20y", "5y"}, 0.86},
        {{"", "20y", "10y"}, 0.95},
        {{"", "20y", "15y"}, 0.98},
        {{"", "20y", "30y"}, 0.99},
        {{"", "30y", "2w"}, 0.16},
        {{"", "30y", "1m"}, 0.16},
        {{"", "30y", "3m"}, 0.22},
        {{"", "30y", "6m"}, 0.33},
        {{"", "30y", "1y"}, 0.53},
        {{"", "30y", "2y"}, 0.67},
        {{"", "30y", "3y"}, 0.74},
        {{"", "30y", "5y"}, 0.84},
        {{"", "30y", "10y"}, 0.94},
        {{"", "30y", "15y"}, 0.98},
        {{"", "30y", "20y"}, 0.99}
    };

    // CreditQ inter-bucket correlations 
    interBucketCorrelation_[RiskType::CreditQ] = {
        {{"", "1", "2"}, 0.36},
        {{"", "1", "3"}, 0.38},
        {{"", "1", "4"}, 0.35},
        {{"", "1", "5"}, 0.37},
        {{"", "1", "6"}, 0.33},
        {{"", "1", "7"}, 0.36},
        {{"", "1", "8"}, 0.31},
        {{"", "1", "9"}, 0.32},
        {{"", "1", "10"}, 0.33},
        {{"", "1", "11"}, 0.32},
        {{"", "1", "12"}, 0.3},
        {{"", "2", "1"}, 0.36},
        {{"", "2", "3"}, 0.46},
        {{"", "2", "4"}, 0.44},
        {{"", "2", "5"}, 0.45},
        {{"", "2", "6"}, 0.43},
        {{"", "2", "7"}, 0.33},
        {{"", "2", "8"}, 0.36},
        {{"", "2", "9"}, 0.38},
        {{"", "2", "10"}, 0.39},
        {{"", "2", "11"}, 0.4},
        {{"", "2", "12"}, 0.36},
        {{"", "3", "1"}, 0.38},
        {{"", "3", "2"}, 0.46},
        {{"", "3", "4"}, 0.49},
        {{"", "3", "5"}, 0.49},
        {{"", "3", "6"}, 0.47},
        {{"", "3", "7"}, 0.34},
        {{"", "3", "8"}, 0.36},
        {{"", "3", "9"}, 0.41},
        {{"", "3", "10"}, 0.42},
        {{"", "3", "11"}, 0.43},
        {{"", "3", "12"}, 0.39},
        {{"", "4", "1"}, 0.35},
        {{"", "4", "2"}, 0.44},
        {{"", "4", "3"}, 0.49},
        {{"", "4", "5"}, 0.48},
        {{"", "4", "6"}, 0.48},
        {{"", "4", "7"}, 0.31},
        {{"", "4", "8"}, 0.34},
        {{"", "4", "9"}, 0.38},
        {{"", "4", "10"}, 0.42},
        {{"", "4", "11"}, 0.41},
        {{"", "4", "12"}, 0.37},
        {{"", "5", "1"}, 0.37},
        {{"", "5", "2"}, 0.45},
        {{"", "5", "3"}, 0.49},
        {{"", "5", "4"}, 0.48},
        {{"", "5", "6"}, 0.48},
        {{"", "5", "7"}, 0.33},
        {{"", "5", "8"}, 0.35},
        {{"", "5", "9"}, 0.39},
        {{"", "5", "10"}, 0.42},
        {{"", "5", "11"}, 0.43},
        {{"", "5", "12"}, 0.38},
        {{"", "6", "1"}, 0.33},
        {{"", "6", "2"}, 0.43},
        {{"", "6", "3"}, 0.47},
        {{"", "6", "4"}, 0.48},
        {{"", "6", "5"}, 0.48},
        {{"", "6", "7"}, 0.29},
        {{"", "6", "8"}, 0.32},
        {{"", "6", "9"}, 0.36},
        {{"", "6", "10"}, 0.39},
        {{"", "6", "11"}, 0.4},
        {{"", "6", "12"}, 0.35},
        {{"", "7", "1"}, 0.36},
        {{"", "7", "2"}, 0.33},
        {{"", "7", "3"}, 0.34},
        {{"", "7", "4"}, 0.31},
        {{"", "7", "5"}, 0.33},
        {{"", "7", "6"}, 0.29},
        {{"", "7", "8"}, 0.28},
        {{"", "7", "9"}, 0.32},
        {{"", "7", "10"}, 0.31},
        {{"", "7", "11"}, 0.3},
        {{"", "7", "12"}, 0.28},
        {{"", "8", "1"}, 0.31},
        {{"", "8", "2"}, 0.36},
        {{"", "8", "3"}, 0.36},
        {{"", "8", "4"}, 0.34},
        {{"", "8", "5"}, 0.35},
        {{"", "8", "6"}, 0.32},
        {{"", "8", "7"}, 0.28},
        {{"", "8", "9"}, 0.33},
        {{"", "8", "10"}, 0.34},
        {{"", "8", "11"}, 0.33},
        {{"", "8", "12"}, 0.3},
        {{"", "9", "1"}, 0.32},
        {{"", "9", "2"}, 0.38},
        {{"", "9", "3"}, 0.41},
        {{"", "9", "4"}, 0.38},
        {{"", "9", "5"}, 0.39},
        {{"", "9", "6"}, 0.36},
        {{"", "9", "7"}, 0.32},
        {{"", "9", "8"}, 0.33},
        {{"", "9", "10"}, 0.38},
        {{"", "9", "11"}, 0.36},
        {{"", "9", "12"}, 0.34},
        {{"", "10", "1"}, 0.33},
        {{"", "10", "2"}, 0.39},
        {{"", "10", "3"}, 0.42},
        {{"", "10", "4"}, 0.42},
        {{"", "10", "5"}, 0.42},
        {{"", "10", "6"}, 0.39},
        {{"", "10", "7"}, 0.31},
        {{"", "10", "8"}, 0.34},
        {{"", "10", "9"}, 0.38},
        {{"", "10", "11"}, 0.38},
        {{"", "10", "12"}, 0.36},
        {{"", "11", "1"}, 0.32},
        {{"", "11", "2"}, 0.4},
        {{"", "11", "3"}, 0.43},
        {{"", "11", "4"}, 0.41},
        {{"", "11", "5"}, 0.43},
        {{"", "11", "6"}, 0.4},
        {{"", "11", "7"}, 0.3},
        {{"", "11", "8"}, 0.33},
        {{"", "11", "9"}, 0.36},
        {{"", "11", "10"}, 0.38},
        {{"", "11", "12"}, 0.35},
        {{"", "12", "1"}, 0.3},
        {{"", "12", "2"}, 0.36},
        {{"", "12", "3"}, 0.39},
        {{"", "12", "4"}, 0.37},
        {{"", "12", "5"}, 0.38},
        {{"", "12", "6"}, 0.35},
        {{"", "12", "7"}, 0.28},
        {{"", "12", "8"}, 0.3},
        {{"", "12", "9"}, 0.34},
        {{"", "12", "10"}, 0.36},
        {{"", "12", "11"}, 0.35}
    };

    // Equity inter-bucket correlations
    interBucketCorrelation_[RiskType::Equity] = {
        {{"", "1", "2"}, 0.2},
        {{"", "1", "3"}, 0.2},
        {{"", "1", "4"}, 0.2},
        {{"", "1", "5"}, 0.13},
        {{"", "1", "6"}, 0.16},
        {{"", "1", "7"}, 0.16},
        {{"", "1", "8"}, 0.16},
        {{"", "1", "9"}, 0.17},
        {{"", "1", "10"}, 0.12},
        {{"", "1", "11"}, 0.18},
        {{"", "1", "12"}, 0.18},
        {{"", "2", "1"}, 0.2},
        {{"", "2", "3"}, 0.25},
        {{"", "2", "4"}, 0.23},
        {{"", "2", "5"}, 0.14},
        {{"", "2", "6"}, 0.17},
        {{"", "2", "7"}, 0.18},
        {{"", "2", "8"}, 0.17},
        {{"", "2", "9"}, 0.19},
        {{"", "2", "10"}, 0.13},
        {{"", "2", "11"}, 0.19},
        {{"", "2", "12"}, 0.19},
        {{"", "3", "1"}, 0.2},
        {{"", "3", "2"}, 0.25},
        {{"", "3", "4"}, 0.24},
        {{"", "3", "5"}, 0.13},
        {{"", "3", "6"}, 0.17},
        {{"", "3", "7"}, 0.18},
        {{"", "3", "8"}, 0.16},
        {{"", "3", "9"}, 0.2},
        {{"", "3", "10"}, 0.13},
        {{"", "3", "11"}, 0.18},
        {{"", "3", "12"}, 0.18},
        {{"", "4", "1"}, 0.2},
        {{"", "4", "2"}, 0.23},
        {{"", "4", "3"}, 0.24},
        {{"", "4", "5"}, 0.17},
        {{"", "4", "6"}, 0.22},
        {{"", "4", "7"}, 0.22},
        {{"", "4", "8"}, 0.22},
        {{"", "4", "9"}, 0.21},
        {{"", "4", "10"}, 0.16},
        {{"", "4", "11"}, 0.24},
        {{"", "4", "12"}, 0.24},
        {{"", "5", "1"}, 0.13},
        {{"", "5", "2"}, 0.14},
        {{"", "5", "3"}, 0.13},
        {{"", "5", "4"}, 0.17},
        {{"", "5", "6"}, 0.27},
        {{"", "5", "7"}, 0.26},
        {{"", "5", "8"}, 0.27},
        {{"", "5", "9"}, 0.15},
        {{"", "5", "10"}, 0.2},
        {{"", "5", "11"}, 0.3},
        {{"", "5", "12"}, 0.3},
        {{"", "6", "1"}, 0.16},
        {{"", "6", "2"}, 0.17},
        {{"", "6", "3"}, 0.17},
        {{"", "6", "4"}, 0.22},
        {{"", "6", "5"}, 0.27},
        {{"", "6", "7"}, 0.34},
        {{"", "6", "8"}, 0.33},
        {{"", "6", "9"}, 0.18},
        {{"", "6", "10"}, 0.24},
        {{"", "6", "11"}, 0.38},
        {{"", "6", "12"}, 0.38},
        {{"", "7", "1"}, 0.16},
        {{"", "7", "2"}, 0.18},
        {{"", "7", "3"}, 0.18},
        {{"", "7", "4"}, 0.22},
        {{"", "7", "5"}, 0.26},
        {{"", "7", "6"}, 0.34},
        {{"", "7", "8"}, 0.32},
        {{"", "7", "9"}, 0.18},
        {{"", "7", "10"}, 0.24},
        {{"", "7", "11"}, 0.37},
        {{"", "7", "12"}, 0.37},
        {{"", "8", "1"}, 0.16},
        {{"", "8", "2"}, 0.17},
        {{"", "8", "3"}, 0.16},
        {{"", "8", "4"}, 0.22},
        {{"", "8", "5"}, 0.27},
        {{"", "8", "6"}, 0.33},
        {{"", "8", "7"}, 0.32},
        {{"", "8", "9"}, 0.18},
        {{"", "8", "10"}, 0.23},
        {{"", "8", "11"}, 0.37},
        {{"", "8", "12"}, 0.37},
        {{"", "9", "1"}, 0.17},
        {{"", "9", "2"}, 0.19},
        {{"", "9", "3"}, 0.2},
        {{"", "9", "4"}, 0.21},
        {{"", "9", "5"}, 0.15},
        {{"", "9", "6"}, 0.18},
        {{"", "9", "7"}, 0.18},
        {{"", "9", "8"}, 0.18},
        {{"", "9", "10"}, 0.14},
        {{"", "9", "11"}, 0.2},
        {{"", "9", "12"}, 0.2},
        {{"", "10", "1"}, 0.12},
        {{"", "10", "2"}, 0.13},
        {{"", "10", "3"}, 0.13},
        {{"", "10", "4"}, 0.16},
        {{"", "10", "5"}, 0.2},
        {{"", "10", "6"}, 0.24},
        {{"", "10", "7"}, 0.24},
        {{"", "10", "8"}, 0.23},
        {{"", "10", "9"}, 0.14},
        {{"", "10", "11"}, 0.25},
        {{"", "10", "12"}, 0.25},
        {{"", "11", "1"}, 0.18},
        {{"", "11", "2"}, 0.19},
        {{"", "11", "3"}, 0.18},
        {{"", "11", "4"}, 0.24},
        {{"", "11", "5"}, 0.3},
        {{"", "11", "6"}, 0.38},
        {{"", "11", "7"}, 0.37},
        {{"", "11", "8"}, 0.37},
        {{"", "11", "9"}, 0.2},
        {{"", "11", "10"}, 0.25},
        {{"", "11", "12"}, 0.45},
        {{"", "12", "1"}, 0.18},
        {{"", "12", "2"}, 0.19},
        {{"", "12", "3"}, 0.18},
        {{"", "12", "4"}, 0.24},
        {{"", "12", "5"}, 0.3},
        {{"", "12", "6"}, 0.38},
        {{"", "12", "7"}, 0.37},
        {{"", "12", "8"}, 0.37},
        {{"", "12", "9"}, 0.2},
        {{"", "12", "10"}, 0.25},
        {{"", "12", "11"}, 0.45}
    };

    // Commodity inter-bucket correlations
    interBucketCorrelation_[RiskType::Commodity] = {
        {{"", "1", "2"}, 0.33},
        {{"", "1", "3"}, 0.21},
        {{"", "1", "4"}, 0.27},
        {{"", "1", "5"}, 0.29},
        {{"", "1", "6"}, 0.21},
        {{"", "1", "7"}, 0.48},
        {{"", "1", "8"}, 0.16},
        {{"", "1", "9"}, 0.41},
        {{"", "1", "10"}, 0.23},
        {{"", "1", "11"}, 0.18},
        {{"", "1", "12"}, 0.02},
        {{"", "1", "13"}, 0.21},
        {{"", "1", "14"}, 0.19},
        {{"", "1", "15"}, 0.15},
        {{"", "1", "16"}, 0.0},
        {{"", "1", "17"}, 0.24},
        {{"", "2", "1"}, 0.33},
        {{"", "2", "3"}, 0.94},
        {{"", "2", "4"}, 0.94},
        {{"", "2", "5"}, 0.89},
        {{"", "2", "6"}, 0.21},
        {{"", "2", "7"}, 0.19},
        {{"", "2", "8"}, 0.13},
        {{"", "2", "9"}, 0.21},
        {{"", "2", "10"}, 0.21},
        {{"", "2", "11"}, 0.41},
        {{"", "2", "12"}, 0.27},
        {{"", "2", "13"}, 0.31},
        {{"", "2", "14"}, 0.29},
        {{"", "2", "15"}, 0.21},
        {{"", "2", "16"}, 0.0},
        {{"", "2", "17"}, 0.6},
        {{"", "3", "1"}, 0.21},
        {{"", "3", "2"}, 0.94},
        {{"", "3", "4"}, 0.91},
        {{"", "3", "5"}, 0.85},
        {{"", "3", "6"}, 0.12},
        {{"", "3", "7"}, 0.2},
        {{"", "3", "8"}, 0.09},
        {{"", "3", "9"}, 0.19},
        {{"", "3", "10"}, 0.2},
        {{"", "3", "11"}, 0.36},
        {{"", "3", "12"}, 0.18},
        {{"", "3", "13"}, 0.22},
        {{"", "3", "14"}, 0.23},
        {{"", "3", "15"}, 0.23},
        {{"", "3", "16"}, 0.0},
        {{"", "3", "17"}, 0.54},
        {{"", "4", "1"}, 0.27},
        {{"", "4", "2"}, 0.94},
        {{"", "4", "3"}, 0.91},
        {{"", "4", "5"}, 0.84},
        {{"", "4", "6"}, 0.14},
        {{"", "4", "7"}, 0.24},
        {{"", "4", "8"}, 0.13},
        {{"", "4", "9"}, 0.21},
        {{"", "4", "10"}, 0.19},
        {{"", "4", "11"}, 0.39},
        {{"", "4", "12"}, 0.25},
        {{"", "4", "13"}, 0.23},
        {{"", "4", "14"}, 0.27},
        {{"", "4", "15"}, 0.18},
        {{"", "4", "16"}, 0.0},
        {{"", "4", "17"}, 0.59},
        {{"", "5", "1"}, 0.29},
        {{"", "5", "2"}, 0.89},
        {{"", "5", "3"}, 0.85},
        {{"", "5", "4"}, 0.84},
        {{"", "5", "6"}, 0.15},
        {{"", "5", "7"}, 0.17},
        {{"", "5", "8"}, 0.09},
        {{"", "5", "9"}, 0.16},
        {{"", "5", "10"}, 0.21},
        {{"", "5", "11"}, 0.38},
        {{"", "5", "12"}, 0.28},
        {{"", "5", "13"}, 0.28},
        {{"", "5", "14"}, 0.27},
        {{"", "5", "15"}, 0.18},
        {{"", "5", "16"}, 0.0},
        {{"", "5", "17"}, 0.55},
        {{"", "6", "1"}, 0.21},
        {{"", "6", "2"}, 0.21},
        {{"", "6", "3"}, 0.12},
        {{"", "6", "4"}, 0.14},
        {{"", "6", "5"}, 0.15},
        {{"", "6", "7"}, 0.33},
        {{"", "6", "8"}, 0.53},
        {{"", "6", "9"}, 0.26},
        {{"", "6", "10"}, 0.09},
        {{"", "6", "11"}, 0.21},
        {{"", "6", "12"}, 0.04},
        {{"", "6", "13"}, 0.11},
        {{"", "6", "14"}, 0.1},
        {{"", "6", "15"}, 0.09},
        {{"", "6", "16"}, 0.0},
        {{"", "6", "17"}, 0.24},
        {{"", "7", "1"}, 0.48},
        {{"", "7", "2"}, 0.19},
        {{"", "7", "3"}, 0.2},
        {{"", "7", "4"}, 0.24},
        {{"", "7", "5"}, 0.17},
        {{"", "7", "6"}, 0.33},
        {{"", "7", "8"}, 0.31},
        {{"", "7", "9"}, 0.72},
        {{"", "7", "10"}, 0.24},
        {{"", "7", "11"}, 0.14},
        {{"", "7", "12"}, -0.12},
        {{"", "7", "13"}, 0.19},
        {{"", "7", "14"}, 0.14},
        {{"", "7", "15"}, 0.08},
        {{"", "7", "16"}, 0.0},
        {{"", "7", "17"}, 0.24},
        {{"", "8", "1"}, 0.16},
        {{"", "8", "2"}, 0.13},
        {{"", "8", "3"}, 0.09},
        {{"", "8", "4"}, 0.13},
        {{"", "8", "5"}, 0.09},
        {{"", "8", "6"}, 0.53},
        {{"", "8", "7"}, 0.31},
        {{"", "8", "9"}, 0.24},
        {{"", "8", "10"}, 0.04},
        {{"", "8", "11"}, 0.13},
        {{"", "8", "12"}, -0.07},
        {{"", "8", "13"}, 0.04},
        {{"", "8", "14"}, 0.06},
        {{"", "8", "15"}, 0.01},
        {{"", "8", "16"}, 0.0},
        {{"", "8", "17"}, 0.16},
        {{"", "9", "1"}, 0.41},
        {{"", "9", "2"}, 0.21},
        {{"", "9", "3"}, 0.19},
        {{"", "9", "4"}, 0.21},
        {{"", "9", "5"}, 0.16},
        {{"", "9", "6"}, 0.26},
        {{"", "9", "7"}, 0.72},
        {{"", "9", "8"}, 0.24},
        {{"", "9", "10"}, 0.21},
        {{"", "9", "11"}, 0.18},
        {{"", "9", "12"}, -0.07},
        {{"", "9", "13"}, 0.12},
        {{"", "9", "14"}, 0.12},
        {{"", "9", "15"}, 0.1},
        {{"", "9", "16"}, 0.0},
        {{"", "9", "17"}, 0.21},
        {{"", "10", "1"}, 0.23},
        {{"", "10", "2"}, 0.21},
        {{"", "10", "3"}, 0.2},
        {{"", "10", "4"}, 0.19},
        {{"", "10", "5"}, 0.21},
        {{"", "10", "6"}, 0.09},
        {{"", "10", "7"}, 0.24},
        {{"", "10", "8"}, 0.04},
        {{"", "10", "9"}, 0.21},
        {{"", "10", "11"}, 0.14},
        {{"", "10", "12"}, 0.11},
        {{"", "10", "13"}, 0.11},
        {{"", "10", "14"}, 0.1},
        {{"", "10", "15"}, 0.07},
        {{"", "10", "16"}, 0.0},
        {{"", "10", "17"}, 0.14},
        {{"", "11", "1"}, 0.18},
        {{"", "11", "2"}, 0.41},
        {{"", "11", "3"}, 0.36},
        {{"", "11", "4"}, 0.39},
        {{"", "11", "5"}, 0.38},
        {{"", "11", "6"}, 0.21},
        {{"", "11", "7"}, 0.14},
        {{"", "11", "8"}, 0.13},
        {{"", "11", "9"}, 0.18},
        {{"", "11", "10"}, 0.14},
        {{"", "11", "12"}, 0.28},
        {{"", "11", "13"}, 0.3},
        {{"", "11", "14"}, 0.25},
        {{"", "11", "15"}, 0.18},
        {{"", "11", "16"}, 0.0},
        {{"", "11", "17"}, 0.38},
        {{"", "12", "1"}, 0.02},
        {{"", "12", "2"}, 0.27},
        {{"", "12", "3"}, 0.18},
        {{"", "12", "4"}, 0.25},
        {{"", "12", "5"}, 0.28},
        {{"", "12", "6"}, 0.04},
        {{"", "12", "7"}, -0.12},
        {{"", "12", "8"}, -0.07},
        {{"", "12", "9"}, -0.07},
        {{"", "12", "10"}, 0.11},
        {{"", "12", "11"}, 0.28},
        {{"", "12", "13"}, 0.18},
        {{"", "12", "14"}, 0.18},
        {{"", "12", "15"}, 0.08},
        {{"", "12", "16"}, 0.0},
        {{"", "12", "17"}, 0.21},
        {{"", "13", "1"}, 0.21},
        {{"", "13", "2"}, 0.31},
        {{"", "13", "3"}, 0.22},
        {{"", "13", "4"}, 0.23},
        {{"", "13", "5"}, 0.28},
        {{"", "13", "6"}, 0.11},
        {{"", "13", "7"}, 0.19},
        {{"", "13", "8"}, 0.04},
        {{"", "13", "9"}, 0.12},
        {{"", "13", "10"}, 0.11},
        {{"", "13", "11"}, 0.3},
        {{"", "13", "12"}, 0.18},
        {{"", "13", "14"}, 0.34},
        {{"", "13", "15"}, 0.16},
        {{"", "13", "16"}, 0.0},
        {{"", "13", "17"}, 0.34},
        {{"", "14", "1"}, 0.19},
        {{"", "14", "2"}, 0.29},
        {{"", "14", "3"}, 0.23},
        {{"", "14", "4"}, 0.27},
        {{"", "14", "5"}, 0.27},
        {{"", "14", "6"}, 0.1},
        {{"", "14", "7"}, 0.14},
        {{"", "14", "8"}, 0.06},
        {{"", "14", "9"}, 0.12},
        {{"", "14", "10"}, 0.1},
        {{"", "14", "11"}, 0.25},
        {{"", "14", "12"}, 0.18},
        {{"", "14", "13"}, 0.34},
        {{"", "14", "15"}, 0.13},
        {{"", "14", "16"}, 0.0},
        {{"", "14", "17"}, 0.26},
        {{"", "15", "1"}, 0.15},
        {{"", "15", "2"}, 0.21},
        {{"", "15", "3"}, 0.23},
        {{"", "15", "4"}, 0.18},
        {{"", "15", "5"}, 0.18},
        {{"", "15", "6"}, 0.09},
        {{"", "15", "7"}, 0.08},
        {{"", "15", "8"}, 0.01},
        {{"", "15", "9"}, 0.1},
        {{"", "15", "10"}, 0.07},
        {{"", "15", "11"}, 0.18},
        {{"", "15", "12"}, 0.08},
        {{"", "15", "13"}, 0.16},
        {{"", "15", "14"}, 0.13},
        {{"", "15", "16"}, 0.0},
        {{"", "15", "17"}, 0.21},
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
        {{"", "17", "2"}, 0.6},
        {{"", "17", "3"}, 0.54},
        {{"", "17", "4"}, 0.59},
        {{"", "17", "5"}, 0.55},
        {{"", "17", "6"}, 0.24},
        {{"", "17", "7"}, 0.24},
        {{"", "17", "8"}, 0.16},
        {{"", "17", "9"}, 0.21},
        {{"", "17", "10"}, 0.14},
        {{"", "17", "11"}, 0.38},
        {{"", "17", "12"}, 0.21},
        {{"", "17", "13"}, 0.34},
        {{"", "17", "14"}, 0.26},
        {{"", "17", "15"}, 0.21},
        {{"", "17", "16"}, 0.0}
    };

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[RiskType::Equity] = {
        {{"1", "", ""}, 0.18},
        {{"2", "", ""}, 0.23},
        {{"3", "", ""}, 0.3},
        {{"4", "", ""}, 0.26},
        {{"5", "", ""}, 0.23},
        {{"6", "", ""}, 0.35},
        {{"7", "", ""}, 0.36},
        {{"8", "", ""}, 0.33},
        {{"9", "", ""}, 0.19},
        {{"10", "", ""}, 0.2},
        {{"11", "", ""}, 0.45},
        {{"12", "", ""}, 0.45}
    };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[RiskType::Commodity] = {
        {{"1", "", ""}, 0.84},
        {{"2", "", ""}, 0.98},
        {{"3", "", ""}, 0.96},
        {{"4", "", ""}, 0.97},
        {{"5", "", ""}, 0.98},
        {{"6", "", ""}, 0.88},
        {{"7", "", ""}, 0.98},
        {{"8", "", ""}, 0.49},
        {{"9", "", ""}, 0.8},
        {{"10", "", ""}, 0.46},
        {{"11", "", ""}, 0.55},
        {{"12", "", ""}, 0.46},
        {{"13", "", ""}, 0.66},
        {{"14", "", ""}, 0.18},
        {{"15", "", ""}, 0.21},
        {{"16", "", ""}, 0},
        {{"17", "", ""}, 0.36}
    };

    // Initialise the single, ad-hoc type, correlations
    xccyCorr_ = 0.01;
    infCorr_ = 0.37;
    infVolCorr_ = 0.37;
    irSubCurveCorr_ = 0.99;
    irInterCurrencyCorr_ = 0.24;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.93;
    crqDiffIntraCorr_ = 0.42;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.82;
    crnqDiffIntraCorr_ = 0.27;
    crnqInterCorr_ = 0.4;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.24;

    // clang-format on
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 8 section 11(d)
of the ISDA-SIMM-v2.5 documentation).
*/
QuantLib::Real SimmConfiguration_ISDA_V2_5::curvatureMarginScaling() const { return pow(hvr_ir_, -2.0); }

void SimmConfiguration_ISDA_V2_5::addLabels2(const RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V2_5::label2(const QuantLib::ext::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::label2(irIndex);
}

} // namespace analytics
} // namespace ore
