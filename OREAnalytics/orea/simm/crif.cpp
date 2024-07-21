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

#include <algorithm>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/simm/crif.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace analytics {

auto isSimmParameter = [](const ore::analytics::CrifRecord& x) { return x.isSimmParameter(); };
auto isNotSimmParameter = std::not_fn(isSimmParameter);

void Crif::addRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies, bool sortFxVolQualifer,
                     bool aggregateRegulations) {
    if (record.type() == CrifRecord::RecordType::FRTB) {
        addFrtbCrifRecord(record, aggregateDifferentAmountCurrencies, sortFxVolQualifer, aggregateRegulations);
    } else if (record.type() == CrifRecord::RecordType::SIMM && !record.isSimmParameter()) {
        addSimmCrifRecord(record, aggregateDifferentAmountCurrencies, sortFxVolQualifer, aggregateRegulations);
    } else {
        addSimmParameterRecord(record);
    }
}

void Crif::addFrtbCrifRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies, bool sortFxVolQualifer,
                             bool aggregateRegulations) {
    QL_REQUIRE(type_ == CrifType::Empty || type_ == CrifType::Frtb, "Can not add a FRTB crif record to a SIMM Crif");
    if (type_ == CrifType::Empty) {
        type_ = CrifType::Frtb;
    }
    insertCrifRecord(record, aggregateDifferentAmountCurrencies);
}

void Crif::addSimmCrifRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies, bool sortFxVolQualifer,
                             bool aggregateRegulations) {
    QL_REQUIRE(type_ == CrifType::Empty || type_ == CrifType::Simm, "Can not add a Simm crif record to a Frtb Crif");
    if (type_ == CrifType::Empty) {
        type_ = CrifType::Simm;
    }
    auto recordToAdd = record;
    if (sortFxVolQualifer && recordToAdd.riskType == CrifRecord::RiskType::FXVol) {
        auto ccy_1 = recordToAdd.qualifier.substr(0, 3);
        auto ccy_2 = recordToAdd.qualifier.substr(3);
        if (ccy_1 > ccy_2)
            ccy_1.swap(ccy_2);
        recordToAdd.qualifier = ccy_1 + ccy_2;
    }
    insertCrifRecord(recordToAdd, aggregateDifferentAmountCurrencies, aggregateRegulations);
}

void Crif::insertCrifRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies,
                            bool aggregateRegulations) {

    auto it = aggregateDifferentAmountCurrencies
                  ? (aggregateRegulations ? records_.find(record, CrifRecord::amountCcyRegsLTCompare)
                                            : records_.find(record, CrifRecord::amountCcyLTCompare))
                  : records_.find(record);

    if (it == records_.end()) {
        records_.insert(record);
        nettingSetDetails_.insert(record.nettingSetDetails);
    } else {
        updateAmountExistingRecord(it, record);
    }
}

void Crif::addSimmParameterRecord(const CrifRecord& record) {
    auto it = records_.find(record);
    if (it == records_.end()) {
        CrifRecord newRecord = record;
        records_.insert(newRecord);
    } else if (it->riskType == CrifRecord::RiskType::AddOnFixedAmount) {
        updateAmountExistingRecord(it, record);
    } else if (it->riskType == CrifRecord::RiskType::AddOnNotionalFactor ||
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

void Crif::updateAmountExistingRecord(CrifRecordContainer::iterator& it, const CrifRecord& record) {
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

void Crif::addRecords(const Crif& crif, bool aggregateDifferentAmountCurrencies, bool sortFxVolQualifer,
                      bool aggregateRegulations) {
    for (const auto& r : crif.records_) {
        addRecord(r, aggregateDifferentAmountCurrencies, sortFxVolQualifer, aggregateRegulations);
    }
}

Crif Crif::aggregate(bool aggregateDifferentAmountCurrencies, bool aggregateRegulations) const {
    Crif result;
    for (auto cr : records_) {
        // We set the trade ID to an empty string because we are netting at portfolio level
        // The only exception here is schedule trades that are denoted by two rows,
        // with RiskType::Notional and RiskType::PV
        if (cr.imModel != "Schedule") {
            cr.tradeId = "";
        }
        result.addRecord(cr, aggregateDifferentAmountCurrencies, aggregateRegulations);
    }
    return result;
}

const bool Crif::hasCrifRecords() const {
    auto it = std::find_if(records_.begin(), records_.end(), isNotSimmParameter);
    return it != records_.end();
}

//! check if the Crif contains simmParameters
const bool Crif::hasSimmParameters() const {
    auto it = std::find_if(records_.begin(), records_.end(), isSimmParameter);
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
//! Find first element
CrifRecordContainer::const_iterator Crif::findBy(const NettingSetDetails nsd, CrifRecord::ProductClass pc,
                                                 const CrifRecord::RiskType rt, const std::string& qualifier) const {
    return std::find_if(records_.begin(), records_.end(), [&nsd, &pc, &rt, &qualifier](const CrifRecord& record) {
        return record.nettingSetDetails == nsd && record.productClass == pc && record.riskType == rt &&
               record.qualifier == qualifier;
    });
};

Crif Crif::filterNonZeroAmount(double threshold, std::string alwaysIncludeFxRiskCcy) const {
    Crif results;
    for (auto record : records_) {
        QL_REQUIRE(record.amount != QuantLib::Null<QuantLib::Real>() || record.amountUsd != QuantLib::Null<double>(),
                   "Internal Error, amount and amountUsd are empty");
        double absAmount = 0.0;
        if ((record.amount != QuantLib::Null<double>()) && (record.amountUsd != QuantLib::Null<double>())) {
            absAmount = std::max(std::fabs(record.amount), std::fabs(record.amountUsd));
        } else if (record.amount != QuantLib::Null<double>()) {
            absAmount = std::fabs(record.amount);
        } else if (record.amountUsd != QuantLib::Null<double>()) {
            absAmount = std::fabs(record.amountUsd);
        }
        bool add = (absAmount > threshold && !QuantLib::close_enough(absAmount, threshold));
        if (!alwaysIncludeFxRiskCcy.empty())
            add = add || (record.riskType == CrifRecord::RiskType::FX && record.qualifier == alwaysIncludeFxRiskCcy);
        if (add) {
            results.addRecord(record);
        }
    }
    return results;
}

std::set<std::string> Crif::qualifiersBy(const NettingSetDetails nsd, CrifRecord::ProductClass pc,
                                         const CrifRecord::RiskType rt) const {
    auto res = records_ | boost::adaptors::filtered([&nsd, &pc, &rt](const CrifRecord& record) {
                   return record.nettingSetDetails == nsd && record.productClass == pc && record.riskType == rt;
               }) |
               boost::adaptors::transformed([](const CrifRecord& record) { return record.qualifier; });
    return boost::copy_range<std::set<std::string>>(res);
}

std::vector<CrifRecord> Crif::filterByQualifierAndBucket(const NettingSetDetails& nsd,
                                                         const CrifRecord::ProductClass pc,
                                                         const CrifRecord::RiskType rt, const std::string& qualifier,
                                                         const std::string& bucket) const {
    return boost::copy_range<std::vector<CrifRecord>>(
        records_ | boost::adaptors::filtered([&nsd, &pc, &rt, &qualifier, &bucket](const CrifRecord& record) {
            return record.nettingSetDetails == nsd && record.productClass == pc && record.riskType == rt &&
                   record.qualifier == qualifier && record.bucket == bucket;
        }));
}

std::vector<CrifRecord> Crif::filterByQualifier(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc,
                                                const CrifRecord::RiskType rt, const std::string& qualifier) const {

    return boost::copy_range<std::vector<CrifRecord>>(
        records_ | boost::adaptors::filtered([&nsd, &pc, &rt, &qualifier](const CrifRecord& record) {
            return record.nettingSetDetails == nsd && record.productClass == pc && record.riskType == rt &&
                   record.qualifier == qualifier;
        }));
}

std::vector<CrifRecord> Crif::filterByBucket(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc,
                                             const CrifRecord::RiskType rt, const std::string& bucket) const {
    return boost::copy_range<std::vector<CrifRecord>>(
        records_ | boost::adaptors::filtered([&nsd, &pc, &rt, &bucket](const CrifRecord& record) {
            return record.nettingSetDetails == nsd && record.productClass == pc && record.riskType == rt &&
                   record.bucket == bucket;
        }));
}

std::vector<CrifRecord> Crif::filterBy(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc,
                                       const CrifRecord::RiskType rt) const {
    return boost::copy_range<std::vector<CrifRecord>>(
        records_ | boost::adaptors::filtered([&nsd, &pc, &rt](const CrifRecord& record) {
            return record.nettingSetDetails == nsd && record.productClass == pc && record.riskType == rt;
        }));
}

std::vector<CrifRecord> Crif::filterBy(const CrifRecord::RiskType rt) const {
    return boost::copy_range<std::vector<CrifRecord>>(
        records_ | boost::adaptors::filtered([&rt](const CrifRecord& record) { return record.riskType == rt; }));
}

std::vector<CrifRecord> Crif::filterByTradeId(const std::string& id) const {
    return boost::copy_range<std::vector<CrifRecord>>(
        records_ | boost::adaptors::filtered([&id](const CrifRecord& record) { return record.tradeId == id; }));
}

std::set<std::string> Crif::tradeIds() const {
    return boost::copy_range<std::set<std::string>>(
        records_ | boost::adaptors::transformed([](const CrifRecord& r) { return r.tradeId; }));
}

//! deletes all existing simmParameter and replaces them with the new one
void Crif::setSimmParameters(const Crif& crif) {
    auto backup = records_;
    records_.clear();
    for (auto& r : backup) {
        if (!r.isSimmParameter()) {
            addRecord(r);
        }
    }
    for (const auto& r : crif) {
        if (r.isSimmParameter()) {
            addSimmParameterRecord(r);
        }
    }
}

void Crif::setCrifRecords(const Crif& crif) {
    auto backup = records_;
    records_.clear();
    for (auto& r : backup) {
        if (r.isSimmParameter()) {
            addRecord(r);
        }
    }
    for (const auto& r : crif) {
        if (!r.isSimmParameter()) {
            addRecord(r);
        }
    }
}

//! Give back the set of portfolio IDs that have been loaded
set<string> Crif::portfolioIds() const {
    return boost::copy_range<set<string>>(records_ | boost::adaptors::transformed([](const CrifRecord& r) {
                                              return r.nettingSetDetails.nettingSetId();
                                          }));
}

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

size_t Crif::countMatching(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc,
                           const CrifRecord::RiskType rt, const std::string& qualifier) const {
    return std::count_if(records_.begin(), records_.end(), [&nsd, &pc, &rt, &qualifier](const CrifRecord& record) {
        return record.nettingSetDetails == nsd && record.productClass == pc && record.riskType == rt &&
               record.qualifier == qualifier;
    });
}

bool Crif::hasNettingSetDetails() const {
    bool hasNettingSetDetails = false;
    for (const auto& nsd : nettingSetDetails_) {
        if (!nsd.emptyOptionalFields())
            hasNettingSetDetails = true;
    }
    return hasNettingSetDetails;
}

void Crif::fillAmountUsd(const QuantLib::ext::shared_ptr<ore::data::Market> market) {
    if (!market) {
        WLOG("CrifLoader::fillAmountUsd() was called, but market object is empty.")
        return;
    }
    CrifRecordContainer results;

    for (const CrifRecord& r : records_) {
        CrifRecord cr = r;
        // Fill in amount USD if it is missing and if CRIF record requires it (i.e. if it has amount and amount
        // currency, and risk type is neither AddOnNotionalFactor or ProductClassMultiplier)
        if (cr.requiresAmountUsd() && !cr.hasAmountUsd()) {
            if (!cr.hasAmount() || !cr.hasAmountCcy()) {
                ore::data::StructuredTradeWarningMessage(
                    cr.tradeId, cr.tradeType, "Populating CRIF amount USD",
                    "CRIF record is missing one of Amount and AmountCurrency, and there is no amountUsd value to "
                    "fall back to: " +
                        ore::data::to_string(cr))
                    .log();
            } else {
                double usdSpot = market->fxRate(cr.amountCurrency + "USD")->value();
                cr.amountUsd = cr.amount * usdSpot;
            }
        }
        results.insert(cr);
    }
    records_ = results;
}

} // namespace analytics
} // namespace ore
