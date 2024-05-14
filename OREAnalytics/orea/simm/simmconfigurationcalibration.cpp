/*
 Copyright (C) 2023 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <orea/simm/simmconcentrationcalibration.hpp>
#include <orea/simm/simmconfigurationcalibration.hpp>
#include <ored/utilities/parsers.hpp>
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

string SimmConfigurationCalibration::group(const string& qualifier,
                                           const std::map<string, std::set<string>>& categories) const {
    string result;
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

QuantLib::Real SimmConfigurationCalibration::weight(const RiskType& rt, boost::optional<string> qualifier,
                                                   boost::optional<string> label_1,
                                                   const string& calculationCurrency) const {

    if (rt == RiskType::FX) {
        QL_REQUIRE(calculationCurrency != "", "no calculation currency provided weight");
        QL_REQUIRE(qualifier, "need a qualifier to return a risk weight for the risk type FX");

        const string label1 = group(calculationCurrency, ccyGroups_);
        const string label2 = group(*qualifier, ccyGroups_);
        auto label12Key = makeKey("", label1, label2);
        return rwBucket_.at(RiskType::FX).at(label12Key);
    }

    return SimmConfigurationBase::weight(rt, qualifier, label_1);
}

QuantLib::Real SimmConfigurationCalibration::correlation(const RiskType& firstRt, const string& firstQualifier,
                                                        const string& firstLabel_1, const string& firstLabel_2,
                                                        const RiskType& secondRt, const string& secondQualifier,
                                                        const string& secondLabel_1, const string& secondLabel_2,
                                                        const string& calculationCurrency) const {

    if (firstRt == RiskType::FX && secondRt == RiskType::FX) {
        QL_REQUIRE(calculationCurrency != "", "no calculation currency provided corr");
        const string bucket = group(calculationCurrency, ccyGroups_);
        const string label1 = group(firstQualifier, ccyGroups_);
        const string label2 = group(secondQualifier, ccyGroups_);
        auto key = makeKey(bucket, label1, label2);

        if (intraBucketCorrelation_.at(RiskType::FX).find(key) != intraBucketCorrelation_.at(RiskType::FX).end()) {
            return intraBucketCorrelation_.at(RiskType::FX).at(key);
        } else {
            QL_FAIL("Could not find FX intrabucket correlation, calculation currency '"
                    << calculationCurrency << "', firstQualifier '" << firstQualifier << "', secondQualifier '"
                    << secondQualifier << "'.");
        }
    }

    return SimmConfigurationBase::correlation(firstRt, firstQualifier, firstLabel_1, firstLabel_2, secondRt,
                                              secondQualifier, secondLabel_1, secondLabel_2);
}

SimmConfigurationCalibration::SimmConfigurationCalibration(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                           const QuantLib::ext::shared_ptr<SimmCalibration>& simmCalibration,
                                                           const QuantLib::Size& mporDays, const string& name)
    : SimmConfigurationBase(simmBucketMapper, name, "", mporDays) {

    version_ = simmCalibration->version();

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentrationCalibration>(simmCalibration, simmBucketMapper_);
    } else {
        // SIMM:Technical Paper, Section I.4: "The Concentration Risk feature is disabled"
        simmConcentration_ = QuantLib::ext::make_shared<SimmConcentrationBase>();
    }

    for (const auto& [riskClass, rcData] : simmCalibration->riskClassData()) {

        // Risk weights
        const auto& riskWeights = rcData->riskWeights();

        // Currency lists for FX
        // Populate CCY groups that are used for FX correlations and risk weights
        // The groups consists of High Vol Currencies & regular vol currencies
        if (riskClass == RiskClass::FX) {
            const auto& fxRiskWeights = QuantLib::ext::dynamic_pointer_cast<SimmCalibration::RiskClassData::FXRiskWeights>(riskWeights);
            QL_REQUIRE(fxRiskWeights, "Cannot cast RiskWeights to FXRiskWeights");
            const auto& ccyLists = fxRiskWeights->currencyLists();

            for (const auto& [ccyKey, ccyList] : ccyLists) {
                const string& bucket = std::get<0>(ccyKey);
                for (const string& ccy : ccyList) {
                    if (ccy == "Other") {
                        ccyGroups_[bucket].clear();
                    } else {
                        ccyGroups_[bucket].insert(ccy);
                    }
                }
            }
        }

        const auto& [deltaRiskType, vegaRiskType] = SimmConfiguration::riskClassToRiskType(riskClass);

        // Risk weights unique to each risk class
        auto singleRiskWeights = riskWeights->uniqueRiskWeights();
        for (const auto& [riskType, rw] : singleRiskWeights) {
            rwRiskType_[riskType] = ore::data::parseReal(rw.at(mporDays_)->value());
        }

        // Delta risk weights
        const auto& deltaRiskWeights = riskWeights->delta().at(mporDays_);
        const auto& vegaRiskWeights = riskWeights->vega().at(mporDays_);

        if (deltaRiskWeights.size() == 1) {
            rwRiskType_[deltaRiskType] = ore::data::parseReal(deltaRiskWeights.begin()->second);
        } else {
            // IR risk weights
            if (riskClass == RiskClass::InterestRate) {
                for (const auto& [rwKey, weight] : deltaRiskWeights) {
                    rwLabel_1_[deltaRiskType][rwKey] = ore::data::parseReal(weight);
                }
            } else {
                for (const auto& [rwKey, weight] : deltaRiskWeights) {
                    rwBucket_[deltaRiskType][rwKey] = ore::data::parseReal(weight);
                }
            }
        }

        // Vega risk weights
        if (vegaRiskWeights.size() == 1) {
            rwRiskType_[vegaRiskType] = ore::data::parseReal(vegaRiskWeights.begin()->second);
            if (vegaRiskType == RiskType::IRVol)
                rwRiskType_[RiskType::InflationVol] = rwRiskType_[RiskType::IRVol];
        } else {
            for (const auto& [rwKey, weight] : vegaRiskWeights) {
                rwBucket_[vegaRiskType][rwKey] = ore::data::parseReal(weight);
            }
        }

        // Historical volatility ratio
        // IR
        if (riskClass == RiskClass::InterestRate) {
            hvr_ir_ = ore::data::parseReal(riskWeights->historicalVolatilityRatio().at(mporDays)->value());
        }
        // EQ, COMM, FX
        if (riskClass == RiskClass::Equity || riskClass == RiskClass::Commodity || riskClass == RiskClass::FX)
            historicalVolatilityRatios_[vegaRiskType] =
                ore::data::parseReal(riskWeights->historicalVolatilityRatio().at(mporDays_)->value());

        // Correlations
        const auto& correlations = rcData->correlations();

        // Interbucket and Intrabucket correlations
        for (const auto& [corrKey, corr] : rcData->correlations()->intraBucketCorrelations()) {
            if (riskClass == RiskClass::CreditQualifying || riskClass == RiskClass::CreditNonQualifying) {
                const string& label1 = std::get<1>(corrKey);
                const string& label2 = std::get<2>(corrKey);
                Real correlation = ore::data::parseReal(corr);

                if (riskClass == RiskClass::CreditQualifying) {
                    if (label1 == "aggregate") {
                        if (label2 == "same") {
                            crqSameIntraCorr_ = correlation;
                        } else {
                            crqDiffIntraCorr_ = correlation;
                        }
                    } else {
                        crqResidualIntraCorr_ = correlation;
                    }
                } else {
                    if (label1 == "aggregate") {
                        if (label2 == "same") {
                            crnqSameIntraCorr_ = correlation;
                        } else {
                            crnqDiffIntraCorr_ = correlation;
                        }
                    } else {
                        crnqResidualIntraCorr_ = correlation;
                    }
                }
            } else {
                intraBucketCorrelation_[deltaRiskType][corrKey] = ore::data::parseReal(corr);
            }
        }
        for (const auto& [corrKey, corr] : rcData->correlations()->interBucketCorrelations()) {
            interBucketCorrelation_[deltaRiskType][corrKey] = ore::data::parseReal(corr);
        }

        // Correlations unique to other risk class
        if (riskClass == RiskClass::InterestRate) {
            const auto& irCorrelations = QuantLib::ext::dynamic_pointer_cast<SimmCalibration::RiskClassData::IRCorrelations>(correlations);
            irSubCurveCorr_ = ore::data::parseReal(irCorrelations->subCurves()->value());
            infCorr_ = ore::data::parseReal(irCorrelations->inflation()->value());
            infVolCorr_ = ore::data::parseReal(irCorrelations->inflation()->value());
            xccyCorr_ = ore::data::parseReal(irCorrelations->xCcyBasis()->value());
            irInterCurrencyCorr_ = ore::data::parseReal(irCorrelations->outer()->value());
        } else if (riskClass == RiskClass::FX) {
            const auto& fxCorrelations =
                QuantLib::ext::dynamic_pointer_cast<SimmCalibration::RiskClassData::FXCorrelations>(correlations);
            fxCorr_ = ore::data::parseReal(fxCorrelations->volatility()->value());
        } else if (riskClass == RiskClass::CreditQualifying) {
            const auto& creditQCorrelations =
                QuantLib::ext::dynamic_pointer_cast<SimmCalibration::RiskClassData::CreditQCorrelations>(correlations);
            basecorrCorr_ = ore::data::parseReal(creditQCorrelations->baseCorrelation()->value());
        } else if (riskClass == RiskClass::CreditNonQualifying) {
            crnqInterCorr_ = ore::data::parseReal(correlations->interBucketCorrelations().begin()->second);
        }
    }

    mapBuckets_ = {{RiskType::IRCurve, {"1", "2", "3"}},
                   {RiskType::CreditQ, {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual"}},
                   {RiskType::CreditVol, {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual"}},
                   {RiskType::CreditNonQ, {"1", "2", "Residual"}},
                   {RiskType::CreditVolNonQ, {"1", "2", "Residual"}},
                   {RiskType::Equity, {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual"}},
                   {RiskType::EquityVol, {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual"}},
                   {RiskType::Commodity,
                    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17"}},
                   {RiskType::CommodityVol,
                    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17"}}};

    mapLabels_1_ = {
        {RiskType::IRCurve, {"2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y"}},
        {RiskType::CreditQ, {"1y", "2y", "3y", "5y", "10y"}},
        {RiskType::CreditNonQ, {"1y", "2y", "3y", "5y", "10y"}},
        {RiskType::IRVol, {"2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y"}},
        {RiskType::InflationVol, {"2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y"}},
        {RiskType::CreditVol, {"1y", "2y", "3y", "5y", "10y"}},
        {RiskType::CreditVolNonQ, {"1y", "2y", "3y", "5y", "10y"}},
        {RiskType::EquityVol, {"2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y"}},
        {RiskType::CommodityVol, {"2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y"}},
        {RiskType::FXVol, {"2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y"}}};

    mapLabels_2_ = {{RiskType::IRCurve, {"OIS", "Libor1m", "Libor3m", "Libor6m", "Libor12m", "Prime", "Municipal"}},
                    {RiskType::CreditQ, {"", "Sec"}}};

    // Valid risk types
    validRiskTypes_ = {RiskType::Commodity,
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
                       RiskType::AddOnFixedAmount};

    // Curvature weights
    // Hardcoded weights - doesn't seem to change much across versions
    // clang-format off
    if (mporDays_ == 10) {
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
    } else {
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
    }
    // clang-format on
    curvatureWeights_[RiskType::InflationVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::EquityVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::CommodityVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::FXVol] = curvatureWeights_[RiskType::IRVol];
    curvatureWeights_[RiskType::CreditVolNonQ] = curvatureWeights_[RiskType::CreditVol];

    // Risk class correlation matrix
    const auto& rcCorrelations = simmCalibration->riskClassCorrelations();
    for (const auto& [rcCorrKey, rcCorr] : rcCorrelations) {
        riskClassCorrelation_[rcCorrKey] = ore::data::parseReal(rcCorr);
    }
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 8 section 11(d)
of the ISDA-SIMM-v2.6 documentation).
*/
QuantLib::Real SimmConfigurationCalibration::curvatureMarginScaling() const { return pow(hvr_ir_, -2.0); }

void SimmConfigurationCalibration::addLabels2(const RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfigurationCalibration::label2(const QuantLib::ext::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::label2(irIndex);
}

} // namespace analytics
} // namespace ore
