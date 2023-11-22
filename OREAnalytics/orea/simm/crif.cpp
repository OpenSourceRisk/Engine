/*
 Copyright (C) 2023 AcadiaSoft Inc.
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

/*! \file orea/simm/crif.cpp
    \brief Struct for holding CRIF records
*/

#include <orea/simm/crif.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace analytics {

void Crif::addRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies) {
    if (record.type() == CrifRecord::RecordType::FRTB) {
        addFrtbCrifRecord(record, aggregateDifferentAmountCurrencies);
    } else if (record.type() == CrifRecord::RecordType::SIMM && !record.isSimmParameter()) {
        addSimmCrifRecord(record, aggregateDifferentAmountCurrencies);
    } else {
        addSimmParameterRecord(record);
    }
}

void Crif::addFrtbCrifRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies) {
    QL_REQUIRE(type_ == CrifType::Empty || type_ == CrifType::Frtb,
                   "Can not add a FRTB crif record to a SIMM Crif");
    if (type_ == CrifType::Empty) {
        type_ = CrifType::Frtb;
    }
    insertCrifRecord(record, aggregateDifferentAmountCurrencies);
}

void Crif::addSimmCrifRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies) {
    QL_REQUIRE(type_ == CrifType::Empty || type_ == CrifType::Simm,
                   "Can not add a Simm crif record to a Frtb Crif");
    if (type_ == CrifType::Empty) {
        type_ = CrifType::Simm;
    }
    insertCrifRecord(record, aggregateDifferentAmountCurrencies);
}

void Crif::insertCrifRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies) {

    auto it = records_.find(record);
    if (aggregateDifferentAmountCurrencies) {
        it = std::find_if(records_.begin(), records_.end(),
                          [&record](const auto& x) { return CrifRecord::amountCcyEQCompare(x, record); });
    }
    if (it == records_.end()) {
        records_.insert(record);
        portfolioIds_.insert(record.portfolioId);
        nettingSetDetails_.insert(record.nettingSetDetails);
    } else {
        updateAmountExistingRecord(it, record);
    }
}

void Crif::addSimmParameterRecord(const CrifRecord& record) {
    auto it = simmParameters_.find(record);
    if (it == simmParameters_.end()) {
        simmParameters_.insert(record);
    } else if (it->riskType == CrifRecord::RiskType::AddOnFixedAmount) {
        updateAmountExistingRecord(it, record);
    } else if (it->riskType == CrifRecord::RiskType::AddOnNotionalFactor &&
               it->riskType == CrifRecord::RiskType::ProductClassMultiplier) {
        // Only log warning if the values are not the same. If they are, then there is no material discrepancy.
        if (record.amount != it->amount) {
            string errMsg = "Found more than one instance of risk type " + ore::data::to_string(it->riskType) +
                            ". Please check the SIMM parameters input. If enforceIMRegulations=False, then it "
                            "is possible that multiple entries for different regulations now belong under the same "
                            "'Unspecified' regulation.";
            ore::analytics::StructuredAnalyticsWarningMessage("SIMM", "Aggregating SIMM parameters", errMsg).log();
        }
    }
}

void Crif::updateAmountExistingRecord(std::set<CrifRecord>::iterator& it, const CrifRecord& record) {
    bool updated = false;
        if (record.hasAmountUsd()) {
            it->amountUsd += record.amountUsd;
            updated = true;
        }
        if (record.hasAmount() && record.hasAmountCcy() && it->amountCurrency == record.amountCurrency) {
            it->amount += record.amount;
            updated = true;
        }
        if (record.hasAmountResultCcy() && record.hasResultCcy() && it->resultCurrency == record.resultCurrency) {
            it->amountResultCcy += record.amountResultCcy;
            updated = true;
        }
        if (updated)
            DLOG("Updated net CRIF records: " << cr)
}

void Crif::addRecords(const Crif& crif, bool aggregateDifferentAmountCurrencies = false) {
    for (const auto& r : crif.records_) {
        addRecord(r, aggregateDifferentAmountCurrencies);
    }
    for (const auto& r : crif.simmParameters_) {
        addRecord(r, aggregateDifferentAmountCurrencies);
    }
}

Crif Crif::aggregate() const {
    Crif result;
    for (auto cr : records_) {
        // We set the trade ID to an empty string because we are netting at portfolio level
        // The only exception here is schedule trades that are denoted by two rows,
        // with RiskType::Notional and RiskType::PV
        if (cr.imModel != "Schedule") {
            cr.tradeId = "";
        }
        result.addRecord(cr);
    }
    return result;
}



//! Give back the set of portfolio IDs that have been loaded
const std::set<std::string>& Crif::portfolioIds() const { return portfolioIds_; }
const std::set<NettingSetDetails>& Crif::nettingSetDetails() const { return nettingSetDetails_; }

bool Crif::hasNettingSetDetails() const {
    bool hasNettingSetDetails = false;
    for (const auto& nsd : nettingSetDetails_) {
        if (!nsd.emptyOptionalFields())
            hasNettingSetDetails = true;
    }
    return hasNettingSetDetails;
}

} // namespace analytics
} // namespace ore