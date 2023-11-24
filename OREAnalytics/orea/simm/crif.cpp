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
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm_ext.hpp>

namespace ore {
namespace analytics {

auto isSimmParameter = [](const ore::analytics::CrifRecord& x) { return x.isSimmParameter(); };
auto isNotSimmParameter = std::not_fn(isSimmParameter);

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
    auto it = records_.find(record);
    if (it == records_.end()) {
        records_.insert(record);
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
            DLOG("Updated net CRIF records: " << *it)
}

void Crif::addRecords(const Crif& crif, bool aggregateDifferentAmountCurrencies = false) {
    for (const auto& r : crif.records_) {
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


const bool Crif::hasCrifRecords() const { 
    auto it= std::find_if(records_.begin(), records_.end(), isNotSimmParameter);
    return it != records_.end();
}

//! check if the Crif contains simmParameters
const bool Crif::hasSimmParameter() const {
    auto it= std::find_if(records_.begin(), records_.end(), isSimmParameter);
    return it != records_.end();
}

    //! returns a Crif containing only simmParameter entries
Crif Crif::simmParameters() const {
    Crif results;
    for (const auto& record : records_) {
        if (record.isSimmParameter()) {
            results.addSimmParameterRecord(record);
        }
    }
    return results;
}
    
//! deletes all existing simmParameter and replaces them with the new one
void Crif::setSimmParameters(const Crif& crif) {
    boost::range::remove_erase_if(records_, isSimmParameter);
    for (const auto& r : crif) {
        if (r.isSimmParameter()) {
            addSimmParameterRecord(r);
        }
    }
}


//! Give back the set of portfolio IDs that have been loaded
const std::set<std::string>& Crif::portfolioIds() const { return portfolioIds_; }
const std::set<NettingSetDetails>& Crif::nettingSetDetails() const { return nettingSetDetails_; }

std::set<CrifRecord::ProductClass> Crif::ProductClassesByNettingSetDetails(const NettingSetDetails nsd) const {
    std::set<CrifRecord::ProductClass> keys;
    for (const auto& record : records_) {
        if (record.nettingSetDetails == nsd) {
            keys.insert(record.productClass);
        }
    }
    return keys;
}




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