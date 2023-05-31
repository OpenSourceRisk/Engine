/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/simm/crifloader.hpp>
#include <orea/simm/simmbucketmapper.hpp>
#include <orea/simm/simmconfiguration.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <ored/utilities/parsers.hpp>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using ore::data::checkCurrency;
using ore::data::parseListOfValues;
using ore::data::parseReal;
using ore::data::parseBool;
using ore::data::to_string;
using ore::data::Report;
using QuantLib::Size;
using QuantLib::Real;
using std::exception;
using std::getline;
using std::ifstream;
using std::map;
using std::max_element;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

// Required headers
map<Size, set<string>> CrifLoader::requiredHeaders = {
    {0, {"tradeid", "trade_id"}},
    {1, {"portfolioid", "portfolio_id"}},
    {2, {"productclass", "product_class", "asset_class"}},
    {3, {"risktype", "risk_type"}},
    {4, {"qualifier"}},
    {5, {"bucket"}},
    {6, {"label1"}},
    {7, {"label2"}},
    {8, {"amountcurrency", "currency", "amount_currency"}},
    {9, {"amount"}},
    {10, {"amountusd", "amount_usd"}}};

// Optional headers
map<Size, set<string>> CrifLoader::optionalHeaders = {
    {11, {"agreementtype", "agreement_type"}},
    {12, {"calltype", "call_type"}},
    {13, {"initialmargintype", "initial_margin_type"}},
    {14, {"legalentityid", "legal_entity_id"}},
    {15, {"tradetype", "trade_type"}},
    {16, {"immodel", "im_model"}},
    {17, {"post_regulations"}},
    {18, {"collect_regulations"}},
    {19, {"end_date"}}};

// Ease syntax
using RiskType = SimmConfiguration::RiskType;
using ProductClass = SimmConfiguration::ProductClass;

void CrifLoader::add(CrifRecord cr, const bool onDiffAmountCcy) {

    // Skip the CRIF record if its risk type is not valid under the configuration
    if (!configuration_->isValidRiskType(cr.riskType)) {
        WLOG("Skipped loading CRIF record " << cr << " because its risk type " << cr.riskType
                                            << " is not valid under SIMM configuration " << configuration_->name());
        return;
    }

    // Some checks based on risk type
    string ccy_1;
    string ccy_2;
    ProductClass pc;
    switch (cr.riskType) {
    case RiskType::IRCurve:
    case RiskType::IRVol:
    case RiskType::Inflation:
    case RiskType::InflationVol:
    case RiskType::XCcyBasis:
    case RiskType::FX:
        // TODO: Do we really need to switch CNH to CNY here?
        //       How many more are like this?
        if (cr.qualifier == "CNH")
            cr.qualifier = "CNY";
        QL_REQUIRE(checkCurrency(cr.qualifier), "currency code '" << cr.qualifier << "' is not a supported currency code");
        break;
    case RiskType::FXVol:
        // Normalise the qualifier i.e. XXXYYY and YYYXXX are the same
        QL_REQUIRE(cr.qualifier.size() == 6,
                   "Expected a string of length 6 for FXVol qualifier but got " << cr.qualifier);
        ccy_1 = cr.qualifier.substr(0, 3);
        ccy_2 = cr.qualifier.substr(3);
        if (ccy_1 == "CNH")
            ccy_1 = "CNY";
        if (ccy_2 == "CNH")
            ccy_2 = "CNY";
        QL_REQUIRE(checkCurrency(ccy_1),
                   "currency code 1 in pair '" << cr.qualifier << "' (" << ccy_1 << ") is not a supported currency code");
        QL_REQUIRE(checkCurrency(ccy_2),
                   "currency code 2 in pair '" << cr.qualifier << "' (" << ccy_2 << ") is not a supported currency code");
        if (ccy_1 > ccy_2)
            ccy_1.swap(ccy_2);
        cr.qualifier = ccy_1 + ccy_2;
        break;
    case RiskType::AddOnFixedAmount:
    case RiskType::AddOnNotionalFactor:
        QL_REQUIRE(cr.productClass == ProductClass::Empty,
                   "Expected product class " << ProductClass::Empty << " for risk type " << cr.riskType);
        break;
    case RiskType::ProductClassMultiplier:
        QL_REQUIRE(cr.productClass == ProductClass::Empty,
                   "Expected product class " << ProductClass::Empty << " for risk type " << cr.riskType);
        // Check that the qualifier is a valid Product class
        pc = parseSimmProductClass(cr.qualifier);
        QL_REQUIRE(pc != ProductClass::Empty,
                   "The qualifier " << cr.qualifier << " should parse to a valid product class for risk type "
                                    << cr.riskType);
        // Check that the amount is a number >= 1.0
        QL_REQUIRE(cr.amount >= 1.0, "Expected an amount greater than or equal to 1.0 "
                                         << "for risk type " << cr.riskType << " and qualifier " << cr.qualifier
                                         << " but got " << cr.amount);
        break;
    case RiskType::Notional:
    case RiskType::PV:
        if (cr.imModel == "Schedule")
            QL_REQUIRE(!cr.additionalFields["end_date"].empty() || !cr.endDate.empty(),
                       "Expected end date for risk type " << cr.riskType << " and im_model=\'Schedule\'");
        break;
    default:
        break;
    }

    // We set the trade ID to an empty string because we are netting at portfolio level
    // The only exception here is schedule trades that are denoted by two rows,
    // with RiskType::Notional and RiskType::PV
    if (aggregateTrades_ && cr.imModel != "Schedule")
        cr.tradeId = "";

    // Add/update the CRIF record
    
    if (cr.isSimmParameter()) {
        auto it = simmParameters_.find(cr);
        if (it != simmParameters_.end()) {
            if (it->riskType == RiskType::AddOnFixedAmount) {
                // If there is already a net CrifRecord, update it
                bool updated = false;
                if (cr.hasAmountUsd()) {
                    it->amountUsd += cr.amountUsd;
                    updated = true;
                }
                if (cr.hasAmount() && cr.hasAmountCcy() && it->amountCurrency == cr.amountCurrency) {
                    it->amount += cr.amount;
                    updated = true;
                }
                if (updated)
                    DLOG("Updated net CRIF records: " << cr);
            } else if (it->riskType == RiskType::AddOnNotionalFactor ||
                       it->riskType == RiskType::ProductClassMultiplier) {
                string errMsg = "Found more than one instance of risk type " + to_string(it->riskType) +
                                ". Please check the SIMM parameters input. If enforceIMRegulations=False, then it "
                                "is possible that multiple entries for different regulations now belong under the same "
                                "'Unspecified' regulation.";
                WLOG(ore::analytics::StructuredAnalyticsWarningMessage("SIMM", "Aggregating SIMM parameters", errMsg));
            } else {
                // Handling in case new SIMM parameters are added in the future
                WLOG(ore::analytics::StructuredAnalyticsWarningMessage("SIMM", "Aggregating SIMM parameters",
                                                              "Unknown risk type: " + to_string(it->riskType)));
            }
        } else {
            // If there is no CrifRecord for it already, insert it
            simmParameters_.insert(cr);
            DLOG("Added to SIMM parameters: " << cr);
        }
    } else {
        auto it = onDiffAmountCcy ? crifRecords_.find(cr, CrifRecord::amountCcyLTCompare) : crifRecords_.find(cr);
        if (it != crifRecords_.end()) {
            // If there is already a net CrifRecord, update it
            bool updated = false;
            if (cr.hasAmountUsd()) {
                it->amountUsd += cr.amountUsd;
                updated = true;
            }
            if (cr.hasAmount() && cr.hasAmountCcy() && it->amountCurrency == cr.amountCurrency) {
                it->amount += cr.amount;
                updated = true;
            }
            if (updated)
                DLOG("Updated net CRIF records: " << cr);
        } else {
            // If there is no CrifRecord for it already, insert it
            crifRecords_.insert(cr);
            DLOG("Added to net CRIF records: " << cr);
        }

        // Update set of portfolio IDs if necessary
        portfolioIds_.insert(cr.portfolioId);
        nettingSetDetails_.insert(cr.nettingSetDetails);

        // Update the SIMM configuration's bucket mapper if the
        // loader has set this flag
        if (updateMapper_) {
            const auto& bm = configuration_->bucketMapper();
            if (bm->hasBuckets(cr.riskType)) {
                bm->addMapping(cr.riskType, cr.qualifier, cr.bucket);
            }
        }
    }
}

void CrifLoader::loadFromFile(const std::string& fileName, char eol, char delim, char quoteChar, char escapeChar) {

    LOG("Loading CRIF records from file " << fileName << " with end of line character " << static_cast<int>(eol)
                                          << ", delimiter " << static_cast<int>(delim) << " quote character "
                                          << static_cast<int>(quoteChar) << " escape character "
                                          << static_cast<int>(escapeChar));

    // Try to open the file
    ifstream file;
    file.open(fileName);
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);

    // Process the file
    loadFromStream(file, eol, delim, quoteChar, escapeChar);

    LOG("Finished loading CRIF records from file " << fileName);
}

void CrifLoader::loadFromString(const std::string& csvBuffer, char eol, char delim, char quoteChar, char escapeChar) {

    LOG("Loading CRIF records from end of line character "
        << static_cast<int>(eol) << ", delimiter " << static_cast<int>(delim) << " quote character "
        << static_cast<int>(quoteChar) << " escape character " << static_cast<int>(escapeChar));

    // Process the file
    std::stringstream csvStream;

    csvStream << csvBuffer;
    loadFromStream(csvStream, eol, delim, quoteChar, escapeChar);

    LOG("Finished loading CRIF records from csvBuffer");
}

void CrifLoader::loadFromStream(std::istream& stream, char eol, char delim, char quoteChar, char escapeChar) {
    string line;
    vector<string> entries;
    bool headerProcessed = false;
    Size emptyLines = 0;
    Size validLines = 0;
    Size invalidLines = 0;
    Size maxIndex = 0;
    Size currentLine = 0;
    while (getline(stream, line, eol)) {

        // Keep track of current line number for messages
        ++currentLine;

        // Trim leading and trailing space
        boost::trim(line);

        // Skip empty lines
        if (line.empty()) {
            ++emptyLines;
            continue;
        }

        // Break line up in to its elements.
        entries = parseListOfValues(line, escapeChar, delim, quoteChar);

        if (headerProcessed) {
            // Process a regular line of the CRIF file
            if (process(entries, maxIndex, currentLine)) {
                ++validLines;
            } else {
                ++invalidLines;
            }
        } else {
            // Process the header line of the CRIF file
            processHeader(entries);
            headerProcessed = true;
            auto maxPair = max_element(
                columnIndex_.begin(), columnIndex_.end(),
                [](const pair<Size, Size>& p1, const pair<Size, Size>& p2) { return p1.second < p2.second; });
            maxIndex = maxPair->second;
        }
    }

    LOG("Out of " << currentLine << " lines, there were " << validLines << " valid lines, " << invalidLines
                  << " invalid lines and " << emptyLines << " empty lines.");
}

const SimmNetSensitivities CrifLoader::netRecords(const bool includeSimmParams) const {
    SimmNetSensitivities netRecords = crifRecords_;

    if (includeSimmParams && !simmParameters_.empty()) {
        for (const CrifRecord& p : simmParameters_)
            netRecords.insert(p);
    }

    return netRecords;
}


//! Give back the set of portfolio IDs that have been loaded
const std::set<std::string>& CrifLoader::portfolioIds() const { return portfolioIds_; }
const std::set<NettingSetDetails>& CrifLoader::nettingSetDetails() const { return nettingSetDetails_; }

void CrifLoader::clear() {
    crifRecords_.clear();
    simmParameters_.clear();
    portfolioIds_.clear();
    nettingSetDetails_.clear();
}

bool CrifLoader::hasNettingSetDetails() const {
    bool hasNettingSetDetails = false;
    for (const auto& nsd : nettingSetDetails_) {
        if (!nsd.emptyOptionalFields())
            hasNettingSetDetails = true;
    }
    return hasNettingSetDetails;
}

void CrifLoader::processHeader(const vector<string>& headers) {
    columnIndex_.clear();

    // Get mapping for all required headers in to column index in the file
    string header;
    for (const auto& kv : requiredHeaders) {
        for (Size i = 0; i < headers.size(); ++i) {
            header = boost::to_lower_copy(headers[i]);
            if (kv.second.count(header) > 0) {
                columnIndex_[kv.first] = i;
            }
        }

        // Some headers are allowed to be missing (under certain circumstances)
        if (kv.first == 1) {
            // Portfolio ID header is allowed to be missing
            if (columnIndex_.count(1) == 0) {
                WLOG("Did not find a header for portfolioid in the CRIF file so using a default value");
            }
        } else if (kv.first == 10) {
            // Allow either amount_usd missing or amount and amount_currency, but not all three.
            // For SIMM, we ultimately use amount_usd, but if missing, we use amount and amount_currency
            // and let the SimmCalculator handle the conversion to amount_usd
            if (columnIndex_.count(10) == 0)
                QL_REQUIRE(columnIndex_.count(8) > 0 && columnIndex_.count(9) > 0,
                           "Must provide either amount and amount_currency, or amount_usd");
        } else {
            // All other headers should be there
            QL_REQUIRE(columnIndex_.count(kv.first) > 0,
                       "Could not find a header in the CRIF file for " << *kv.second.begin());
        }
    }

    for (const auto& kv : optionalHeaders) {
        for (Size i = 0; i < headers.size(); ++i) {
            header = boost::to_lower_copy(headers[i]);
            if (kv.second.count(header) > 0) {
                columnIndex_[kv.first] = i;
            }
        }
    }

    for (const auto& kv : additionalHeaders_) {
        for (Size i = 0; i < headers.size(); ++i) {
            header = boost::to_lower_copy(headers[i]);
            if (kv.second.count(header) > 0) {
                columnIndex_[kv.first] = i;
            }
        }
    }
}

bool CrifLoader::process(const vector<string>& entries, Size maxIndex, Size currentLine) {
    // Return early if there are not enough entries in the line
    if (entries.size() <= maxIndex) {
        WLOG("Line number: " << currentLine << ". Expected at least " << maxIndex + 1 << " entries but got only "
                             << entries.size());
        return false;
    }

    // Try to create and add a CRIF record
    // There could still be issues here so we surround with try..catch to allow processing to continue
    auto loadOptionalString = [&entries, this](int column) {
        return columnIndex_.count(column) == 0 ? "" : entries[columnIndex_[column]];
    };
    auto loadOptionalReal = [&entries, this](int column) -> QuantLib::Real{
        if (columnIndex_.count(column) == 0) {
            return QuantLib::Null<QuantLib::Real>();
        } else{
            return entries[columnIndex_[column]].empty() ? QuantLib::Null<QuantLib::Real>()
                                                         : parseReal(entries[columnIndex_[column]]);
        } 
    };

    string tradeId, tradeType, imModel;
    try {
        CrifRecord cr;
        tradeId = entries[columnIndex_.at(0)];
        tradeType = loadOptionalString(15);
        imModel = loadOptionalString(16);

        cr.tradeId = tradeId;
        cr.tradeType = tradeType;
        cr.imModel = imModel;
        cr.portfolioId = columnIndex_.count(1) == 0 ? "DummyPortfolio" : entries[columnIndex_.at(1)];
        cr.productClass = parseSimmProductClass(entries[columnIndex_.at(2)]);
        cr.riskType = parseSimmRiskType(entries[columnIndex_.at(3)]);
        
        // Qualifier - There are many other possible qualifier values, but we only do case-insensitive checks
        // for those with standardised values, i.e. currencies or ccy pairs
        cr.qualifier = entries[columnIndex_.at(4)];
        if ((cr.riskType == RiskType::IRCurve || cr.riskType == RiskType::IRVol || cr.riskType == RiskType::FX) &&
            cr.qualifier.size() == 3) {
            string ccyUpper = boost::to_upper_copy(cr.qualifier);

            // If ccy is already valid, do nothing. Otherwise, replace with all uppercase equivalent.
            // FIXME: Minor currencies will fail to get spotted here, though it is not likely that we will have
            // a qualifier in a minor ccy?
            if (!checkCurrency(cr.qualifier) && checkCurrency(ccyUpper))
                cr.qualifier = ccyUpper;
        } else if (cr.riskType == RiskType::FXVol && cr.qualifier.size() == 6) {
            string ccyPairUpper = boost::to_upper_copy(cr.qualifier);
            string ccy1Upper = ccyPairUpper.substr(0, 3);
            string ccy2Upper = ccyPairUpper.substr(3);
            string ccy1Lower = cr.qualifier.substr(0, 3);
            string ccy2Lower = cr.qualifier.substr(3);

            // If ccy pair is already valid, do nothing. Otherwise, replace with uppercase equivalent of ccy1 and ccy2.
            // FIXME: Minor currencies will fail to get spotted here, though it is not likely that we will have
            // a ccy pair where at least one is a minor currency?
            if (!(checkCurrency(ccy1Lower) && checkCurrency(ccy2Lower)) && checkCurrency(ccy1Upper) && checkCurrency(ccy2Upper))
                cr.qualifier = ccyPairUpper;
        }

        // Bucket - Hardcoded "Residual" for case-insensitive check since this is currently the only non-numeric value
        cr.bucket = entries[columnIndex_.at(5)];
        if (boost::to_lower_copy(cr.bucket) == "residual")
            cr.bucket = "Residual";

        // Label1
        cr.label1 = entries[columnIndex_.at(6)];
        if (configuration_->isValidRiskType(cr.riskType)) {
            for (const string& l : configuration_->labels1(cr.riskType)) {
                if (boost::to_lower_copy(cr.label1) == boost::to_lower_copy(l))
                    cr.label1 = l;
            }
        }
        // Label2
        cr.label2 = entries[columnIndex_.at(7)];
        if (configuration_->isValidRiskType(cr.riskType)) {
            for (const string& l : configuration_->labels2(cr.riskType)) {
                if (boost::to_lower_copy(cr.label2) == boost::to_lower_copy(l))
                    cr.label2 = l;
            }
        }

        // We populate these 'required' values using loadOptional*, but they will have been validated already in processHeader,
        // and missing amountUsd (but with valid amount and amountCurrency) values populated later on in the analytics

        cr.amountCurrency = loadOptionalString(8);
        string amountCcyUpper = boost::to_upper_copy(cr.amountCurrency);
        if (!amountCcyUpper.empty() && !checkCurrency(cr.amountCurrency) && checkCurrency(amountCcyUpper))
            cr.amountCurrency = amountCcyUpper;

        cr.amount = loadOptionalReal(9);
        cr.amountUsd = loadOptionalReal(10);

        // Populate netting set details
        cr.agreementType = loadOptionalString(11);
        cr.callType = loadOptionalString(12);
        cr.initialMarginType = loadOptionalString(13);
        cr.legalEntityId = loadOptionalString(14);
        cr.nettingSetDetails = NettingSetDetails(cr.portfolioId, cr. agreementType, cr.callType, cr.initialMarginType,
                                                 cr.legalEntityId);
        cr.postRegulations = loadOptionalString(17);
        cr.collectRegulations = loadOptionalString(18);
        cr.endDate = loadOptionalString(19);

        // Check the IM model
        try {
            cr.imModel = to_string(parseIMModel(cr.imModel));
        } catch (...) {
            // If we cannot convert to a valid im_model, then it was either provided blank
            // or is simply not a valid value
        }

        // Store additional data that matches the defined additional headers in the additional fields map
        for (auto& additionalField : additionalHeaders_) {
            std::string value = loadOptionalString(additionalField.first);
            if (!value.empty())
                cr.additionalFields[*additionalField.second.begin()] = value;
        }

        // Add the CRIF record to the net records
        add(cr);

    } catch (const exception& e) {
        WLOG(ore::data::StructuredTradeErrorMessage(tradeId, tradeType, "CRIF loading",
            "Line number: " + to_string(currentLine) +
                ". Error processing CRIF line, so skipping it. Error: " + to_string(e.what())));
        return false;
    }

    return true;
}

void CrifLoader::aggregate() {
    aggregateTrades_ = true;

    //! temp records (CRIF records + SIMM parameters)
    SimmNetSensitivities tempRecords = netRecords(true);

    // clear net records, simm parameters, portfolioIds and netting set details
    clear();

    for (const auto& r : tempRecords)
        add(r);
}

void CrifLoader::fillAmountUsd(const boost::shared_ptr<ore::data::Market> market) {
    if (!market) {
        WLOG("CrifLoader::fillAmountUsd() was called, but market object is empty.")
        return;
    }

    auto fillNetRecords = [market](SimmNetSensitivities& records){
        SimmNetSensitivities tmpRecords;
        for (const CrifRecord& cr : records) {
            // Fill in amount USD if it is missing and if CRIF record requires it (i.e. if it has amount and amount
            // currency, and risk type is neither AddOnNotionalFactor or ProductClassMultiplier)
            if (cr.requiresAmountUsd() && !cr.hasAmountUsd()) {
                if (!cr.hasAmount() || !cr.hasAmountCcy()) {
                    WLOG(ore::data::StructuredTradeWarningMessage(
                        cr.tradeId, cr.tradeType, "Populating CRIF amount USD",
                        "CRIF record is missing one of Amount and AmountCurrency: " + to_string(cr)));
                } else {
                    Real usdSpot = market->fxRate(cr.amountCurrency + "USD")->value();
                    cr.amountUsd = cr.amount * usdSpot;
                }
            }

            tmpRecords.insert(cr);
        }
        records = tmpRecords;
    };
    fillNetRecords(crifRecords_);
    fillNetRecords(simmParameters_);
}

} // namespace analytics
} // namespace ore
