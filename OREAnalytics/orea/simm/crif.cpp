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

void Crif::addRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies = false) {
    auto it = records_.find(record);
    if (!record.isSimmParameter() && aggregateDifferentAmountCurrencies) {
        it = std::find_if(records_.begin(), records_.end(),
                          [&record](const auto& x) { return CrifRecord::amountCcyEQCompare(x, record); });
    }
    if (it == records_.end()) {
        records_.insert(record);
        portfolioIds_.insert(record.portfolioId);
        nettingSetDetails_.insert(record.nettingSetDetails);
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
    } else {

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
            DLOG("Updated net CRIF records: " << cr);
    }
}


void Crif::addRecords(const Crif& crif, bool aggregateDifferentAmountCurrencies = false) {
    for (const auto& r : crif) {
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

void writeCrifReport(const boost::shared_ptr<Crif>& crif, ore::data::Report& crifReport,
                     const bool writeUseCounterpartyTrade = false, const bool applyThreshold = true) const;

//! Initialise the CRIF report headers
void writeCrifReportHeaders(ore::data::Report& report, bool writeUseCounterpartyTrade) const;

//! Write a CRIF record to the report
void writeCrifRecord(const CrifPlusRecord& crifRecord, ore::data::Report& report, bool writeUseCounterpartyTrade,
                     const bool applyThreshold = true);
} // namespace analytics
} // namespace ore