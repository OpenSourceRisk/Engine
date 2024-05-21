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
    using RiskType = CrifRecord::RiskType;
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

SimmConfiguration_ISDA_V2_6::SimmConfiguration_ISDA_V2_6(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                         const QuantLib::Size& mporDays, const std::string& name,
                                                         const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentration_ISDA_V2_6>(simmBucketMapper_);
    } else {
        // SIMM:Technical Paper, Section I.4: "The Concentration Risk feature is disabled"
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentrationBase>();
    }

    // clang-format off
    // clang-format on

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
            { CrifRecord::RiskType::Inflation, 61 },
            { CrifRecord::RiskType::XCcyBasis, 21 },
            { CrifRecord::RiskType::IRVol, 0.23 },
            { CrifRecord::RiskType::InflationVol, 0.23 },
            { CrifRecord::RiskType::CreditVol, 0.76 },
            { CrifRecord::RiskType::CreditVolNonQ, 0.76 },
            { CrifRecord::RiskType::CommodityVol, 0.55 },
            { CrifRecord::RiskType::FXVol, 0.48 },
            { CrifRecord::RiskType::BaseCorr, 10 }
        };

        rwBucket_ = {
            {CrifRecord::RiskType::CreditQ,
             {{{"1", "", ""}, 75},
              {{"2", "", ""}, 90},
              {{"3", "", ""}, 84},
              {{"4", "", ""}, 54},
              {{"5", "", ""}, 62},
              {{"6", "", ""}, 48},
              {{"7", "", ""}, 185},
              {{"8", "", ""}, 343},
              {{"9", "", ""}, 255},
              {{"10", "", ""}, 250},
              {{"11", "", ""}, 214},
              {{"12", "", ""}, 173},
              {{"Residual", "", ""}, 343}}},
            {CrifRecord::RiskType::CreditNonQ, {{{"1", "", ""}, 280}, {{"2", "", ""}, 1300}, {{"Residual", "", ""}, 1300}}},
            {CrifRecord::RiskType::Equity,
             {{{"1", "", ""}, 30},
              {{"2", "", ""}, 33},
              {{"3", "", ""}, 36},
              {{"4", "", ""}, 29},
              {{"5", "", ""}, 26},
              {{"6", "", ""}, 25},
              {{"7", "", ""}, 34},
              {{"8", "", ""}, 28},
              {{"9", "", ""}, 36},
              {{"10", "", ""}, 50},
              {{"11", "", ""}, 19},
              {{"12", "", ""}, 19},
              {{"Residual", "", ""}, 50}}},
            {CrifRecord::RiskType::Commodity,
             {{{"1", "", ""}, 48},
              {{"2", "", ""}, 29},
              {{"3", "", ""}, 33},
              {{"4", "", ""}, 25},
              {{"5", "", ""}, 35},
              {{"6", "", ""}, 30},
              {{"7", "", ""}, 60},
              {{"8", "", ""}, 52},
              {{"9", "", ""}, 68},
              {{"10", "", ""}, 63},
              {{"11", "", ""}, 21},
              {{"12", "", ""}, 21},
              {{"13", "", ""}, 15},
              {{"14", "", ""}, 16},
              {{"15", "", ""}, 13},
              {{"16", "", ""}, 68},
              {{"17", "", ""}, 17}}},
            {CrifRecord::RiskType::EquityVol,
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
            {CrifRecord::RiskType::IRCurve, {
                {{"1", "2w", ""}, 109},
                {{"1", "1m", ""}, 105},
                {{"1", "3m", ""}, 90},
                {{"1", "6m", ""}, 71},
                {{"1", "1y", ""}, 66},
                {{"1", "2y", ""}, 66},
                {{"1", "3y", ""}, 64},
                {{"1", "5y", ""}, 60},
                {{"1", "10y", ""}, 60},
                {{"1", "15y", ""}, 61},
                {{"1", "20y", ""}, 61},
                {{"1", "30y", ""}, 67},
                {{"2", "2w", ""}, 15},
                {{"2", "1m", ""}, 18},
                {{"2", "3m", ""}, 9.0},
                {{"2", "6m", ""}, 11},
                {{"2", "1y", ""}, 13},
                {{"2", "2y", ""}, 15},
                {{"2", "3y", ""}, 19},
                {{"2", "5y", ""}, 23},
                {{"2", "10y", ""}, 23},
                {{"2", "15y", ""}, 22},
                {{"2", "20y", ""}, 22},
                {{"2", "30y", ""}, 23},
                {{"3", "2w", ""}, 163},
                {{"3", "1m", ""}, 109},
                {{"3", "3m", ""}, 87},
                {{"3", "6m", ""}, 89},
                {{"3", "1y", ""}, 102},
                {{"3", "2y", ""}, 96},
                {{"3", "3y", ""}, 101},
                {{"3", "5y", ""}, 97},
                {{"3", "10y", ""}, 97},
                {{"3", "15y", ""}, 102},
                {{"3", "20y", ""}, 106},
                {{"3", "30y", ""}, 101}}
            }
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[CrifRecord::RiskType::EquityVol] = 0.6;
        historicalVolatilityRatios_[CrifRecord::RiskType::CommodityVol] = 0.74;
        historicalVolatilityRatios_[CrifRecord::RiskType::FXVol] = 0.57;
        hvr_ir_ = 0.47;

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
           1.8,  3.5,
           3.5, 4.5
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { CrifRecord::RiskType::Inflation, 15 },
            { CrifRecord::RiskType::XCcyBasis, 6.0 },
            { CrifRecord::RiskType::IRVol, 0.046 },
            { CrifRecord::RiskType::InflationVol, 0.046 },
            { CrifRecord::RiskType::CreditVol, 0.09 },
            { CrifRecord::RiskType::CreditVolNonQ, 0.09 },
            { CrifRecord::RiskType::CommodityVol, 0.14 },
            { CrifRecord::RiskType::FXVol, 0.1 },
            { CrifRecord::RiskType::BaseCorr, 2.4 }
        };

        rwBucket_ = {
            {CrifRecord::RiskType::CreditQ,
             {{{"1", "", ""}, 20},
              {{"2", "", ""}, 27},
              {{"3", "", ""}, 17},
              {{"4", "", ""}, 12},
              {{"5", "", ""}, 13},
              {{"6", "", ""}, 12},
              {{"7", "", ""}, 50},
              {{"8", "", ""}, 93},
              {{"9", "", ""}, 51},
              {{"10", "", ""}, 57},
              {{"11", "", ""}, 43},
              {{"12", "", ""}, 37},
              {{"Residual", "", ""}, 93}}},
            {CrifRecord::RiskType::CreditNonQ, {{{"1", "", ""}, 66}, {{"2", "", ""}, 280}, {{"Residual", "", ""}, 280}}},
            {CrifRecord::RiskType::Equity,
             {{{"1", "", ""}, 8.8},
              {{"2", "", ""}, 9.6},
              {{"3", "", ""}, 10},
              {{"4", "", ""}, 9.0},
              {{"5", "", ""}, 8.6},
              {{"6", "", ""}, 8.6},
              {{"7", "", ""}, 11},
              {{"8", "", ""}, 10},
              {{"9", "", ""}, 9.8},
              {{"10", "", ""}, 14},
              {{"11", "", ""}, 6.1},
              {{"12", "", ""}, 6.1},
              {{"Residual", "", ""}, 14}}},
            {CrifRecord::RiskType::Commodity,
             {{{"1", "", ""}, 11},
              {{"2", "", ""}, 9.1},
              {{"3", "", ""}, 8.3},
              {{"4", "", ""}, 7.4},
              {{"5", "", ""}, 10},
              {{"6", "", ""}, 9.3},
              {{"7", "", ""}, 17},
              {{"8", "", ""}, 12},
              {{"9", "", ""}, 14},
              {{"10", "", ""}, 18},
              {{"11", "", ""}, 6.6},
              {{"12", "", ""}, 6.7},
              {{"13", "", ""}, 5.0},
              {{"14", "", ""}, 4.8},
              {{"15", "", ""}, 3.8},
              {{"16", "", ""}, 18},
              {{"17", "", ""}, 5.2}}},
            {CrifRecord::RiskType::EquityVol,
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
              {{"12", "", ""}, 0.23},
              {{"Residual", "", ""}, 0.093}}},
        };

        rwLabel_1_ = {
            {
                CrifRecord::RiskType::IRCurve,
             {{{"1", "2w", ""}, 19},
              {{"1", "1m", ""}, 15},
              {{"1", "3m", ""}, 12},
              {{"1", "6m", ""}, 13},
              {{"1", "1y", ""}, 15},
              {{"1", "2y", ""}, 18},
              {{"1", "3y", ""}, 18},
              {{"1", "5y", ""}, 18},
              {{"1", "10y", ""}, 18},
              {{"1", "15y", ""}, 18},
              {{"1", "20y", ""}, 17},
              {{"1", "30y", ""}, 18},
              {{"2", "2w", ""}, 1.7},
              {{"2", "1m", ""}, 2.9},
              {{"2", "3m", ""}, 1.7},
              {{"2", "6m", ""}, 2.0},
              {{"2", "1y", ""}, 3.4},
              {{"2", "2y", ""}, 4.8},
              {{"2", "3y", ""}, 5.8},
              {{"2", "5y", ""}, 7.3},
              {{"2", "10y", ""}, 7.8},
              {{"2", "15y", ""}, 7.5},
              {{"2", "20y", ""}, 8.0},
              {{"2", "30y", ""}, 9.0},
              {{"3", "2w", ""}, 55},
              {{"3", "1m", ""}, 29},
              {{"3", "3m", ""}, 18},
              {{"3", "6m", ""}, 21},
              {{"3", "1y", ""}, 26},
              {{"3", "2y", ""}, 25},
              {{"3", "3y", ""}, 34},
              {{"3", "5y", ""}, 33},
              {{"3", "10y", ""}, 34},
              {{"3", "15y", ""}, 31},
              {{"3", "20y", ""}, 34},
              {{"3", "30y", ""}, 28}}},
        };

        // Historical volatility ratios
        historicalVolatilityRatios_[CrifRecord::RiskType::EquityVol] = 0.55;
        historicalVolatilityRatios_[CrifRecord::RiskType::CommodityVol] = 0.74;
        historicalVolatilityRatios_[CrifRecord::RiskType::FXVol] = 0.74;
        hvr_ir_ = 0.51;

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
        {{"", "InterestRate", "CreditQualifying"}, 0.04},
        {{"", "InterestRate", "CreditNonQualifying"}, 0.04},
        {{"", "InterestRate", "Equity"}, 0.07},
        {{"", "InterestRate", "Commodity"}, 0.37},
        {{"", "InterestRate", "FX"}, 0.14},
        {{"", "CreditQualifying", "InterestRate"}, 0.04},
        {{"", "CreditQualifying", "CreditNonQualifying"}, 0.54},
        {{"", "CreditQualifying", "Equity"}, 0.7},
        {{"", "CreditQualifying", "Commodity"}, 0.27},
        {{"", "CreditQualifying", "FX"}, 0.37},
        {{"", "CreditNonQualifying", "InterestRate"}, 0.04},
        {{"", "CreditNonQualifying", "CreditQualifying"}, 0.54},
        {{"", "CreditNonQualifying", "Equity"}, 0.46},
        {{"", "CreditNonQualifying", "Commodity"}, 0.24},
        {{"", "CreditNonQualifying", "FX"}, 0.15},
        {{"", "Equity", "InterestRate"}, 0.07},
        {{"", "Equity", "CreditQualifying"}, 0.7},
        {{"", "Equity", "CreditNonQualifying"}, 0.46},
        {{"", "Equity", "Commodity"}, 0.35},
        {{"", "Equity", "FX"}, 0.39},
        {{"", "Commodity", "InterestRate"}, 0.37},
        {{"", "Commodity", "CreditQualifying"}, 0.27},
        {{"", "Commodity", "CreditNonQualifying"}, 0.24},
        {{"", "Commodity", "Equity"}, 0.35},
        {{"", "Commodity", "FX"}, 0.35},
        {{"", "FX", "InterestRate"}, 0.14},
        {{"", "FX", "CreditQualifying"}, 0.37},
        {{"", "FX", "CreditNonQualifying"}, 0.15},
        {{"", "FX", "Equity"}, 0.39},
        {{"", "FX", "Commodity"}, 0.35}
    };

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
    intraBucketCorrelation_[CrifRecord::RiskType::IRCurve] = {
        {{"", "2w", "1m"}, 0.77},
        {{"", "2w", "3m"}, 0.67},
        {{"", "2w", "6m"}, 0.59},
        {{"", "2w", "1y"}, 0.48},
        {{"", "2w", "2y"}, 0.39},
        {{"", "2w", "3y"}, 0.34},
        {{"", "2w", "5y"}, 0.3},
        {{"", "2w", "10y"}, 0.25},
        {{"", "2w", "15y"}, 0.23},
        {{"", "2w", "20y"}, 0.21},
        {{"", "2w", "30y"}, 0.2},
        {{"", "1m", "2w"}, 0.77},
        {{"", "1m", "3m"}, 0.84},
        {{"", "1m", "6m"}, 0.74},
        {{"", "1m", "1y"}, 0.56},
        {{"", "1m", "2y"}, 0.43},
        {{"", "1m", "3y"}, 0.36},
        {{"", "1m", "5y"}, 0.31},
        {{"", "1m", "10y"}, 0.26},
        {{"", "1m", "15y"}, 0.21},
        {{"", "1m", "20y"}, 0.19},
        {{"", "1m", "30y"}, 0.19},
        {{"", "3m", "2w"}, 0.67},
        {{"", "3m", "1m"}, 0.84},
        {{"", "3m", "6m"}, 0.88},
        {{"", "3m", "1y"}, 0.69},
        {{"", "3m", "2y"}, 0.55},
        {{"", "3m", "3y"}, 0.47},
        {{"", "3m", "5y"}, 0.4},
        {{"", "3m", "10y"}, 0.34},
        {{"", "3m", "15y"}, 0.27},
        {{"", "3m", "20y"}, 0.25},
        {{"", "3m", "30y"}, 0.25},
        {{"", "6m", "2w"}, 0.59},
        {{"", "6m", "1m"}, 0.74},
        {{"", "6m", "3m"}, 0.88},
        {{"", "6m", "1y"}, 0.86},
        {{"", "6m", "2y"}, 0.73},
        {{"", "6m", "3y"}, 0.65},
        {{"", "6m", "5y"}, 0.57},
        {{"", "6m", "10y"}, 0.49},
        {{"", "6m", "15y"}, 0.4},
        {{"", "6m", "20y"}, 0.38},
        {{"", "6m", "30y"}, 0.37},
        {{"", "1y", "2w"}, 0.48},
        {{"", "1y", "1m"}, 0.56},
        {{"", "1y", "3m"}, 0.69},
        {{"", "1y", "6m"}, 0.86},
        {{"", "1y", "2y"}, 0.94},
        {{"", "1y", "3y"}, 0.87},
        {{"", "1y", "5y"}, 0.79},
        {{"", "1y", "10y"}, 0.68},
        {{"", "1y", "15y"}, 0.6},
        {{"", "1y", "20y"}, 0.57},
        {{"", "1y", "30y"}, 0.55},
        {{"", "2y", "2w"}, 0.39},
        {{"", "2y", "1m"}, 0.43},
        {{"", "2y", "3m"}, 0.55},
        {{"", "2y", "6m"}, 0.73},
        {{"", "2y", "1y"}, 0.94},
        {{"", "2y", "3y"}, 0.96},
        {{"", "2y", "5y"}, 0.91},
        {{"", "2y", "10y"}, 0.8},
        {{"", "2y", "15y"}, 0.74},
        {{"", "2y", "20y"}, 0.7},
        {{"", "2y", "30y"}, 0.69},
        {{"", "3y", "2w"}, 0.34},
        {{"", "3y", "1m"}, 0.36},
        {{"", "3y", "3m"}, 0.47},
        {{"", "3y", "6m"}, 0.65},
        {{"", "3y", "1y"}, 0.87},
        {{"", "3y", "2y"}, 0.96},
        {{"", "3y", "5y"}, 0.97},
        {{"", "3y", "10y"}, 0.88},
        {{"", "3y", "15y"}, 0.81},
        {{"", "3y", "20y"}, 0.77},
        {{"", "3y", "30y"}, 0.76},
        {{"", "5y", "2w"}, 0.3},
        {{"", "5y", "1m"}, 0.31},
        {{"", "5y", "3m"}, 0.4},
        {{"", "5y", "6m"}, 0.57},
        {{"", "5y", "1y"}, 0.79},
        {{"", "5y", "2y"}, 0.91},
        {{"", "5y", "3y"}, 0.97},
        {{"", "5y", "10y"}, 0.95},
        {{"", "5y", "15y"}, 0.9},
        {{"", "5y", "20y"}, 0.86},
        {{"", "5y", "30y"}, 0.85},
        {{"", "10y", "2w"}, 0.25},
        {{"", "10y", "1m"}, 0.26},
        {{"", "10y", "3m"}, 0.34},
        {{"", "10y", "6m"}, 0.49},
        {{"", "10y", "1y"}, 0.68},
        {{"", "10y", "2y"}, 0.8},
        {{"", "10y", "3y"}, 0.88},
        {{"", "10y", "5y"}, 0.95},
        {{"", "10y", "15y"}, 0.97},
        {{"", "10y", "20y"}, 0.94},
        {{"", "10y", "30y"}, 0.94},
        {{"", "15y", "2w"}, 0.23},
        {{"", "15y", "1m"}, 0.21},
        {{"", "15y", "3m"}, 0.27},
        {{"", "15y", "6m"}, 0.4},
        {{"", "15y", "1y"}, 0.6},
        {{"", "15y", "2y"}, 0.74},
        {{"", "15y", "3y"}, 0.81},
        {{"", "15y", "5y"}, 0.9},
        {{"", "15y", "10y"}, 0.97},
        {{"", "15y", "20y"}, 0.98},
        {{"", "15y", "30y"}, 0.97},
        {{"", "20y", "2w"}, 0.21},
        {{"", "20y", "1m"}, 0.19},
        {{"", "20y", "3m"}, 0.25},
        {{"", "20y", "6m"}, 0.38},
        {{"", "20y", "1y"}, 0.57},
        {{"", "20y", "2y"}, 0.7},
        {{"", "20y", "3y"}, 0.77},
        {{"", "20y", "5y"}, 0.86},
        {{"", "20y", "10y"}, 0.94},
        {{"", "20y", "15y"}, 0.98},
        {{"", "20y", "30y"}, 0.99},
        {{"", "30y", "2w"}, 0.2},
        {{"", "30y", "1m"}, 0.19},
        {{"", "30y", "3m"}, 0.25},
        {{"", "30y", "6m"}, 0.37},
        {{"", "30y", "1y"}, 0.55},
        {{"", "30y", "2y"}, 0.69},
        {{"", "30y", "3y"}, 0.76},
        {{"", "30y", "5y"}, 0.85},
        {{"", "30y", "10y"}, 0.94},
        {{"", "30y", "15y"}, 0.97},
        {{"", "30y", "20y"}, 0.99}
    };

    // CreditQ inter-bucket correlations
    interBucketCorrelation_[CrifRecord::RiskType::CreditQ] = {
        {{"", "1", "2"}, 0.38},
        {{"", "1", "3"}, 0.38},
        {{"", "1", "4"}, 0.35},
        {{"", "1", "5"}, 0.37},
        {{"", "1", "6"}, 0.34},
        {{"", "1", "7"}, 0.42},
        {{"", "1", "8"}, 0.32},
        {{"", "1", "9"}, 0.34},
        {{"", "1", "10"}, 0.33},
        {{"", "1", "11"}, 0.34},
        {{"", "1", "12"}, 0.33},
        {{"", "2", "1"}, 0.38},
        {{"", "2", "3"}, 0.48},
        {{"", "2", "4"}, 0.46},
        {{"", "2", "5"}, 0.48},
        {{"", "2", "6"}, 0.46},
        {{"", "2", "7"}, 0.39},
        {{"", "2", "8"}, 0.4},
        {{"", "2", "9"}, 0.41},
        {{"", "2", "10"}, 0.41},
        {{"", "2", "11"}, 0.43},
        {{"", "2", "12"}, 0.4},
        {{"", "3", "1"}, 0.38},
        {{"", "3", "2"}, 0.48},
        {{"", "3", "4"}, 0.5},
        {{"", "3", "5"}, 0.51},
        {{"", "3", "6"}, 0.5},
        {{"", "3", "7"}, 0.4},
        {{"", "3", "8"}, 0.39},
        {{"", "3", "9"}, 0.45},
        {{"", "3", "10"}, 0.44},
        {{"", "3", "11"}, 0.47},
        {{"", "3", "12"}, 0.42},
        {{"", "4", "1"}, 0.35},
        {{"", "4", "2"}, 0.46},
        {{"", "4", "3"}, 0.5},
        {{"", "4", "5"}, 0.5},
        {{"", "4", "6"}, 0.5},
        {{"", "4", "7"}, 0.37},
        {{"", "4", "8"}, 0.37},
        {{"", "4", "9"}, 0.41},
        {{"", "4", "10"}, 0.43},
        {{"", "4", "11"}, 0.45},
        {{"", "4", "12"}, 0.4},
        {{"", "5", "1"}, 0.37},
        {{"", "5", "2"}, 0.48},
        {{"", "5", "3"}, 0.51},
        {{"", "5", "4"}, 0.5},
        {{"", "5", "6"}, 0.5},
        {{"", "5", "7"}, 0.39},
        {{"", "5", "8"}, 0.38},
        {{"", "5", "9"}, 0.43},
        {{"", "5", "10"}, 0.43},
        {{"", "5", "11"}, 0.46},
        {{"", "5", "12"}, 0.42},
        {{"", "6", "1"}, 0.34},
        {{"", "6", "2"}, 0.46},
        {{"", "6", "3"}, 0.5},
        {{"", "6", "4"}, 0.5},
        {{"", "6", "5"}, 0.5},
        {{"", "6", "7"}, 0.37},
        {{"", "6", "8"}, 0.35},
        {{"", "6", "9"}, 0.39},
        {{"", "6", "10"}, 0.41},
        {{"", "6", "11"}, 0.44},
        {{"", "6", "12"}, 0.41},
        {{"", "7", "1"}, 0.42},
        {{"", "7", "2"}, 0.39},
        {{"", "7", "3"}, 0.4},
        {{"", "7", "4"}, 0.37},
        {{"", "7", "5"}, 0.39},
        {{"", "7", "6"}, 0.37},
        {{"", "7", "8"}, 0.33},
        {{"", "7", "9"}, 0.37},
        {{"", "7", "10"}, 0.37},
        {{"", "7", "11"}, 0.35},
        {{"", "7", "12"}, 0.35},
        {{"", "8", "1"}, 0.32},
        {{"", "8", "2"}, 0.4},
        {{"", "8", "3"}, 0.39},
        {{"", "8", "4"}, 0.37},
        {{"", "8", "5"}, 0.38},
        {{"", "8", "6"}, 0.35},
        {{"", "8", "7"}, 0.33},
        {{"", "8", "9"}, 0.36},
        {{"", "8", "10"}, 0.37},
        {{"", "8", "11"}, 0.37},
        {{"", "8", "12"}, 0.36},
        {{"", "9", "1"}, 0.34},
        {{"", "9", "2"}, 0.41},
        {{"", "9", "3"}, 0.45},
        {{"", "9", "4"}, 0.41},
        {{"", "9", "5"}, 0.43},
        {{"", "9", "6"}, 0.39},
        {{"", "9", "7"}, 0.37},
        {{"", "9", "8"}, 0.36},
        {{"", "9", "10"}, 0.41},
        {{"", "9", "11"}, 0.4},
        {{"", "9", "12"}, 0.38},
        {{"", "10", "1"}, 0.33},
        {{"", "10", "2"}, 0.41},
        {{"", "10", "3"}, 0.44},
        {{"", "10", "4"}, 0.43},
        {{"", "10", "5"}, 0.43},
        {{"", "10", "6"}, 0.41},
        {{"", "10", "7"}, 0.37},
        {{"", "10", "8"}, 0.37},
        {{"", "10", "9"}, 0.41},
        {{"", "10", "11"}, 0.41},
        {{"", "10", "12"}, 0.39},
        {{"", "11", "1"}, 0.34},
        {{"", "11", "2"}, 0.43},
        {{"", "11", "3"}, 0.47},
        {{"", "11", "4"}, 0.45},
        {{"", "11", "5"}, 0.46},
        {{"", "11", "6"}, 0.44},
        {{"", "11", "7"}, 0.35},
        {{"", "11", "8"}, 0.37},
        {{"", "11", "9"}, 0.4},
        {{"", "11", "10"}, 0.41},
        {{"", "11", "12"}, 0.4},
        {{"", "12", "1"}, 0.33},
        {{"", "12", "2"}, 0.4},
        {{"", "12", "3"}, 0.42},
        {{"", "12", "4"}, 0.4},
        {{"", "12", "5"}, 0.42},
        {{"", "12", "6"}, 0.41},
        {{"", "12", "7"}, 0.35},
        {{"", "12", "8"}, 0.36},
        {{"", "12", "9"}, 0.38},
        {{"", "12", "10"}, 0.39},
        {{"", "12", "11"}, 0.4}
    };

     // Equity inter-bucket correlations
    interBucketCorrelation_[CrifRecord::RiskType::Equity] = {
        {{"", "1", "2"}, 0.18},
        {{"", "1", "3"}, 0.19},
        {{"", "1", "4"}, 0.19},
        {{"", "1", "5"}, 0.14},
        {{"", "1", "6"}, 0.16},
        {{"", "1", "7"}, 0.15},
        {{"", "1", "8"}, 0.16},
        {{"", "1", "9"}, 0.18},
        {{"", "1", "10"}, 0.12},
        {{"", "1", "11"}, 0.19},
        {{"", "1", "12"}, 0.19},
        {{"", "2", "1"}, 0.18},
        {{"", "2", "3"}, 0.22},
        {{"", "2", "4"}, 0.21},
        {{"", "2", "5"}, 0.15},
        {{"", "2", "6"}, 0.18},
        {{"", "2", "7"}, 0.17},
        {{"", "2", "8"}, 0.19},
        {{"", "2", "9"}, 0.2},
        {{"", "2", "10"}, 0.14},
        {{"", "2", "11"}, 0.21},
        {{"", "2", "12"}, 0.21},
        {{"", "3", "1"}, 0.19},
        {{"", "3", "2"}, 0.22},
        {{"", "3", "4"}, 0.22},
        {{"", "3", "5"}, 0.13},
        {{"", "3", "6"}, 0.16},
        {{"", "3", "7"}, 0.18},
        {{"", "3", "8"}, 0.17},
        {{"", "3", "9"}, 0.22},
        {{"", "3", "10"}, 0.13},
        {{"", "3", "11"}, 0.2},
        {{"", "3", "12"}, 0.2},
        {{"", "4", "1"}, 0.19},
        {{"", "4", "2"}, 0.21},
        {{"", "4", "3"}, 0.22},
        {{"", "4", "5"}, 0.17},
        {{"", "4", "6"}, 0.22},
        {{"", "4", "7"}, 0.22},
        {{"", "4", "8"}, 0.23},
        {{"", "4", "9"}, 0.22},
        {{"", "4", "10"}, 0.17},
        {{"", "4", "11"}, 0.26},
        {{"", "4", "12"}, 0.26},
        {{"", "5", "1"}, 0.14},
        {{"", "5", "2"}, 0.15},
        {{"", "5", "3"}, 0.13},
        {{"", "5", "4"}, 0.17},
        {{"", "5", "6"}, 0.29},
        {{"", "5", "7"}, 0.26},
        {{"", "5", "8"}, 0.29},
        {{"", "5", "9"}, 0.14},
        {{"", "5", "10"}, 0.24},
        {{"", "5", "11"}, 0.32},
        {{"", "5", "12"}, 0.32},
        {{"", "6", "1"}, 0.16},
        {{"", "6", "2"}, 0.18},
        {{"", "6", "3"}, 0.16},
        {{"", "6", "4"}, 0.22},
        {{"", "6", "5"}, 0.29},
        {{"", "6", "7"}, 0.34},
        {{"", "6", "8"}, 0.36},
        {{"", "6", "9"}, 0.17},
        {{"", "6", "10"}, 0.3},
        {{"", "6", "11"}, 0.39},
        {{"", "6", "12"}, 0.39},
        {{"", "7", "1"}, 0.15},
        {{"", "7", "2"}, 0.17},
        {{"", "7", "3"}, 0.18},
        {{"", "7", "4"}, 0.22},
        {{"", "7", "5"}, 0.26},
        {{"", "7", "6"}, 0.34},
        {{"", "7", "8"}, 0.33},
        {{"", "7", "9"}, 0.16},
        {{"", "7", "10"}, 0.28},
        {{"", "7", "11"}, 0.36},
        {{"", "7", "12"}, 0.36},
        {{"", "8", "1"}, 0.16},
        {{"", "8", "2"}, 0.19},
        {{"", "8", "3"}, 0.17},
        {{"", "8", "4"}, 0.23},
        {{"", "8", "5"}, 0.29},
        {{"", "8", "6"}, 0.36},
        {{"", "8", "7"}, 0.33},
        {{"", "8", "9"}, 0.17},
        {{"", "8", "10"}, 0.29},
        {{"", "8", "11"}, 0.4},
        {{"", "8", "12"}, 0.4},
        {{"", "9", "1"}, 0.18},
        {{"", "9", "2"}, 0.2},
        {{"", "9", "3"}, 0.22},
        {{"", "9", "4"}, 0.22},
        {{"", "9", "5"}, 0.14},
        {{"", "9", "6"}, 0.17},
        {{"", "9", "7"}, 0.16},
        {{"", "9", "8"}, 0.17},
        {{"", "9", "10"}, 0.13},
        {{"", "9", "11"}, 0.21},
        {{"", "9", "12"}, 0.21},
        {{"", "10", "1"}, 0.12},
        {{"", "10", "2"}, 0.14},
        {{"", "10", "3"}, 0.13},
        {{"", "10", "4"}, 0.17},
        {{"", "10", "5"}, 0.24},
        {{"", "10", "6"}, 0.3},
        {{"", "10", "7"}, 0.28},
        {{"", "10", "8"}, 0.29},
        {{"", "10", "9"}, 0.13},
        {{"", "10", "11"}, 0.3},
        {{"", "10", "12"}, 0.3},
        {{"", "11", "1"}, 0.19},
        {{"", "11", "2"}, 0.21},
        {{"", "11", "3"}, 0.2},
        {{"", "11", "4"}, 0.26},
        {{"", "11", "5"}, 0.32},
        {{"", "11", "6"}, 0.39},
        {{"", "11", "7"}, 0.36},
        {{"", "11", "8"}, 0.4},
        {{"", "11", "9"}, 0.21},
        {{"", "11", "10"}, 0.3},
        {{"", "11", "12"}, 0.45},
        {{"", "12", "1"}, 0.19},
        {{"", "12", "2"}, 0.21},
        {{"", "12", "3"}, 0.2},
        {{"", "12", "4"}, 0.26},
        {{"", "12", "5"}, 0.32},
        {{"", "12", "6"}, 0.39},
        {{"", "12", "7"}, 0.36},
        {{"", "12", "8"}, 0.4},
        {{"", "12", "9"}, 0.21},
        {{"", "12", "10"}, 0.3},
        {{"", "12", "11"}, 0.45}
    };

    // Commodity inter-bucket correlations
    interBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        {{"", "1", "2"}, 0.22},
        {{"", "1", "3"}, 0.18},
        {{"", "1", "4"}, 0.21},
        {{"", "1", "5"}, 0.2},
        {{"", "1", "6"}, 0.24},
        {{"", "1", "7"}, 0.49},
        {{"", "1", "8"}, 0.16},
        {{"", "1", "9"}, 0.38},
        {{"", "1", "10"}, 0.14},
        {{"", "1", "11"}, 0.1},
        {{"", "1", "12"}, 0.02},
        {{"", "1", "13"}, 0.12},
        {{"", "1", "14"}, 0.11},
        {{"", "1", "15"}, 0.02},
        {{"", "1", "16"}, 0.0},
        {{"", "1", "17"}, 0.17},
        {{"", "2", "1"}, 0.22},
        {{"", "2", "3"}, 0.92},
        {{"", "2", "4"}, 0.9},
        {{"", "2", "5"}, 0.88},
        {{"", "2", "6"}, 0.25},
        {{"", "2", "7"}, 0.08},
        {{"", "2", "8"}, 0.19},
        {{"", "2", "9"}, 0.17},
        {{"", "2", "10"}, 0.17},
        {{"", "2", "11"}, 0.42},
        {{"", "2", "12"}, 0.28},
        {{"", "2", "13"}, 0.36},
        {{"", "2", "14"}, 0.27},
        {{"", "2", "15"}, 0.2},
        {{"", "2", "16"}, 0.0},
        {{"", "2", "17"}, 0.64},
        {{"", "3", "1"}, 0.18},
        {{"", "3", "2"}, 0.92},
        {{"", "3", "4"}, 0.87},
        {{"", "3", "5"}, 0.84},
        {{"", "3", "6"}, 0.16},
        {{"", "3", "7"}, 0.07},
        {{"", "3", "8"}, 0.15},
        {{"", "3", "9"}, 0.1},
        {{"", "3", "10"}, 0.18},
        {{"", "3", "11"}, 0.33},
        {{"", "3", "12"}, 0.22},
        {{"", "3", "13"}, 0.27},
        {{"", "3", "14"}, 0.23},
        {{"", "3", "15"}, 0.16},
        {{"", "3", "16"}, 0.0},
        {{"", "3", "17"}, 0.54},
        {{"", "4", "1"}, 0.21},
        {{"", "4", "2"}, 0.9},
        {{"", "4", "3"}, 0.87},
        {{"", "4", "5"}, 0.77},
        {{"", "4", "6"}, 0.19},
        {{"", "4", "7"}, 0.11},
        {{"", "4", "8"}, 0.18},
        {{"", "4", "9"}, 0.16},
        {{"", "4", "10"}, 0.14},
        {{"", "4", "11"}, 0.32},
        {{"", "4", "12"}, 0.22},
        {{"", "4", "13"}, 0.28},
        {{"", "4", "14"}, 0.22},
        {{"", "4", "15"}, 0.11},
        {{"", "4", "16"}, 0.0},
        {{"", "4", "17"}, 0.58},
        {{"", "5", "1"}, 0.2},
        {{"", "5", "2"}, 0.88},
        {{"", "5", "3"}, 0.84},
        {{"", "5", "4"}, 0.77},
        {{"", "5", "6"}, 0.19},
        {{"", "5", "7"}, 0.09},
        {{"", "5", "8"}, 0.12},
        {{"", "5", "9"}, 0.13},
        {{"", "5", "10"}, 0.18},
        {{"", "5", "11"}, 0.42},
        {{"", "5", "12"}, 0.34},
        {{"", "5", "13"}, 0.32},
        {{"", "5", "14"}, 0.29},
        {{"", "5", "15"}, 0.13},
        {{"", "5", "16"}, 0.0},
        {{"", "5", "17"}, 0.59},
        {{"", "6", "1"}, 0.24},
        {{"", "6", "2"}, 0.25},
        {{"", "6", "3"}, 0.16},
        {{"", "6", "4"}, 0.19},
        {{"", "6", "5"}, 0.19},
        {{"", "6", "7"}, 0.31},
        {{"", "6", "8"}, 0.62},
        {{"", "6", "9"}, 0.23},
        {{"", "6", "10"}, 0.1},
        {{"", "6", "11"}, 0.21},
        {{"", "6", "12"}, 0.05},
        {{"", "6", "13"}, 0.18},
        {{"", "6", "14"}, 0.1},
        {{"", "6", "15"}, 0.08},
        {{"", "6", "16"}, 0.0},
        {{"", "6", "17"}, 0.28},
        {{"", "7", "1"}, 0.49},
        {{"", "7", "2"}, 0.08},
        {{"", "7", "3"}, 0.07},
        {{"", "7", "4"}, 0.11},
        {{"", "7", "5"}, 0.09},
        {{"", "7", "6"}, 0.31},
        {{"", "7", "8"}, 0.21},
        {{"", "7", "9"}, 0.79},
        {{"", "7", "10"}, 0.17},
        {{"", "7", "11"}, 0.1},
        {{"", "7", "12"}, -0.08},
        {{"", "7", "13"}, 0.1},
        {{"", "7", "14"}, 0.07},
        {{"", "7", "15"}, -0.02},
        {{"", "7", "16"}, 0.0},
        {{"", "7", "17"}, 0.13},
        {{"", "8", "1"}, 0.16},
        {{"", "8", "2"}, 0.19},
        {{"", "8", "3"}, 0.15},
        {{"", "8", "4"}, 0.18},
        {{"", "8", "5"}, 0.12},
        {{"", "8", "6"}, 0.62},
        {{"", "8", "7"}, 0.21},
        {{"", "8", "9"}, 0.16},
        {{"", "8", "10"}, 0.08},
        {{"", "8", "11"}, 0.13},
        {{"", "8", "12"}, -0.07},
        {{"", "8", "13"}, 0.07},
        {{"", "8", "14"}, 0.05},
        {{"", "8", "15"}, 0.02},
        {{"", "8", "16"}, 0.0},
        {{"", "8", "17"}, 0.19},
        {{"", "9", "1"}, 0.38},
        {{"", "9", "2"}, 0.17},
        {{"", "9", "3"}, 0.1},
        {{"", "9", "4"}, 0.16},
        {{"", "9", "5"}, 0.13},
        {{"", "9", "6"}, 0.23},
        {{"", "9", "7"}, 0.79},
        {{"", "9", "8"}, 0.16},
        {{"", "9", "10"}, 0.15},
        {{"", "9", "11"}, 0.09},
        {{"", "9", "12"}, -0.06},
        {{"", "9", "13"}, 0.06},
        {{"", "9", "14"}, 0.06},
        {{"", "9", "15"}, 0.01},
        {{"", "9", "16"}, 0.0},
        {{"", "9", "17"}, 0.16},
        {{"", "10", "1"}, 0.14},
        {{"", "10", "2"}, 0.17},
        {{"", "10", "3"}, 0.18},
        {{"", "10", "4"}, 0.14},
        {{"", "10", "5"}, 0.18},
        {{"", "10", "6"}, 0.1},
        {{"", "10", "7"}, 0.17},
        {{"", "10", "8"}, 0.08},
        {{"", "10", "9"}, 0.15},
        {{"", "10", "11"}, 0.16},
        {{"", "10", "12"}, 0.09},
        {{"", "10", "13"}, 0.14},
        {{"", "10", "14"}, 0.09},
        {{"", "10", "15"}, 0.03},
        {{"", "10", "16"}, 0.0},
        {{"", "10", "17"}, 0.11},
        {{"", "11", "1"}, 0.1},
        {{"", "11", "2"}, 0.42},
        {{"", "11", "3"}, 0.33},
        {{"", "11", "4"}, 0.32},
        {{"", "11", "5"}, 0.42},
        {{"", "11", "6"}, 0.21},
        {{"", "11", "7"}, 0.1},
        {{"", "11", "8"}, 0.13},
        {{"", "11", "9"}, 0.09},
        {{"", "11", "10"}, 0.16},
        {{"", "11", "12"}, 0.36},
        {{"", "11", "13"}, 0.3},
        {{"", "11", "14"}, 0.25},
        {{"", "11", "15"}, 0.18},
        {{"", "11", "16"}, 0.0},
        {{"", "11", "17"}, 0.37},
        {{"", "12", "1"}, 0.02},
        {{"", "12", "2"}, 0.28},
        {{"", "12", "3"}, 0.22},
        {{"", "12", "4"}, 0.22},
        {{"", "12", "5"}, 0.34},
        {{"", "12", "6"}, 0.05},
        {{"", "12", "7"}, -0.08},
        {{"", "12", "8"}, -0.07},
        {{"", "12", "9"}, -0.06},
        {{"", "12", "10"}, 0.09},
        {{"", "12", "11"}, 0.36},
        {{"", "12", "13"}, 0.2},
        {{"", "12", "14"}, 0.18},
        {{"", "12", "15"}, 0.11},
        {{"", "12", "16"}, 0.0},
        {{"", "12", "17"}, 0.26},
        {{"", "13", "1"}, 0.12},
        {{"", "13", "2"}, 0.36},
        {{"", "13", "3"}, 0.27},
        {{"", "13", "4"}, 0.28},
        {{"", "13", "5"}, 0.32},
        {{"", "13", "6"}, 0.18},
        {{"", "13", "7"}, 0.1},
        {{"", "13", "8"}, 0.07},
        {{"", "13", "9"}, 0.06},
        {{"", "13", "10"}, 0.14},
        {{"", "13", "11"}, 0.3},
        {{"", "13", "12"}, 0.2},
        {{"", "13", "14"}, 0.28},
        {{"", "13", "15"}, 0.19},
        {{"", "13", "16"}, 0.0},
        {{"", "13", "17"}, 0.39},
        {{"", "14", "1"}, 0.11},
        {{"", "14", "2"}, 0.27},
        {{"", "14", "3"}, 0.23},
        {{"", "14", "4"}, 0.22},
        {{"", "14", "5"}, 0.29},
        {{"", "14", "6"}, 0.1},
        {{"", "14", "7"}, 0.07},
        {{"", "14", "8"}, 0.05},
        {{"", "14", "9"}, 0.06},
        {{"", "14", "10"}, 0.09},
        {{"", "14", "11"}, 0.25},
        {{"", "14", "12"}, 0.18},
        {{"", "14", "13"}, 0.28},
        {{"", "14", "15"}, 0.13},
        {{"", "14", "16"}, 0.0},
        {{"", "14", "17"}, 0.26},
        {{"", "15", "1"}, 0.02},
        {{"", "15", "2"}, 0.2},
        {{"", "15", "3"}, 0.16},
        {{"", "15", "4"}, 0.11},
        {{"", "15", "5"}, 0.13},
        {{"", "15", "6"}, 0.08},
        {{"", "15", "7"}, -0.02},
        {{"", "15", "8"}, 0.02},
        {{"", "15", "9"}, 0.01},
        {{"", "15", "10"}, 0.03},
        {{"", "15", "11"}, 0.18},
        {{"", "15", "12"}, 0.11},
        {{"", "15", "13"}, 0.19},
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
        {{"", "17", "1"}, 0.17},
        {{"", "17", "2"}, 0.64},
        {{"", "17", "3"}, 0.54},
        {{"", "17", "4"}, 0.58},
        {{"", "17", "5"}, 0.59},
        {{"", "17", "6"}, 0.28},
        {{"", "17", "7"}, 0.13},
        {{"", "17", "8"}, 0.19},
        {{"", "17", "9"}, 0.16},
        {{"", "17", "10"}, 0.11},
        {{"", "17", "11"}, 0.37},
        {{"", "17", "12"}, 0.26},
        {{"", "17", "13"}, 0.39},
        {{"", "17", "14"}, 0.26},
        {{"", "17", "15"}, 0.21},
        {{"", "17", "16"}, 0.0}
    };

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[CrifRecord::RiskType::Equity] = {
        {{"1", "", ""}, 0.18},
        {{"2", "", ""}, 0.2},
        {{"3", "", ""}, 0.28},
        {{"4", "", ""}, 0.24},
        {{"5", "", ""}, 0.25},
        {{"6", "", ""}, 0.36},
        {{"7", "", ""}, 0.35},
        {{"8", "", ""}, 0.37},
        {{"9", "", ""}, 0.23},
        {{"10", "", ""}, 0.27},
        {{"11", "", ""}, 0.45},
        {{"12", "", ""}, 0.45}
    };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[CrifRecord::RiskType::Commodity] = {
        {{"1", "", ""}, 0.83},
        {{"2", "", ""}, 0.97},
        {{"3", "", ""}, 0.93},
        {{"4", "", ""}, 0.97},
        {{"5", "", ""}, 0.98},
        {{"6", "", ""}, 0.9},
        {{"7", "", ""}, 0.98},
        {{"8", "", ""}, 0.49},
        {{"9", "", ""}, 0.8},
        {{"10", "", ""}, 0.46},
        {{"11", "", ""}, 0.58},
        {{"12", "", ""}, 0.53},
        {{"13", "", ""}, 0.62},
        {{"14", "", ""}, 0.16},
        {{"15", "", ""}, 0.18},
        {{"16", "", ""}, 0},
        {{"17", "", ""}, 0.38}
    };

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

    // clang-format ond
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

string SimmConfiguration_ISDA_V2_6::label2(const QuantLib::ext::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::label2(irIndex);
}

} // namespace analytics
} // namespace ore
