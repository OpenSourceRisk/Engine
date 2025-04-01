/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <orea/simm/simmbucketmapper.hpp>
#include <orea/simm/crifgenerator.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/crifrecordgenerator.hpp>
#include <orea/simm/utilities.hpp>

#include <ored/portfolio/referencedata.hpp>
#include <ored/report/report.hpp>
#include <ored/utilities/currencyhedgedequityindexdecomposition.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/utilities/inflation.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <ql/tuple.hpp>
#include <ql/errors.hpp>

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
namespace ore {
namespace analytics {

typedef ore::analytics::CrifRecord::RiskType RiskType;

CrifGenerator::CrifGenerator(const QuantLib::ext::shared_ptr<SimmConfiguration>& simmConfiguration,
                             const QuantLib::ext::shared_ptr<SimmNameMapper>& nameMapper,
			     const QuantLib::ext::shared_ptr<SimmTradeData>& tradeData,
                             const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket, bool xccyDiscounting,
                             const std::string& currency, QuantLib::Real usdSpot,
                             const QuantLib::ext::shared_ptr<ore::data::PortfolioFieldGetter>& fieldGetter,
                             const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData,
                             const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                             const string& discountIndex)
    : simmConfiguration_(simmConfiguration), nameMapper_(nameMapper), tradeData_(tradeData), crifMarket_(crifMarket),
      xccyDiscounting_(xccyDiscounting), currency_(currency), fieldGetter_(fieldGetter), usdSpot_(usdSpot),
      referenceData_(referenceData), curveConfigs_(curveConfigs), discountIndex_(discountIndex),
      hasNettingSetDetails_(tradeData->hasNettingSetDetails()) {

    QL_REQUIRE(checkCurrency(currency_), "Expected a valid base currency for crif generation but got " << currency_);
    QL_REQUIRE(!isSimmNonStandardCurrency(currency_),
               "Expected a standard simm currency as base ccy for crif generation, got "
                   << currency_ << ". Consider using " << simmStandardCurrency(currency_) << " instead?");
    QL_REQUIRE(usdSpot_ > 0.0,
               "The exchange rate from " << currency_ << " to USD should be positive for crif generation.");
    QL_REQUIRE(nameMapper_, "NameMapper is NULL in crif generator.");

    simmRecord_ = QuantLib::ext::make_shared<SimmRecordGenerator>(simmConfiguration, nameMapper_, tradeData_, crifMarket_,
                                                          xccyDiscounting_, currency_, usdSpot_, fieldGetter_,
                                                          referenceData_, curveConfigs_, discountIndex_);
}

QuantLib::ext::shared_ptr<ore::analytics::Crif>
CrifGenerator::generateCrif(const QuantLib::ext::shared_ptr<ore::analytics::SensitivityStream>& ss) {

    LOG("Starting to generate CRIF from SensitivityStream");

    auto results = QuantLib::ext::make_shared<Crif>();

    // A map that will be used to store Risk_FX sensitivities for the (base) currency itself for each trade.
    map<QuantLib::ext::tuple<string, string, string>, CrifRecord> riskFxCalcCurrency;

    set<string> failedTrades;

    // Populate all trade IDs.

    allTradeIds_ = tradeData_->simmTradeIds();

    LOG("Process sensitivity stream");

    // Generate CRIF records from sensitivities for non failed trades
    std::vector<ore::analytics::CrifRecord> records;
    if (ss)
        records = processSensitivityStream(*ss, failedTrades);

    // Identify trades without sensitivity records, i.e. remove trade IDs with some sensitivity
    for (const auto& r : records) {
        if (allTradeIds_.count(r.tradeId) == 1 && !close_enough(std::fabs(r.amountUsd), 0.0)) {
            allTradeIds_.erase(r.tradeId);
        }
    }

    // Ignore SIMM exemptions overrides, just add records to the resulting CRIF:
    for (auto& record : records)
        results->addRecord(record, false, false);

    // Add a zero RiskFX record for trades without CRIF records
    LOG("Process empty result trades");
    set<string> tmpAllTradeIds;
    tmpAllTradeIds.insert(allTradeIds_.begin(), allTradeIds_.end());
    for (const string& tradeId : tmpAllTradeIds) {
        if (failedTrades.find(tradeId) == failedTrades.end()) {
            // Assume that it is a zero sensitivity trade and write a Risk_FX row.
            results->addRecord(createZeroRiskFxRecord(tradeId), false, false);
        }
    }

    LOG("Finished generating CRIF report from SensitivityData.");
    return results;
}

std::vector<ore::analytics::CrifRecord> CrifGenerator::processSensitivityStream(SensitivityStream& ss,
                                                                                std::set<std::string>& failedTrades) {

    vector<CrifRecord> results;
    std::size_t processedRecords = 0;
    ss.reset();

    const auto& ids = tradeData_->simmTradeIds();

    vector<boost::optional<CrifRecord>> crifRecords;
    map<string, CrifRecord> fxRiskRecords, riskFxCalcCurrency;

    // Loop over the sensi stream
    while (SensitivityRecord sr = ss.next()) {

        ++processedRecords;

        // Skip if 1) not par or 2) is a cross gamma
        if (!sr.isPar || sr.key_2 != RiskFactorKey())
            continue;

        // Skip trades that are not relevant
        if (ids.find(sr.tradeId) == ids.end())
            continue;

        // Skip trades that have already failed
        if (failedTrades.find(sr.tradeId) != failedTrades.end())
            continue;

        vector<boost::optional<CrifRecord>> records;
        boost::optional<CrifRecord> record = simmRecord_->operator()(sr, failedTrades);

        // Post processing of records to ensure they are in a simm standard ccy
        // also generate additional inflation risk entries for "unidade" ccys - see QPR_11424
        std::vector<CrifRecord> additionalUnidadeRiskRecords;
        if (record) {
            if (record->riskType == RiskType::XCcyBasis || record->riskType == RiskType::IRCurve ||
                record->riskType == RiskType::FX || record->riskType == RiskType::Inflation ||
                record->riskType == RiskType::IRVol || record->riskType == RiskType::InflationVol) {
                if (record->riskType == RiskType::IRCurve && isUnidadeCurrency(record->qualifier)) {
                    CrifRecord cr = *record;
                    cr.qualifier = simmStandardCurrency(record->qualifier);
                    cr.riskType = RiskType::Inflation;
                    cr.amount = -cr.amount;
                    cr.amountUsd = -cr.amountUsd;
                    cr.bucket = "";
                    cr.label1 = "";
                    cr.label2 = "";
                    additionalUnidadeRiskRecords.push_back(cr);
                }
                convertToSimmStandardCurrency(record->qualifier);
            } else if (record->riskType == RiskType::FXVol) {
                if (!convertToSimmStandardCurrencyPair(record->qualifier)) {
                    StructuredAnalyticsErrorMessage("CRIF Generation", "",
                                                    "Removing FXVol entry with qualifier '" + record->qualifier +
                                                        "' arising from ccy standardization.")
                        .log();
                    record = boost::none;
                }
            }
        }
        records.push_back(record);
        records.insert(records.end(), additionalUnidadeRiskRecords.begin(), additionalUnidadeRiskRecords.end());

        // If we have a valid record, process it
        if (records.size() > 0) {

            // Process each record from records
            for (auto& r : records) {
                if (r) {
                    crifRecords.push_back(r);

                    // Add element to the Risk_FX in calculation currency map
                    auto srKey = sr.tradeId;

                    if (fxRiskRecords.count(srKey) == 0) {
                        QL_REQUIRE(sr.currency == currency_, "CrifGenerator: Sensitivity currency ("
                                                                 << sr.currency << ") must be the same as base ccy ("
                                                                 << currency_ << ")");
                        CrifRecord cr = *r;
                        cr.riskType = RiskType::FX;
                        cr.qualifier = currency_;
                        cr.bucket = "";
                        cr.label1 = "";
                        cr.label2 = "";
                        cr.amount = sr.baseNpv * 0.01;
                        cr.amountCurrency = currency_;
                        cr.amountUsd = usdSpot_ * cr.amount;
                        fxRiskRecords[srKey] = cr;
                    }
                }
            }
        }
    }

    if (crifRecords.size() > 0) {
        // Process each record from records
        for (const auto& r : crifRecords) {
            if (r && failedTrades.find(r->tradeId) == failedTrades.end()) {
                results.push_back(*r);
                if (r->riskType == RiskType::FX) {
                    auto srKey = r->tradeId;
                    QL_REQUIRE(fxRiskRecords[srKey].amountCurrency == r->amountCurrency,
                               "CrifGenerator: Cannot subtract Risk_FX amounts with different amount currencies");
                    fxRiskRecords[srKey].amount -= r->amount;
                    fxRiskRecords[srKey].amountUsd = usdSpot_ * fxRiskRecords[srKey].amount;
                }
            }
        }
    }
    // Add FX Record in calc currency
    for (auto& [key, r] : fxRiskRecords) {
        if (failedTrades.find(r.tradeId) == failedTrades.end()) {
            results.push_back(r);
        }
    }

    LOG("Processed sensi stream with " << processedRecords << " records, " << ids.size() << " trades");
    return results;
}

CrifRecord CrifGenerator::createZeroRiskFxRecord(const std::string& tradeId) const {
    DLOG("Writing a zero Risk_FX record to the CRIF report for trade ID " << tradeId);
    CrifRecord::RiskType riskType = CrifRecord::RiskType::FX;
    string imModelStr = fieldGetter_->field(tradeId, "im_model");
    CrifRecord::IMModel imModel = CrifRecord::IMModel::Empty;
    if (imModelStr.empty()) {
        imModel = CrifRecord::IMModel::SIMM;
    } else {
        imModel = parseIMModel(imModelStr);
    }
    string qualifier = currency_;
    bool useAvailableEndDate = false;
    return createZeroAmountCrifRecord(tradeId, riskType, imModel, qualifier, useAvailableEndDate);
}

CrifRecord CrifGenerator::createZeroAmountCrifRecord(const string& tradeId, const CrifRecord::RiskType riskType,
                                                     const CrifRecord::IMModel& imModel, const std::string& qualifer,
                                                     bool includeEndDate) const {
    QuantLib::ext::shared_ptr<SimmTradeData::TradeAttributes> ta;
    const bool hasAttributes = tradeData_->hasAttributes(tradeId);
    if (hasAttributes)
        ta = tradeData_->getAttributes(tradeId);

    string tradeType = hasAttributes ? ta->getTradeType() : "";

    const auto& simmCollectRegs = set<CrifRecord::Regulation>();
    const auto& simmPostRegs = set<CrifRecord::Regulation>();

    string enddate = includeEndDate && hasAttributes ? ta->getEndDate() : "";

    CrifRecord cr(tradeId, tradeType, tradeData_->nettingSetDetails(tradeId),
                  hasAttributes ? ta->getSimmProductClass() : CrifRecord::ProductClass::Empty, riskType, qualifer, "",
                  "", "", currency_, 0.0, 0.0, imModel, simmCollectRegs, simmPostRegs, enddate);

    if (fieldGetter_) {
        std::map<std::string, std::string> tradeAdditionalFields = fieldGetter_->fields(tradeId);
        for (const auto& fieldName : fieldGetter_->fieldNames()) {
            auto it = tradeAdditionalFields.find(fieldName);
            if (it != tradeAdditionalFields.end()) {
                cr.additionalFields[it->first] = it->second;
            }
        }
    }

    return cr;
}

} // namespace analytics
} // namespace ore
