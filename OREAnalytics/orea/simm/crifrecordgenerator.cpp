/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

#include <orea/simm/crifrecordgenerator.hpp>
#include <orea/simm/simmbucketmapper.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/utilities.hpp>
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>

#include <ored/portfolio/referencedata.hpp>
#include <ored/report/report.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

#include <qle/utilities/inflation.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <ql/tuple.hpp>

#include <iomanip>
#include <map>

using namespace ore::analytics;

using ore::data::checkCurrency;
using ore::data::parseIborIndex;
using ore::data::parseZeroInflationIndex;
using ore::data::Report;
using ore::data::to_string;
using ore::data::PortfolioFieldGetter;
using QuantLib::Real;
using std::ostream;
using std::string;
using std::vector;

namespace {

QuantLib::ext::shared_ptr<QuantLib::IborIndex> getIborIndex(const string& indexName) { return parseIborIndex(indexName); }

} // namespace

namespace ore {
namespace analytics {

VolatilityDataCrif::VolatilityDataCrif(const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket) : crifMarket_(crifMarket) {}

double VolatilityDataCrif::vegaTimesVol(ore::analytics::RiskFactorKey::KeyType rfType, const std::string& rfName,
                                        double sensitivity, const std::string& expiryTenor,
                                        const std::string& underlyingTerm) {
    auto shiftData = getShiftData(rfType, rfName);
    if (shiftData.shiftType == ShiftType::Relative) {
        return sensitivity / shiftData.shiftSize;
    } else {
        double vol = getVolatility(rfType, rfName, expiryTenor, underlyingTerm);
        TLOG("For (" << rfType << ") sensitivity (" << rfName << "," << expiryTenor << "," << underlyingTerm << "), "
                     << std::fixed << std::setprecision(2) << "(sensi, atm_vol, shift_size) is (" << sensitivity
                     << std::setprecision(9) << "," << vol << "," << shiftData.shiftSize << ").");
        return sensitivity * vol / shiftData.shiftSize;
    }
}

Volatility VolatilityDataCrif::getVolatility(RiskFactorKey::KeyType rfType, const string& rfName,
                                             const string& expiryTenor, const string& underlyingTerm) {

    // Have we cached the volatility from a previous call.
    Key key{rfType, rfName, expiryTenor, underlyingTerm};
    auto it = volatilities_.find(key);
    if (it != volatilities_.end())
        return it->second;

    // If not cached, get the volatility from CrifMarket.
    TLOG("VolatilityDataCrif: volatility not cached for key: " << key << ".");

    Volatility vol;
    switch (rfType) {
    case RiskFactorKey::KeyType::OptionletVolatility: {
        auto ovs = crifMarket_->capFloorVol(rfName);
        QL_REQUIRE(!ovs.empty(), "VolatilityDataCrif: need non-empty optionlet structure "
                                     << "handle for currency " << rfName << ".");
        QL_REQUIRE(*ovs, "VolatilityDataCrif: need valid optionlet structure for currency " << rfName << ".");
        vol = ovs->volatility(parsePeriod(expiryTenor), Null<Real>());
        break;
    }
    case RiskFactorKey::KeyType::SwaptionVolatility: {
        auto svs = crifMarket_->swaptionVol(rfName);
        QL_REQUIRE(!svs.empty(), "VolatilityDataCrif: need non-empty swaption volatility structure "
                                     << "handle for currency " << rfName << ".");
        QL_REQUIRE(*svs, "VolatilityDataCrif: need valid swaption volatility structure for currency " << rfName << ".");
        vol = svs->volatility(parsePeriod(expiryTenor), parsePeriod(underlyingTerm), Null<Real>());
        break;
    }
    case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility: {
        QL_REQUIRE(crifMarket_, "VolatilityDataCrif: need non-empty crifMarket");
        QL_REQUIRE(crifMarket_->simMarket(), "VolatilityDataCrif: crifMarket need non-empty simMarket");

        auto cpivs = crifMarket_->simMarket()->cpiInflationCapFloorVolatilitySurface(rfName);
        auto index = crifMarket_->simMarket()->zeroInflationIndex(rfName);

        QL_REQUIRE(!cpivs.empty() && *cpivs,
                   "VolatilityDataCrif : need non - empty cpiInflationCapFloorVolatilitySurface for index " << rfName);
        QL_REQUIRE(!index.empty() && *index,
                   "VolatilityDataCrif : need non - empty zeroInflationIndex for index " << rfName);

        auto maturity = cpivs->optionDateFromTenor(parsePeriod(expiryTenor));
        double baseCPI = QuantExt::ZeroInflation::cpiFixing(*index, cpivs->baseDate() + cpivs->observationLag(),
                                                            cpivs->observationLag(), cpivs->indexIsInterpolated());
        double forwardCPI =
            QuantExt::ZeroInflation::cpiFixing(*index, maturity, cpivs->observationLag(), cpivs->indexIsInterpolated());
        double ttm = inflationYearFraction(cpivs->frequency(), cpivs->indexIsInterpolated(), cpivs->dayCounter(),
                                           cpivs->baseDate(), maturity - cpivs->observationLag());
        double atmStrike = std::pow(forwardCPI / baseCPI, 1. / ttm) - 1;

        vol = cpivs->volatility(parsePeriod(expiryTenor), atmStrike);
        break;
    }
    case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility: {

        QL_REQUIRE(crifMarket_, "VolatilityDataCrif: need non-empty crifMarket");
        QL_REQUIRE(crifMarket_->simMarket(), "VolatilityDataCrif: crifMarket need non-empty simMarket");

        auto yoyvs = crifMarket_->simMarket()->yoyCapFloorVol(rfName);
        auto index = crifMarket_->simMarket()->yoyInflationIndex(rfName);

        QL_REQUIRE(!yoyvs.empty() && *yoyvs,
                   "VolatilityDataCrif : need non - empty yoyCapFloorVol for index " << rfName);
        QL_REQUIRE(!index.empty() && *index,
                   "VolatilityDataCrif : need non - empty yoyInflationIndex for index " << rfName);

        auto maturity = yoyvs->optionDateFromTenor(parsePeriod(expiryTenor));
        auto fixingDate = maturity - yoyvs->observationLag();
        if (yoyvs->indexIsInterpolated()) {
            fixingDate = inflationPeriod(fixingDate, yoyvs->frequency()).first;
        }
        double forwardYoY = index->fixing(fixingDate);
        vol = yoyvs->volatility(fixingDate, forwardYoY, 0 * Days);
        break;
    }
    case RiskFactorKey::KeyType::FXVolatility: {
            QL_REQUIRE(rfName.size() == 6, "VolatilityDataCrif: expect FX vol name in CCY1CCY2");
            QL_REQUIRE(crifMarket_, "VolatilityDataCrif: need non-empty crifMarket");
            QL_REQUIRE(crifMarket_->simMarket(), "VolatilityDataCrif: crifMarket need non-empty simMarket");
            std::string forCcy = rfName.substr(0,3);
            std::string domCcy = rfName.substr(3);
            double spot = crifMarket_->simMarket()->fxSpot(rfName)->value();
            auto fxVolSurface = crifMarket_->simMarket()->fxVol(rfName);
            auto foreignDiscountCurve = crifMarket_->simMarket()->discountCurve(forCcy);
            auto domesticDiscountCurve = crifMarket_->simMarket()->discountCurve(domCcy);
            QuantLib::Date optionExpiryDate = fxVolSurface->optionDateFromTenor(parsePeriod(expiryTenor));
            double forward = spot * foreignDiscountCurve->discount(optionExpiryDate) / domesticDiscountCurve->discount(optionExpiryDate);
            vol = fxVolSurface->blackVol(optionExpiryDate, forward);
            break;
    } case RiskFactorKey::KeyType::EquityVolatility: {
            QL_REQUIRE(crifMarket_, "VolatilityDataCrif: need non-empty crifMarket");
            QL_REQUIRE(crifMarket_->simMarket(), "VolatilityDataCrif: crifMarket need non-empty simMarket");
            auto equityCurve = crifMarket_->simMarket()->equityCurve(rfName);
            auto eqVolSurface = crifMarket_->simMarket()->equityVol(rfName);
            QuantLib::Date optionExpiryDate = eqVolSurface->optionDateFromTenor(parsePeriod(expiryTenor));
            double forward = equityCurve->forecastFixing(optionExpiryDate);
            vol = eqVolSurface->blackVol(optionExpiryDate, forward);
            break;
    }case RiskFactorKey::KeyType::CommodityVolatility: {
            QL_REQUIRE(crifMarket_, "VolatilityDataCrif: need non-empty crifMarket");
            QL_REQUIRE(crifMarket_->simMarket(), "VolatilityDataCrif: crifMarket need non-empty simMarket");
            auto commodityCurve = crifMarket_->simMarket()->commodityPriceCurve(rfName);
            auto commVolSurface = crifMarket_->simMarket()->commodityVolatility(rfName);
            QuantLib::Date optionExpiryDate = commVolSurface->optionDateFromTenor(parsePeriod(expiryTenor));
            double forward = commodityCurve->price(optionExpiryDate);
            vol = commVolSurface->blackVol(optionExpiryDate, forward);
            break;
    } case RiskFactorKey::KeyType::YieldVolatility: {
            QL_REQUIRE(crifMarket_, "VolatilityDataCrif: need non-empty crifMarket");
            QL_REQUIRE(crifMarket_->simMarket(), "VolatilityDataCrif: crifMarket need non-empty simMarket");
            auto commodityCurve = crifMarket_->simMarket()->yieldVol(rfName);
            auto yieldVolSurface = crifMarket_->simMarket()->yieldVol(rfName);
            vol = yieldVolSurface->volatility(parsePeriod(expiryTenor), parsePeriod(underlyingTerm), Null<Real>());
            break;
    }
    default:
        QL_FAIL("VolatilityDataCrif: risk factor key type " << rfType << " not supported.");
    }

    volatilities_[key] = vol;
    TLOG("VolatilityDataCrif: cached volatility for key: " << key << ".");

    return vol;
}

ore::analytics::SensitivityScenarioData::ShiftData VolatilityDataCrif::getShiftData(RiskFactorKey::KeyType rfType, const string& rfName) {
    // Have we cached the shift size from a previous call.
    Key key{rfType, rfName, "", ""};
    auto it = shifts_.find(key);
    if (it != shifts_.end())
        return it->second;
    // If not cached, get the shift size from sensitivity data.
    TLOG("VolatilityDataCrif: shift size not cached for key: " << key << ".");
    // Get the sensitivity data from CrifMarket.
    const auto& ssd = crifMarket_->sensiData();
    auto shiftData = ssd->shiftData(rfType, rfName);
    shifts_[key] = shiftData;
    TLOG("VolatilityDataCrif: cached shift size for key: " << key << ".");
    return shiftData;
}

void VolatilityDataCrif::reset() {
    volatilities_.clear();
    shifts_.clear();
}

bool VolatilityDataCrif::Key::operator<(const VolatilityDataCrif::Key& key) const {
    return std::tie(rfType, rfName, expiryTenor, underlyingTerm) <
           std::tie(key.rfType, key.rfName, key.expiryTenor, key.underlyingTerm);
}

ostream& operator<<(ostream& os, const VolatilityDataCrif::Key& key) {
    return os << "(" << key.rfType << "," << key.rfName << "," << key.expiryTenor << "," << key.underlyingTerm << ")";
}

CrifRecordGenerator::CrifRecordGenerator(const QuantLib::ext::shared_ptr<ore::analytics::CrifConfiguration>& config,
                                         const QuantLib::ext::shared_ptr<SimmNameMapper>& nameMapper,
                                         const SimmTradeData& simmTradeData,
                                         const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket, bool xccyDiscounting,
                                         const std::string& currency, QuantLib::Real usdSpot,
                                         const QuantLib::ext::shared_ptr<ore::data::PortfolioFieldGetter>& fieldGetter,
                                         const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData,
                                         const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                                         const string& discountIndex)
    : config_(config), nameMapper_(nameMapper), tradeData_(simmTradeData), crifMarket_(crifMarket),
      xccyDiscounting_(xccyDiscounting), currency_(currency), usdSpot_(usdSpot), fieldGetter_(fieldGetter),
      referenceData_(referenceData), curveConfigs_(curveConfigs), discountIndex_(discountIndex),
      volatilityData_(crifMarket_) {

    baseCcyDiscLabel2_ = "OIS";
    if (!discountIndex_.empty()) {
        QuantLib::ext::shared_ptr<IborIndex> index = getIborIndex(discountIndex_);
        if (!QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index)) {
            string indexCcy = index->currency().code();
            QL_REQUIRE(indexCcy == currency_, "The currency, "
                                                  << indexCcy << ", of the discount index, " << discountIndex_
                                                  << ", must match the CRIF generator currency, " << currency_ << ".");
            baseCcyDiscLabel2_ = label2(index);
        }
    }
}

ore::analytics::CrifRecord CrifRecordGenerator::record(const SensitivityRecord& sr, CrifRecord::RiskType riskType,
                                                       const std::string& qualifier, const std::string& bucket,
                                                       const std::string& label1, const std::string& label2,
                                                       double sensitivity) {
    const ore::data::NettingSetDetails& tradeNettingSetDetails = tradeData_.nettingSetDetails(sr.tradeId);
    return CrifRecord(sr.tradeId, "", tradeNettingSetDetails, CrifRecord::ProductClass::Empty, riskType, qualifier,
                      bucket, label1, label2, currency_, sensitivity, usdSpot_ * sensitivity);
}

boost::optional<ore::analytics::CrifRecord> CrifRecordGenerator::operator()(const ore::analytics::SensitivityRecord& sr,
                                                                            std::set<std::string>& failedTrades) {
    // Split the sensitivity record factor description into tokens
    vector<string> rfTokens;
    boost::split(rfTokens, sr.desc_1, boost::is_any_of("/"), boost::token_compress_off);
    QL_REQUIRE(!rfTokens.empty(), "Expected one token at least for factor '" << sr.key_1 << "'");

    boost::optional<CrifRecord> result;
    try {
        CrifRecordData data;
        // Case statement to populate the remaining CRIF record entries
        switch (sr.key_1.keytype) {

        case RiskFactorKey::KeyType::DiscountCurve:
            // rfKey of form DiscountCurve/USD/# => qualifier = "USD"
            // rfTokens expected to be just a tenor e.g. 2W
            if (xccyDiscounting_ && sr.key_1.name != currency_) {
                data = xccyBasisImpl(sr.key_2.name, sr.delta);
            } else {
                data = discountCurveImpl(sr, rfTokens);
            }
            break;
        case RiskFactorKey::KeyType::IndexCurve:
            // rfKey of form "IndexCurve/USD-LIBOR-3M/#" => qualifier = "USD"
            // rfTokens expected to be just a tenor e.g. 3Y
            data = indexCurveImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::YieldCurve:
            data = yieldCurveImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::OptionletVolatility:
            data = irVolatilityImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::SwaptionVolatility:
            data = irVolatilityImpl(sr, rfTokens, true);
            break;
        case RiskFactorKey::KeyType::SurvivalProbability:
            data = survivalProbabilityCurveImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::YieldVolatility:
            data = yieldVolatilityImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::FXSpot:
            data = fxSpotImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::FXVolatility:
            data = fxVolatilityImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::EquitySpot:
            data = equitySpotImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::EquityVolatility:
            data = equityVolatilityImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::CDSVolatility: {
            data = cdsVolatilityImpl(sr, rfTokens);
            break;
        }
        case RiskFactorKey::KeyType::BaseCorrelation:
            data = baseCorrelationImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::CommodityCurve:
            data = commodityCurveImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::CommodityVolatility:
            data = commodityVolatilityImpl(sr, rfTokens);
            break;

        case RiskFactorKey::KeyType::YoYInflationCurve:
        case RiskFactorKey::KeyType::ZeroInflationCurve:
            data = inflationCurveImpl(sr, rfTokens);
            break;
        case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility:
        case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility: {
            data = inflationVolatilityImpl(sr, rfTokens);
            break;
        }
        case RiskFactorKey::KeyType::DividendYield:
            LOG("CRIF: Skip dividend yield factor " << sr.key_1.name << " for trade " << sr.tradeId
                                                    << " as it is not needed.");
            break;

        case RiskFactorKey::KeyType::SecuritySpread:
            // TODO:
            // Do we need this risk factor for SIMM?
            WLOG("CRIF: skip bond security spread sensitivity factor  " << sr.key_1.name << " for trade "
                                                                        << sr.tradeId);
            break;
        case RiskFactorKey::KeyType::Correlation:
            LOG("CRIF: Skip Correlation factor " << sr.key_1.name << " for trade " << sr.tradeId
                                                 << " as it is not needed.");
            break;
        default:
            QL_FAIL("CRIF: unexpected risk factor key " << sr.key_1.keytype);
            break;
        }
        // If riskType has been set, we should be able to create a valid CrifRecord
        if (data.riskType != CrifRecord::RiskType::Empty) {
            return record(sr, data.riskType, data.qualifier, data.bucket, data.label1, data.label2, data.sensitivity);
        }
    } catch (std::exception& e) {
        failedTrades.insert(sr.tradeId);
        auto subFields = map<string, string>({{"TradeId", sr.tradeId}, {"RiskFactorKey", to_string(sr.key_1)}});
        StructuredAnalyticsErrorMessage(
            "CRIF Generation", "Failed to generate CRIF Record",
            "Excluding trade " + sr.tradeId + " from CRIF generation due to error: " + to_string(e.what()), subFields)
            .log();
    }
    return result;
}

CrifRecordData CrifRecordGenerator::discountCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                                      const std::vector<std::string>& rfTokens) {
    // rfKey of form DiscountCurve/USD/# => qualifier = "USD"
    // rfTokens expected to be just a tenor e.g. 2W
    CrifRecordData data = defaultRecord(sr, rfTokens, true, true);
    if (data.qualifier == currency_) {
        data.label2 = baseCcyDiscLabel2_;
    } else {
        // Deduce Label2 value for the discount currency.
        data.label2 = label2(data.qualifier);
    }
    return data;
}

CrifRecordData CrifRecordGenerator::yieldCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                                   const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    // 1: rfKey of form "YieldCurve/CCY1-IN-CCY2/#" => qualifier = "CCY1", tenor defaults to 1D
    // 2: rfKey of form "YieldCurve/CURVENAME-CCY-TENOR/#" => qualifier = "CCY", tenor = TENOR
    //    rfKey of form "YieldCurve/CURVENAME-CCY-MUN/#" => qualifier = "CCY", tenor = Municipal
    // 3: rfKey of form "YieldCurve/CURVENAME-CCY/#" => qualifier = "CCY", tenor defaults to 1D
    // 4: rfKey of form "YieldCurve/CMB-A-B-....-TENOR/#" => qualifier from reference data, tenor = TENOR
    //    rfKey of form "YieldCurve/CMB-A-B-....-MUN/#" => qualifier from reference data, tenor = Municipal
    // rfTokens expected to be just a tenor e.g. 3Y
    std::vector<std::string> tokens;
    std::string period;
    std::string originalQualifier = crifQualifier(sr.key_1.name);
    boost::split(tokens, originalQualifier, boost::is_any_of("-"));
    if (tokens.size() >= 3 && tokens[0] == "CMB") {
        // Case 4: CMB-A-B-....-TENOR or CMB-A-B-....-MUN
        // Try looking up the yield curve config to determine the currency
        if (curveConfigs_->hasYieldCurveConfig(originalQualifier)) {
            LOG("Found yield curve config for qualifier " << originalQualifier);
            auto ycc = curveConfigs_->yieldCurveConfig(originalQualifier);
            data.qualifier = ycc->currency();
        } else {
            WLOG("Yield curve config for qualifier " << originalQualifier << " not found");

            // Next, try finding bond reference data, assuming that the yield curve name contains
            // the security id. Try cutting off the last tokens first.
            std::string security = tokens[0];
            for (Size i = 1; i < tokens.size() - 1; ++i)
                security = security + "-" + tokens[i];

            WLOG("Look up security " << security);
            auto ref = referenceData_->getData("Bond", security);
            // else try the full name
            if (!ref) {
                WLOG("Look up security " << originalQualifier);
                ref = referenceData_->getData("Bond", originalQualifier);
            }
            QL_REQUIRE(ref, "bond reference data not found for id " << security);
            auto bondref = QuantLib::ext::dynamic_pointer_cast<BondReferenceDatum>(ref);
            QL_REQUIRE(bondref, "cast to BondReferenceDatum has failed");
            data.qualifier = bondref->bondData().legData.front().currency();
        }

        // in any case:
        period = tokens.back();
    } else if (tokens.size() == 3 && tokens[1] == "IN") {
        // Case "CCY1-IN-CCY2"
        data.qualifier = tokens[0];
        period = "1D";
    } else if ((tokens.size() == 2 || tokens.size() == 3) && tokens[1].size() == 3) {
        // Cases 2 and 3: CURVENAME-CCY or CURVENAME-CCY-TENOR
        data.qualifier = tokens[1];
        period = tokens.size() == 3 ? tokens[2] : "1D";
    } else {
        QL_FAIL("CRIF: Unexpected yield curve name format '"
                << originalQualifier
                << "'. Expected CCY1-IN-CCY2, CURVENAME-CCY, CURVENAME-CCY-TENOR, CMB-A-B-...-TENOR, "
                   "CMB-A-B-...-MUN.");
    }

    // Special treatment for yield curves that are used as xccy discount curves. They curve name should be of
    // the form CURVENAME-CCY with CURVENAME being the internally reserved prefix for xccy yield curves.
    if (tokens.size() == 2 && tokens[0] == ore::data::xccyCurveNamePrefix) {
        if (data.qualifier != currency_) {
            data = xccyBasisImpl(data.qualifier, sr.delta);
        } else {
            // We shouldn't really get here but leave it in just in case.
            data = defaultRecord(sr, rfTokens, true, true);
            // Deduce Label2 value for the discount currency.
            data.label2 = label2(data.qualifier);
        }
    } else {
        data.riskType = ore::analytics::CrifRecord::RiskType::IRCurve;
        Period p;
        if (period != "MUN") {
            try {
                p = parsePeriod(period);
                data.label2 = label2(p);
            } catch (...) {
                QL_FAIL("CRIF: YieldCurve risk factor '"
                        << originalQualifier << "' contains illegal tenor '" << period
                        << "'. Expected CCY1-IN-CCY2, CURVENAME-CCY, CURVENAME-CCY-TENOR, "
                           "CMB-A-B-...-TENOR, CMB-A-B-...-MUN.");
            }
        } else
            data.label2 = "Municipal";

        data.bucket = bucket(data.riskType, data.qualifier);
        data.label1 = tenorLabel(rfTokens.front());
    }
    data.sensitivity = sr.delta;
    return data;
}

CrifRecordData CrifRecordGenerator::indexCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                                   const std::vector<std::string>& rfTokens) {
    // rfKey of form "IndexCurve/USD-LIBOR-3M/#" => qualifier = "USD"
    // rfTokens expected to be just a tenor e.g. 3Y
    CrifRecordData data;
    data.riskType = riskTypeImpl(sr.key_1.keytype);
    data.label1 = tenorLabel(rfTokens.front());
    auto index = parseIborIndex(crifQualifier(sr.key_1.name));
    data.qualifier = index->currency().code();
    data.bucket = bucket(data.riskType, data.qualifier);
    data.label2 = label2(index);
    data.sensitivity = sr.delta;
    return data;
}

CrifRecordData CrifRecordGenerator::irVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                     const std::vector<std::string>& rfTokens, bool swaptionVol) {

    auto data = defaultRecord(sr, rfTokens, true, false);

    std::string volKey = data.qualifier;
    QuantLib::ext::shared_ptr<IborIndex> dummy;
    if (tryParseIborIndex(volKey, dummy)) {
        auto index = getIborIndex(data.qualifier);
        data.qualifier = index->currency().code();
    }

    // rfTokens of the form EXPIRY_TENOR/ATM (e.g. 2Y/ATM) or EXPIRY_TENOR/STRIKE (e.g. 3Y/0.0200)

    // For CRIF, the optionlet vega should be (atm vol in vol units) * [V(vol + 1) - V(vol)] where again 1 is
    // understood to be in the units of the vol. So, we could have the following two examples:
    // 1) a normal ATM vol of 49 bps, we should have 49 * [V(50) - V(49)]
    // 2) a lognormal ATM vol of 49%, we should have 49 * [V(50) - V(49)]
    // We shift in absolute units so we have stored abs_atm_vol in our vol curve below and we have an
    // abs_shift_size when generating sensitivities. abs_shift_size is generally n x (unit of vol) and n
    // is generally 1. So, for normal vol shift, it will generally be 1 bp and for lognormal vol shift,
    // it will generally be 1%. To convert our sensitivity, sensi = V(abs_vol + abs_shift_size) - V(abs_vol),
    // in to what is requested for the CRIF, we need: abs_atm_vol * sensi / abs_shift_size.
    // More details in risk_catalogue.pdf in section 'CRIF Generation' - 'Vega Risk'
    // Also feedback from ISDA in ticket 1017 in openxva.

    auto shiftData = volatilityData_.getShiftData(sr.key_1.keytype, volKey);
    double shiftSize = shiftData.shiftSize;
    if (shiftData.shiftType == ShiftType::Relative) {
        data.sensitivity = sr.delta / shiftSize;
    } else {
        Volatility vol;
        const string& tenor = rfTokens.front();
        if (swaptionVol) {
            QL_REQUIRE(rfTokens.size() > 1, "rfTokens doesnt have a underlying maturity");
            const string& swapMaturity = rfTokens[1];
            vol = volatilityData_.getVolatility(sr.key_1.keytype, volKey, tenor, swapMaturity);
        } else {
            vol = volatilityData_.getVolatility(sr.key_1.keytype, volKey, tenor);
        }

        // Update the sensitivity to give the optionlet vega expected in the CRIF (before summing at the expiry
        // level).
        data.sensitivity = vol * sr.delta / shiftSize;
    }
    return data;
}

CrifRecordData CrifRecordGenerator::fxSpotImpl(const ore::analytics::SensitivityRecord& sr,
                                               const std::vector<std::string>& rfTokens) {
    // rfKey of form "FXSpot/CCYUSD/0" and rfTokens is "spot"
    //  => qualifier = the non-USD currency
    CrifRecordData data;
    data.riskType = riskTypeImpl(sr.key_1.keytype);
    string qualifier = crifQualifier(sr.key_1.name);
    QL_REQUIRE(qualifier.length() == 6, "CRIF: Expected a string of length 6 for currency pair but got " << qualifier);
    auto ccy_1 = qualifier.substr(0, 3);
    auto ccy_2 = qualifier.substr(3);
    QL_REQUIRE(ccy_2 == currency_,
               "CRIF: Expected the FX spot sensitivity to be of form CCY" << currency_ << " but got " << qualifier);
    QL_REQUIRE(ccy_1 != ccy_2, "CRIF: Expected currency pair with different currencies but both are " << ccy_1);
    data.qualifier = ccy_1;

    // Check the shift type is relative and scale sensitivity to align with 1% relative shift
    auto p = volatilityData_.getShiftData(sr.key_1.keytype, sr.key_1.name);
    QL_REQUIRE(p.shiftType == ShiftType::Relative, "CrifGenerator: expected FXSpot shift to be relative.");
    data.sensitivity = sr.delta / (100. * p.shiftSize);

    return data;
}

CrifRecordData CrifRecordGenerator::fxVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                     const std::vector<std::string>& rfTokens) {

    // rfKey of form "FXVolatility/CCY_1CCY_2/#" => qualifier = "CCY_1CCY_2"
    // rfTokens of the form EXPIRY_TENOR/ATM (e.g. 2Y/ATM) or EXPIRY_TENOR/STRIKE (e.g. 3Y/0.0200)
    CrifRecordData data = defaultRecord(sr, rfTokens, true, false);
    return data;
}

CrifRecordData CrifRecordGenerator::equitySpotImpl(const ore::analytics::SensitivityRecord& sr,
                                                   const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("CrifRecordGenerator: Equity spot sensitivity not covered, returning empty CRIF record data");
    return data;
}

CrifRecordData CrifRecordGenerator::equityVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                         const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("CrifRecordGenerator: Equity volatility sensitivity not covered, returning empty CRIF record data");
    return data;
}

CrifRecordData CrifRecordGenerator::commodityCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                                       const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("CrifRecordGenerator: Commodity curve sensitivity not covered, returning empty CRIF record data");
    return data;
}

CrifRecordData CrifRecordGenerator::commodityVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                            const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("CrifRecordGenerator: Commodity volatility sensitivity not covered, returning empty CRIF record data");
    return data;
}

CrifRecordData CrifRecordGenerator::inflationCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                                       const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("CrifRecordGenerator: Inflation curve sensitivity not covered, returning empty CRIF record data");
    return data;
}

CrifRecordData CrifRecordGenerator::inflationVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                            const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("CrifRecordGenerator: Inflation volatility sensitivity not covered, returning empty CRIF record data");
    return data;
}

//! Handle special case if all non basecurrency discount curves are treated as xccy basis risk
CrifRecordData CrifRecordGenerator::xccyBasisImpl(const std::string qualifier, double sensitivity) {
    CrifRecordData data;
    data.riskType = CrifRecord::RiskType::XCcyBasis;
    data.sensitivity = sensitivity;
    data.qualifier = crifQualifier(qualifier);
    if (data.qualifier == "USD") {
        // SIMM requires cross currency basis spread sensitivities to be the sensitivity to a 1bp shift on
        // the non-USD leg vs USD flat. We assume here that if the currency of calculation, call it CCY, is
        // not USD then the USD cross currency par instruments have been set up with a spread on the CCY leg
        // so that the USD par discount sensitivity is just the Risk_XCcyBasis for currency CCY in CRIF
        // terminology
        data.qualifier = currency_;
    }
    return data;
}

CrifRecordData SimmRecordGenerator::survivalProbabilityCurveImpl(const ore::analytics::SensitivityRecord& sr,
                                                                 const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("CrifRecordGenerator: Survival probability curve sensitivity not covered, returning empty CRIF record data");
    return data;
}

CrifRecordData CrifRecordGenerator::yieldVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                        const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("CrifRecordGenerator: Yield volatility sensitivity not covered, returning empty CRIF record data");
    return data;
}

double CrifRecordGenerator::CdsAtmVol(const std::string& tradeId, const std::string& optionExpiryTenor) const {
    Real atmVol = 0.0;
    QL_FAIL("CrifRecordGenerator::CdsAtmVol not implemented");
    return atmVol;
}

SimmRecordGenerator::SimmRecordGenerator(const QuantLib::ext::shared_ptr<SimmConfiguration>& simmConfiguration,
                                         const QuantLib::ext::shared_ptr<SimmNameMapper>& nameMapper,
                                         const SimmTradeData& tradeData,
                                         const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket, bool xccyDiscounting,
                                         const std::string& currency, QuantLib::Real usdSpot,
                                         const QuantLib::ext::shared_ptr<ore::data::PortfolioFieldGetter>& fieldGetter,
                                         const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData,
                                         const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                                         const std::string& discountIndex)
    : CrifRecordGenerator(simmConfiguration, nameMapper, tradeData, crifMarket, xccyDiscounting, currency, usdSpot,
                          fieldGetter, referenceData, curveConfigs, discountIndex) {}

CrifRecordData SimmRecordGenerator::cdsVolatilityImpl(const ore::analytics::SensitivityRecord& sr,
                                                      const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("SimmRecordGenerator: CDS volatility sensitivity not covered, returning empty CRIF record data");
    return data;
}

CrifRecordData SimmRecordGenerator::baseCorrelationImpl(const ore::analytics::SensitivityRecord& sr,
                                                        const std::vector<std::string>& rfTokens) {
    CrifRecordData data;
    ALOG("SimmRecordGenerator: Base correlation sensitivity not covered, returning empty CRIF record data");
    return data;
}

ore::analytics::CrifRecord::RiskType
SimmRecordGenerator::riskTypeImpl(const ore::analytics::RiskFactorKey::KeyType& rfKeyType) {
    static std::map<ore::analytics::RiskFactorKey::KeyType, ore::analytics::CrifRecord::RiskType> mapping = {
        {ore::analytics::RiskFactorKey::KeyType::DiscountCurve, ore::analytics::CrifRecord::RiskType::IRCurve},
        {ore::analytics::RiskFactorKey::KeyType::IndexCurve, ore::analytics::CrifRecord::RiskType::IRCurve},
        {ore::analytics::RiskFactorKey::KeyType::YieldCurve, ore::analytics::CrifRecord::RiskType::IRCurve},
        {ore::analytics::RiskFactorKey::KeyType::BaseCorrelation, ore::analytics::CrifRecord::RiskType::BaseCorr},
        {ore::analytics::RiskFactorKey::KeyType::CommodityCurve, ore::analytics::CrifRecord::RiskType::Commodity},
        {ore::analytics::RiskFactorKey::KeyType::CommodityVolatility,
         ore::analytics::CrifRecord::RiskType::CommodityVol},
        {ore::analytics::RiskFactorKey::KeyType::EquitySpot, ore::analytics::CrifRecord::RiskType::Equity},
        {ore::analytics::RiskFactorKey::KeyType::EquityVolatility, ore::analytics::CrifRecord::RiskType::EquityVol},
        {ore::analytics::RiskFactorKey::KeyType::FXSpot, ore::analytics::CrifRecord::RiskType::FX},
        {ore::analytics::RiskFactorKey::KeyType::FXVolatility, ore::analytics::CrifRecord::RiskType::FXVol},
        {ore::analytics::RiskFactorKey::KeyType::OptionletVolatility, ore::analytics::CrifRecord::RiskType::IRVol},
        {ore::analytics::RiskFactorKey::KeyType::SwaptionVolatility, ore::analytics::CrifRecord::RiskType::IRVol},
        {ore::analytics::RiskFactorKey::KeyType::YieldVolatility, ore::analytics::CrifRecord::RiskType::IRVol},
        {ore::analytics::RiskFactorKey::KeyType::YoYInflationCapFloorVolatility,
         ore::analytics::CrifRecord::RiskType::InflationVol},
        {ore::analytics::RiskFactorKey::KeyType::YoYInflationCurve, ore::analytics::CrifRecord::RiskType::Inflation},
        {ore::analytics::RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility,
         ore::analytics::CrifRecord::RiskType::InflationVol},
        {ore::analytics::RiskFactorKey::KeyType::ZeroInflationCurve, ore::analytics::CrifRecord::RiskType::Inflation}};
    auto it = mapping.find(rfKeyType);
    if (it != mapping.end()) {
        return it->second;
    } else {
        StructuredAnalyticsWarningMessage("SIMM Record Generation", "Internal error",
                                          "Couldnt not find a riskType for riskFactorKey " + to_string(rfKeyType));
        return CrifRecord::RiskType::Empty;
    }
}

ore::analytics::CrifRecord SimmRecordGenerator::record(const SensitivityRecord& sr, CrifRecord::RiskType riskType,
                                                       const std::string& qualifier, const std::string& bucket,
                                                       const std::string& label1, const std::string& label2,
                                                       double sensitivity) {
    const std::string tradeId = sr.tradeId;
    CrifRecord::ProductClass productClass = CrifRecord::ProductClass::Empty;
    if (tradeData_.hasAttributes(tradeId))
        productClass = tradeData_.getAttributes(tradeId).getSimmProductClass();

    map<string, string> additionalFields;
    set<CrifRecord::Regulation> simmCollectRegs;
    set<CrifRecord::Regulation> simmPostRegs;

    auto tc = tradeCache_.find(tradeId);

    if (tc != tradeCache_.end()) {
        additionalFields = tc->second.additionalFields;
        simmCollectRegs = tc->second.simmCollectRegs;
        simmPostRegs = tc->second.simmPostRegs;
    } else {

        // Get additional fields for inclusion in CRIF if field getter is not null
        if (fieldGetter_) {
            additionalFields = fieldGetter_->fields(tradeId);
            additionalFields.erase("im_model");
        }

        if (tradeData_.hasAttributes(tradeId)) {
            simmCollectRegs = tradeData_.getAttributes(tradeId).getSimmCollectRegulations();
            simmPostRegs = tradeData_.getAttributes(tradeId).getSimmPostRegulations();
        }

        tradeCache_[tradeId] = TradeCache{additionalFields, simmCollectRegs, simmPostRegs};
    }
    const ore::data::NettingSetDetails& tradeNettingSetDetails = tradeData_.nettingSetDetails(tradeId);
    return CrifRecord(tradeId, tradeData_.getAttributes(tradeId).getTradeType(), tradeNettingSetDetails, productClass,
                      riskType, qualifier, bucket, label1, label2, currency_, sensitivity, usdSpot_ * sensitivity,
                      CrifRecord::IMModel::SIMM, simmCollectRegs, simmPostRegs, "", additionalFields);
}

} // namespace analytics
} // namespace ore
