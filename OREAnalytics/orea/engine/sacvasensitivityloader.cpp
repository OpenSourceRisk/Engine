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

#include <orea/engine/sacvasensitivityloader.hpp>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/marketdata.hpp>

using namespace ore::data;
using namespace QuantLib;
using namespace std;

namespace ore {
namespace analytics {

// Allowable headers
map<Size, string> SaCvaSensitivityLoader::expectedHeaders = {{0, "NettingSet"},
                                                    {1, "RiskType"},
                                                    {2, "CvaType"},
                                                    {3, "MarginType"},
                                                    {4, "RiskFactor"},
                                                    {5, "Bucket"},
                                                    {6, "Value"}
                                                };

CvaRiskFactorKey mapRiskFactorKeyToCvaRiskFactorKey(string s) {
    pair<RiskFactorKey, string> keyDesc = deconstructFactor(s);
    RiskFactorKey rfk = keyDesc.first;
    string desc = keyDesc.second;

    CvaRiskFactorKey::KeyType keyType = CvaRiskFactorKey::KeyType::None;
    CvaRiskFactorKey::MarginType marginType = CvaRiskFactorKey::MarginType::None;
    string name = rfk.name;
    Period period;
    try {
        period = parsePeriod(desc);
    } catch (...) {
        period = Period();
        WLOG("Failed to parse risk factor description '" << desc << "' in risk factor " << s << " into a period");
    }

    switch (rfk.keytype) {
    case RiskFactorKey::KeyType::DiscountCurve:
        keyType = CvaRiskFactorKey::KeyType::InterestRate;
        marginType = CvaRiskFactorKey::MarginType::Delta;
        break;
    case RiskFactorKey::KeyType::IndexCurve: {
        keyType = CvaRiskFactorKey::KeyType::InterestRate;
        marginType = CvaRiskFactorKey::MarginType::Delta;
        // We need currency rather than index name
        auto idx = parseIborIndex(name);
        name = idx->currency().code();
        break;
    }
    case RiskFactorKey::KeyType::YieldCurve:
    case RiskFactorKey::KeyType::ZeroInflationCurve:
    case RiskFactorKey::KeyType::YoYInflationCurve: {
        keyType = CvaRiskFactorKey::KeyType::InterestRate;
        marginType = CvaRiskFactorKey::MarginType::Delta;
	QL_FAIL("Clarify mapping of risk factor " << s << " to SaCvaRskFactor");
        break;
    }
    case RiskFactorKey::KeyType::SwaptionVolatility:
    case RiskFactorKey::KeyType::OptionletVolatility:
    case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility:
    case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility:
        keyType = CvaRiskFactorKey::KeyType::InterestRate;
        marginType = CvaRiskFactorKey::MarginType::Vega;
        break;
    case RiskFactorKey::KeyType::FXSpot:
        keyType = CvaRiskFactorKey::KeyType::ForeignExchange;
        marginType = CvaRiskFactorKey::MarginType::Delta;
        break;
    case RiskFactorKey::KeyType::FXVolatility:
        keyType = CvaRiskFactorKey::KeyType::ForeignExchange;
        marginType = CvaRiskFactorKey::MarginType::Vega;
        break;
    case RiskFactorKey::KeyType::EquitySpot:
        keyType = CvaRiskFactorKey::KeyType::Equity;
        marginType = CvaRiskFactorKey::MarginType::Delta;
        break;
    case RiskFactorKey::KeyType::EquityVolatility:
        keyType = CvaRiskFactorKey::KeyType::Equity;
        marginType = CvaRiskFactorKey::MarginType::Vega;
        break;
    case RiskFactorKey::KeyType::CommodityCurve:
        keyType = CvaRiskFactorKey::KeyType::Commodity;
        marginType = CvaRiskFactorKey::MarginType::Delta;
        break;
    case RiskFactorKey::KeyType::CommodityVolatility:
        keyType = CvaRiskFactorKey::KeyType::Commodity;
        marginType = CvaRiskFactorKey::MarginType::Vega;
        break;
    case RiskFactorKey::KeyType::SurvivalProbability:
        // FIXME: Distinguish CreditReference from CreditCounterparty risk
        keyType = CvaRiskFactorKey::KeyType::CreditCounterparty;
        marginType = CvaRiskFactorKey::MarginType::Delta;
	ALOG("Cannot distinguish CreditReference from CreditCounterparty risk for risk factor " << s);
        break;
    case RiskFactorKey::KeyType::CDSVolatility:
        keyType = CvaRiskFactorKey::KeyType::CreditReference;
        marginType = CvaRiskFactorKey::MarginType::Vega;
        break;
    default:
        keyType = CvaRiskFactorKey::KeyType::None;
        marginType = CvaRiskFactorKey::MarginType::None;
	QL_FAIL("Clarify mapping of risk factor " << s << " to SaCvaRskFactor");
    }

    CvaRiskFactorKey cvaRiskFactorKey(keyType, marginType, name, period);

    LOG("Map RiskFactorKey " << s << " -> " << rfk << " : " << desc << " => " << cvaRiskFactorKey);

    return cvaRiskFactorKey;
}

void SaCvaSensitivityLoader::add(const SaCvaSensitivityRecord& record, bool aggregate) {
    SaCvaSensitivityRecord cr = record;
    if (aggregate)
        cr.nettingSetId = "";

    // Add/update the CvaSensitivity record
    auto it = netRecords_.find(cr);
    if (it != netRecords_.end()) {
        // If there is already a net CvaSensitivityRecord, update it
        it->value += cr.value;
        DLOG("Updated net CvaSensitivity records: " << cr);
    } else {
        // If there is no CvaSensitivityRecord for it already, insert it
        netRecords_.insert(cr);
        DLOG("Added to net CvaSensitivity records: " << cr);
    }

    // Update set of portfolio IDs if necessary
    nettingSetIds_.insert(cr.nettingSetId);

}

void SaCvaSensitivityLoader::load(const std::string& fileName, char eol, char delim, char quoteChar) {

    LOG("Loading CvaSensitivity records from file " << fileName << " with end of line character " << static_cast<int>(eol)
                                          << ", delimiter " << static_cast<int>(delim) << " and quote character "
                                          << static_cast<int>(quoteChar));

    // Try to open the file
    ifstream file;
    file.open(fileName);
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    // Process the file
    string line;
    vector<string> entries;
    bool headerProcessed = false;
    Size emptyLines = 0;
    Size validLines = 0;
    Size invalidLines = 0;
    Size maxIndex = 0;
    Size currentLine = 0;

    while (getline(file, line, eol)) {
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
        boost::split(entries, line, boost::is_any_of(string(1, delim)), boost::token_compress_off);

        // If quote character is provided attempt to strip it from each entry
        if (quoteChar != '\0') {
            for (string& entry : entries) {
                // Only remove the quote character if it appears at start and end of entry
                if (entry.front() == quoteChar && entry.back() == quoteChar) {
                    entry.erase(0, 1);
                    entry.erase(entry.size() - 1);
                }
            }
        }

        if (headerProcessed) {
	    // Process a regular line of the CvaSensitivity file
            if (process(entries, maxIndex, currentLine)) {
                ++validLines;
            } else {
                ++invalidLines;
            }
        } else {
	    // Process the header line of the CvaSensitivity file
            processHeader(entries);
            headerProcessed = true;
            auto maxPair = max_element(
                columnIndex_.begin(), columnIndex_.end(),
                [](const pair<Size, Size>& p1, const pair<Size, Size>& p2) { return p1.second < p2.second; });
            maxIndex = maxPair->second;
        }
    }

    LOG("Finished loading CvaSensitivity records from file " << fileName);
    LOG("Out of " << currentLine << " lines, there were " << validLines << " valid lines, " << invalidLines
                  << " invalid lines and " << emptyLines << " empty lines.");
}


void SaCvaSensitivityLoader::loadRawSensi(const CvaSensitivityRecord& cvaSensi,
    const std::string& baseCurrency, const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager) {
    if (close_enough(cvaSensi.delta, 0.0))
        return;

    cvaSensitivityRecords_.push_back(cvaSensi);
    
    // const the cvaSensitivity Record to a SaCvaSensitivityRecord
    SaCvaSensitivityRecord cr;
    cr.nettingSetId = cvaSensi.nettingSetId;

    cr.riskType = cvaSensi.key.keytype;
    // always Aggregate when loading from a sensistream
    cr.cvaType = CvaType::CvaAggregate;
    cr.marginType = cvaSensi.key.margintype;
    string riskFactor = "";
    string bucket = "";
    string ccy1, ccy2;
    switch (cr.riskType) {
    case CvaRiskFactorKey::KeyType::InterestRate:
        if (cr.marginType == CvaRiskFactorKey::MarginType::Delta) {
            riskFactor = to_string(cvaSensi.key.period);
        } else if (cr.marginType == CvaRiskFactorKey::MarginType::Vega) {
            riskFactor = "IRVolatility";
        }
        bucket = cvaSensi.key.name;
        break;
    case CvaRiskFactorKey::KeyType::ForeignExchange:
        if (cr.marginType == CvaRiskFactorKey::MarginType::Delta) {
            riskFactor = "FXSpot";
        } else if (cr.marginType == CvaRiskFactorKey::MarginType::Vega) {
            riskFactor = "FXVolatility";
        }
        ccy1 = cvaSensi.key.name.substr(0, 3);
        ccy2 = cvaSensi.key.name.substr(3, 3);
        bucket = (ccy1 == baseCurrency) ? ccy2 : ccy1;
        break;
    case CvaRiskFactorKey::KeyType::CreditCounterparty:
        riskFactor = cvaSensi.key.name + "/" + to_string(cvaSensi.key.period);
        if (counterpartyManager) {
            // cpty from the risk factor, look up the bucket in counterparty manager
            QL_REQUIRE(counterpartyManager->has(cvaSensi.key.name),
                "counterparty ID " << cvaSensi.key.name << " missing in counterparty manager for SA CVA loader");
            QuantLib::ext::shared_ptr<CounterpartyInformation> cp = counterpartyManager->get(cvaSensi.key.name);
            bucket = cp->saCvaRiskBucket();
        } else {
            QL_FAIL("Counterparty manager required for risk bucket lookup");
        }
        QL_REQUIRE(bucket != "", "Cannot find SA Risk Bucket in counterparty xml");
        break;
    case CvaRiskFactorKey::KeyType::CreditReference:
        bucket = "";
        break;
    case CvaRiskFactorKey::KeyType::Equity:
        bucket = "";
        break;
    case CvaRiskFactorKey::KeyType::Commodity:
        bucket = "";
        break;
    default:
        QL_FAIL("unknown riskType");
    }
    cr.riskFactor = riskFactor;
    cr.bucket = bucket;
    cr.value = cvaSensi.delta / cvaSensi.shiftSize;

    // add the new record
    add(cr, false);
    add(cr, true);
}

void SaCvaSensitivityLoader::loadFromRawSensis(const QuantLib::ext::shared_ptr<CvaSensitivityCubeStream> sensiStream,
    const string& baseCurrency, const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager) {
    sensiStream->reset();
    while (CvaSensitivityRecord sr = sensiStream->next())
        loadRawSensi(sr, baseCurrency, counterpartyManager);
}


void SaCvaSensitivityLoader::loadFromRawSensis(std::vector<CvaSensitivityRecord> cvaSensis,
    const std::string& baseCurrency, const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager) {
    for (auto sr : cvaSensis)
        loadRawSensi(sr, baseCurrency, counterpartyManager);
}

void SaCvaSensitivityLoader::loadFromRawSensis(
    const QuantLib::ext::shared_ptr<ParSensitivityCubeStream> parSensiStream, const std::string& baseCurrency,
    const QuantLib::ext::shared_ptr<SensitivityScenarioData>& scenarioData,
    const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager) {

    QL_REQUIRE(parSensiStream, "SaCvaSensitivityLoader: par sensi cube stream is not provided");
    QL_REQUIRE(scenarioData, "SaCvaSensitivityLoader: Sensi scenario data not set");
    QL_REQUIRE(counterpartyManager, "SaCvaSensitivityLoader: Counterparty manager not set");

    vector<CvaSensitivityRecord> cvaSensis;
    parSensiStream->reset();
    while (SensitivityRecord sr = parSensiStream->next()) {
        CvaSensitivityRecord r;
        r.nettingSetId = sr.tradeId;
        string rf = prettyPrintInternalCurveName(reconstructFactor(sr.key_1, sr.desc_1));
	LOG("SaCvaSensitivityLoader: sr.key_1=" << sr.key_1 << " sr.desc_1=" << sr.desc_1 << " keytype=" << sr.key_1.keytype << " name=" << sr.key_1.name << " index=" << sr.key_1.index);
        r.key = mapRiskFactorKeyToCvaRiskFactorKey(rf);
	// Shift type and size from the sensitivity config is needed here
	SensitivityScenarioData::ShiftData shiftData = scenarioData->shiftData(sr.key_1.keytype,  sr.key_1.name);
	r.shiftType = shiftData.shiftType;
	r.shiftSize = shiftData.shiftSize;
	LOG("SaCvaSensitivityLoader: sr.key_1=" << sr.key_1 << " shiftType=" << r.shiftType << " shiftSize=" << r.shiftSize);
	r.currency = sr.currency;
        r.baseCva = sr.baseNpv;
        r.delta = sr.delta;
        cvaSensis.push_back(r);
	LOG("SaCvaSensitivityLoader " << r.key << " " << r.delta);
    }

    loadFromRawSensis(cvaSensis, baseCurrency, counterpartyManager);
}

//! Give back the set of netting set IDs that have been loaded
const std::set<std::string>& SaCvaSensitivityLoader::nettingSetIds() const { return nettingSetIds_; }

void SaCvaSensitivityLoader::clear() {
    netRecords_.clear();
    nettingSetIds_.clear();
}

void SaCvaSensitivityLoader::processHeader(const vector<string>& headers) {
    // Get mapping for all headers in to column index in the file
    string header;
    for (const auto& kv : expectedHeaders) {
        for (Size i = 0; i < headers.size(); ++i) {
            if (kv.second == headers[i]) {
                columnIndex_[kv.first] = i;
		LOG("SaCvaSensitivityLoader::processHeader " << kv.first << " " << i);
            }
        }

    }
}

bool SaCvaSensitivityLoader::process(const vector<string>& entries, Size maxIndex, Size currentLine) {
    // Return early if there are not enough entries in the line
    if (entries.size() <= maxIndex) {
        WLOG("Line number: " << currentLine << ". Expected at least " << maxIndex + 1 << " entries but got only "
                             << entries.size());
        return false;
    }

    // Try to create and add a CvaSensitivity record
    // There could still be issues here so we surround with try..catch to allow processing to continue
    try {
        SaCvaSensitivityRecord cr;
        cr.nettingSetId = entries[columnIndex_.at(0)];
        cr.riskType = parseCvaRiskFactorKeyType(entries[columnIndex_.at(1)]);
        cr.cvaType = parseCvaType(entries[columnIndex_.at(2)]);
        cr.marginType = parseCvaRiskFactorMarginType(entries[columnIndex_.at(3)]);
        cr.riskFactor = entries[columnIndex_.at(4)];
        cr.bucket = entries[columnIndex_.at(5)];
        cr.value = parseReal(entries[columnIndex_.at(6)]);

        // Add the CvaSensitivity record to the net records
        add(cr, false);
        add(cr, true);

    } catch (const exception& e) {
        WLOG("Line number: " << currentLine << ". Error processing line so skipping it. Error: " << e.what());
        return false;
    }

    return true;
}

} // namespace analytics
} // namespace ore
