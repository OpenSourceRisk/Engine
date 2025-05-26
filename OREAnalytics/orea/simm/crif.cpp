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
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/crif.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace analytics {

using std::ostream;
using std::pair;
using ore::data::NettingSetDetails;
using std::make_tuple;
using std::make_pair;

auto isSimmParameter = [](const SlimCrifRecord& x) { return x.isSimmParameter(); };
auto isNotSimmParameter = std::not_fn(isSimmParameter);

SlimCrifRecord::SlimCrifRecord(const QuantLib::ext::weak_ptr<Crif>& crif) {
    crif_ = crif;
}

void Crif::addRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies, bool sortFxVolQualifer) {
    SlimCrifRecord newCrifRecord;

    // If we are adding a slim record that already belongs to another CRIF, we need to now attach this record to this
    // CRIF so that its integer values correspond to this CRIF's map values.
    auto thisPtr = shared_from_this();
    if (record.crif().lock() != thisPtr)
        newCrifRecord = SlimCrifRecord(QuantLib::ext::weak_ptr<Crif>(thisPtr), record);
    else
        newCrifRecord = record;

    if (newCrifRecord.type() == CrifRecord::RecordType::FRTB) {
        addFrtbCrifRecord(newCrifRecord, aggregateDifferentAmountCurrencies, sortFxVolQualifer);
    } else if (newCrifRecord.type() == CrifRecord::RecordType::SIMM && !newCrifRecord.isSimmParameter()) {
        addSimmCrifRecord(newCrifRecord, aggregateDifferentAmountCurrencies, sortFxVolQualifer);
    } else {
        addSimmParameterRecord(newCrifRecord, aggregateDifferentAmountCurrencies);
    }
}

void Crif::addRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies, bool sortFxVolQualifer) {
    SlimCrifRecord scr(QuantLib::ext::weak_ptr<Crif>(shared_from_this()), record);
    addRecord(scr, aggregateDifferentAmountCurrencies, sortFxVolQualifer);
}

void Crif::addFrtbCrifRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies,
                             bool sortFxVolQualifer) {
    QL_REQUIRE(type_ == CrifType::Empty || type_ == CrifType::Frtb, "Can not add a FRTB crif record to a SIMM Crif");
    if (type_ == CrifType::Empty) {
        type_ = CrifType::Frtb;
    }
    insertCrifRecord(record, aggregateDifferentAmountCurrencies);
}

void Crif::addSimmCrifRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies,
                             bool sortFxVolQualifer) {
    QL_REQUIRE(type_ == CrifType::Empty || type_ == CrifType::Simm, "Can not add a Simm crif record to a Frtb Crif");
    if (type_ == CrifType::Empty) {
        type_ = CrifType::Simm;
    }
    auto recordToAdd = record;
    if (sortFxVolQualifer && recordToAdd.riskType() == CrifRecord::RiskType::FXVol) {
        auto ccy_1 = recordToAdd.getQualifier().substr(0, 3);
        auto ccy_2 = recordToAdd.getQualifier().substr(3);
        if (ccy_1 > ccy_2)
            ccy_1.swap(ccy_2);
        recordToAdd.setQualifier(ccy_1 + ccy_2);
    }
    insertCrifRecord(recordToAdd, aggregateDifferentAmountCurrencies);
}

void Crif::clear() {
    records_.clear();
    tradeIdIndex_.clear();
    tradeTypeIndex_.clear();
    qualifierIndex_.clear();
    nettingSetDetailsIndex_.clear();
    bucketIndex_.clear();
    label1Index_.clear();
    label2Index_.clear();
    currencyIndex_.clear();
    resultCurrencyIndex_.clear();
    endDateIndex_.clear();
    label3Index_.clear();
    creditQualityIndex_.clear();
    longShortIndIndex_.clear();
    coveredBondIndIndex_.clear();
    trancheThicknessIndex_.clear();
    bbRwIndex_.clear();
    nettingSetDetails_.clear();
}

void Crif::insertCrifRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies) {
    auto newCrifRecord = record;
    if (aggregate_ && newCrifRecord.imModel() != CrifRecord::IMModel::Schedule) {
        newCrifRecord.setTradeId(string());
    }

    auto it = aggregateDifferentAmountCurrencies ? records_.find(newCrifRecord, SlimCrifRecord::amountCcyLTCompare)
                                                 : records_.find(newCrifRecord);

    if (it == records_.end()) {
        records_.insert(newCrifRecord);
        nettingSetDetails_.insert(newCrifRecord.getNettingSetDetails());
    } else {
        updateAmountExistingRecord(it, newCrifRecord);
    }
}

void Crif::addSimmParameterRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies) {

    auto it = aggregateDifferentAmountCurrencies ? records_.find(record, SlimCrifRecord::amountCcyLTCompare)
                                                 : records_.find(record);
    if (it == records_.end()) {
        SlimCrifRecord newRecord(shared_from_this(), record);
        records_.insert(newRecord);
    } else if (it->riskType() == CrifRecord::RiskType::AddOnFixedAmount) {
        updateAmountExistingRecord(it, record);
    } else if (it->riskType() == CrifRecord::RiskType::AddOnNotionalFactor ||
               it->riskType() == CrifRecord::RiskType::ProductClassMultiplier) {
        // Only log warning if the values are not the same. If they are, then there is no material discrepancy.
        if (record.amount() != it->amount()) {
            string errMsg = "Found more than one instance of risk type " + ore::data::to_string(it->riskType()) +
                            ". Please check the SIMM parameters input. If enforceIMRegulations=False, then it "
                            "is possible that multiple entries for different regulations now belong under the same "
                            "'Unspecified' regulation.";
            ore::analytics::StructuredAnalyticsWarningMessage("SIMM", "Aggregating SIMM parameters", errMsg).log();
        }
    }
}

void Crif::updateAmountExistingRecord(SlimCrifRecordContainer::nth_index_iterator<0>::type it,
                                      const SlimCrifRecord& record) {
    bool updated = false;
    if (record.hasAmountUsd()) {
        it->setAmountUsd(it->amountUsd() + record.amountUsd());
        updated = true;
    }
    if (record.hasAmount() && record.hasAmountCcy() && it->getCurrency() == record.getCurrency()) {
        it->setAmount(it->amount() + record.amount());
        updated = true;
    }
    if (record.hasAmountResultCcy() && record.hasResultCcy() && it->getResultCurrency() == record.getResultCurrency()) {
        it->setAmountResultCurrency(it->amountResultCurrency() + record.amountResultCurrency());
        updated = true;
    }
    if (updated)
        DLOG("Updated net CRIF records: " << *it)
}

void Crif::addRecords(const Crif& crif, bool aggregateDifferentAmountCurrencies, bool sortFxVolQualifer) {
    for (const auto& r : crif.records_) {
        addRecord(r, aggregateDifferentAmountCurrencies, sortFxVolQualifer);
    }
}

void Crif::addRecords(const QuantLib::ext::shared_ptr<Crif>& crif, bool aggregateDifferentAmountCurrencies, bool sortFxVolQualifer) {
    if (crif) {
        addRecords(*crif);
    }
}

QuantLib::ext::shared_ptr<Crif> Crif::aggregate(bool aggregateDifferentAmountCurrencies) const {
    MEM_LOG_USING_LEVEL(ORE_WARNING, "Calling Crif::aggregate()");
    
    auto result = QuantLib::ext::make_shared<Crif>();
    result->setAggregate(true);
    for (const auto& cr : records_)
        result->addRecord(cr, aggregateDifferentAmountCurrencies);
    
    MEM_LOG_USING_LEVEL(ORE_WARNING, "Finished Crif::aggregate()");

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
QuantLib::ext::shared_ptr<Crif> Crif::simmParameters() const {
    auto results = QuantLib::ext::make_shared<Crif>();
    for (const auto& record : records_) {
        if (record.isSimmParameter()) {
            results->addRecord(record);
        }
    }
    return results;
}
//! Find first element
pair<SlimCrifRecordContainer::nth_index<1>::type::const_iterator,
     SlimCrifRecordContainer::nth_index<1>::type::const_iterator>
Crif::findBy(const NettingSetDetails nsd, CrifRecord::ProductClass pc, const CrifRecord::RiskType rt,
             const std::string& qualifier) const {
    
    auto& idx = records_.get<QualifierTag>();

    auto nsdIt = nettingSetDetailsIndex_.right.find(nsd);
    auto qIt = qualifierIndex_.right.find(qualifier);
    if (nsdIt == nettingSetDetailsIndex_.right.end() || qIt == qualifierIndex_.right.end())
        return make_pair(idx.end(), idx.end());

    return make_pair(idx.find(make_tuple(nsdIt->second, pc, rt, qIt->second)), idx.end());
};

QuantLib::ext::shared_ptr<Crif> Crif::filterNonZeroAmount(double threshold, std::string alwaysIncludeFxRiskCcy) const {
    LOG("Calling Crif::filterNonZeroAmount()");
    auto results = QuantLib::ext::make_shared<Crif>();
    for (auto record : records_) {
        QL_REQUIRE(record.amount() != QuantLib::Null<QuantLib::Real>() || record.amountUsd() != QuantLib::Null<double>(),
                   "Internal Error, amount and amountUsd are empty");
        double absAmount = 0.0;
        if ((record.amount() != QuantLib::Null<double>()) && (record.amountUsd() != QuantLib::Null<double>())) {
            absAmount = std::max(std::fabs(record.amount()), std::fabs(record.amountUsd()));
        } else if (record.amount() != QuantLib::Null<double>()) {
            absAmount = std::fabs(record.amount());
        } else if (record.amountUsd() != QuantLib::Null<double>()) {
            absAmount = std::fabs(record.amountUsd());
        }
        bool add = (absAmount > threshold && !QuantLib::close_enough(absAmount, threshold));
        if (!alwaysIncludeFxRiskCcy.empty())
            add = add ||
                  (record.riskType() == CrifRecord::RiskType::FX && record.getQualifier() == alwaysIncludeFxRiskCcy);
        if (add) {
            results->addRecord(record);
        }
    }
    return results;
}

std::set<std::string> Crif::qualifiersBy(const NettingSetDetails nsd, CrifRecord::ProductClass pc,
                                         const CrifRecord::RiskType rt) const {
    auto res = records_ | boost::adaptors::filtered([&nsd, &pc, &rt](const SlimCrifRecord& record) {
                   return record.getNettingSetDetails() == nsd && record.productClass() == pc && record.riskType() == rt;
               }) |
               boost::adaptors::transformed([](const SlimCrifRecord& record) { return record.getQualifier(); });
    return boost::copy_range<std::set<std::string>>(res);
}

pair<SlimCrifRecordContainer::nth_index<1>::type::const_iterator,
     SlimCrifRecordContainer::nth_index<1>::type::const_iterator>
Crif::filterByQualifier(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc, const CrifRecord::RiskType rt,
                        const std::string& qualifier) const {

    auto& idx = records_.get<QualifierTag>();

    auto nsdIt = nettingSetDetailsIndex_.right.find(nsd);
    auto qIt = qualifierIndex_.right.find(qualifier);

    if (nsdIt == nettingSetDetailsIndex_.right.end() || qIt == qualifierIndex_.right.end())
        return make_pair(idx.end(), idx.end());

    return idx.equal_range(make_tuple(nsdIt->second, pc, rt, qIt->second));
}

pair<SlimCrifRecordContainer::nth_index<2>::type::const_iterator,
     SlimCrifRecordContainer::nth_index<2>::type::const_iterator>
Crif::filterByBucket(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc, const CrifRecord::RiskType rt,
                     const std::string& bucket) const {

    auto& idx = records_.get<BucketTag>();

    auto nsdIt = nettingSetDetailsIndex_.right.find(nsd);
    auto bIt = bucketIndex_.right.find(bucket);

    if (nsdIt == nettingSetDetailsIndex_.right.end() || bIt == bucketIndex_.right.end())
        return make_pair(idx.end(), idx.end());

    return idx.equal_range(make_tuple(nsdIt->second, pc, rt, bIt->second));
}

pair<SlimCrifRecordContainer::nth_index<3>::type::const_iterator,
     SlimCrifRecordContainer::nth_index<3>::type::const_iterator>
Crif::filterByQualifierAndBucket(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc,
                                 const CrifRecord::RiskType rt, const std::string& qualifier,
                                 const std::string& bucket) const {
    auto& idx = records_.get<QualifierBucketTag>();

    auto nsdIt = nettingSetDetailsIndex_.right.find(nsd);
    auto qIt = qualifierIndex_.right.find(qualifier);
    auto bIt = bucketIndex_.right.find(bucket);

    if (nsdIt == nettingSetDetailsIndex_.right.end() || qIt == qualifierIndex_.right.end() || bIt == bucketIndex_.right.end())
        return make_pair(idx.end(), idx.end());

    return idx.equal_range(make_tuple(nsdIt->second, pc, rt, qIt->second, bIt->second));
}

pair<SlimCrifRecordContainer::nth_index<4>::type::const_iterator,
     SlimCrifRecordContainer::nth_index<4>::type::const_iterator>
Crif::filterBy(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc, const CrifRecord::RiskType rt) const {
    auto& idx = records_.get<RiskTypeTag>();

    auto nsdIt = nettingSetDetailsIndex_.right.find(nsd);

    if (nsdIt == nettingSetDetailsIndex_.right.end())
        return make_pair(idx.end(), idx.end());

    return idx.equal_range(make_tuple(nsdIt->second, pc, rt));
}

std::vector<SlimCrifRecord> Crif::filterBy(const CrifRecord::RiskType rt) const {
    return boost::copy_range<std::vector<SlimCrifRecord>>(
        records_ | boost::adaptors::filtered([&rt](const SlimCrifRecord& record) { return record.riskType() == rt; }));
}

std::vector<SlimCrifRecord> Crif::filterByTradeId(const std::string& id) const {
    return boost::copy_range<std::vector<SlimCrifRecord>>(
        records_ | boost::adaptors::filtered([&id](const SlimCrifRecord& record) { return record.getTradeId() == id; }));
}

std::set<std::string> Crif::tradeIds() const {
    return boost::copy_range<std::set<std::string>>(
        records_ | boost::adaptors::transformed([](const SlimCrifRecord& r) { return r.getTradeId(); }));
}

//! deletes all existing simmParameter and replaces them with the new one
void Crif::setSimmParameters(const QuantLib::ext::shared_ptr<Crif>& crif) {
    if (!crif)
        return;

    auto backup = records_;
    records_.clear();
    for (auto& r : backup) {
        if (!r.isSimmParameter()) {
            addRecord(r);
        }
    }
    for (const auto& r : *crif) {
        if (r.isSimmParameter()) {
            addRecord(r);
        }
    }
}

void Crif::setCrifRecords(const QuantLib::ext::shared_ptr<Crif>& crif) {
    if (!crif)
        return;
    
    auto backup = records_;
    records_.clear();
    for (auto& r : backup) {
        if (r.isSimmParameter()) {
            addRecord(r);
        }
    }
    for (const auto& r : *crif) {
        if (!r.isSimmParameter()) {
            addRecord(r);
        }
    }
}

//! Give back the set of portfolio IDs that have been loaded
set<string> Crif::portfolioIds() const {
    return boost::copy_range<set<string>>(records_ | boost::adaptors::transformed([](const SlimCrifRecord& r) {
                                              return r.getNettingSetDetails().nettingSetId();
                                          }));
}

std::set<CrifRecord::ProductClass> Crif::ProductClassesByNettingSetDetails(const NettingSetDetails nsd) const {
    std::set<CrifRecord::ProductClass> keys;
    for (const auto& record : records_) {
        if (record.getNettingSetDetails() == nsd) {
            keys.insert(record.productClass());
        }
    }
    return keys;
}

size_t Crif::countMatching(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc,
                           const CrifRecord::RiskType rt, const std::string& qualifier) const {
    auto& idx = records_.get<QualifierTag>();

    auto nsdIt = nettingSetDetailsIndex_.right.find(nsd);
    auto qIt = qualifierIndex_.right.find(qualifier);

    if (nsdIt == nettingSetDetailsIndex_.right.end() || qIt == qualifierIndex_.right.end())
        return 0;

    return idx.count(make_tuple(nsdIt->second, pc, rt, qIt->second));
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

    for (auto it = records_.begin(); it != records_.end(); it++) {
        // Fill in amount USD if it is missing and if CRIF record requires it (i.e. if it has amount and amount
        // currency, and risk type is neither AddOnNotionalFactor or ProductClassMultiplier)
        if (it->requiresAmountUsd() && !it->hasAmountUsd()) {
            if (!it->hasAmount() || !it->hasAmountCcy()) {
                ore::data::StructuredTradeWarningMessage(
                    it->getTradeId(), it->getTradeType(), "Populating CRIF amount USD",
                    "CRIF record is missing one of Amount and Currency, and there is no amountUsd value to "
                    "fall back to: " +
                        ore::data::to_string(*it))
                    .log();
            } else {
                double usdSpot = market->fxRate(it->getCurrency() + "USD")->value();
                it->setAmountUsd(it->amount() * usdSpot);
            }
        }
    }
}

const string& Crif::getTradeId(int idx) const {
    QL_REQUIRE(tradeIdIndex_.left.find(idx) != tradeIdIndex_.left.end(), "Crif::getTradeId() : could not find int index " << idx);
    return tradeIdIndex_.left.at(idx);
}

const string& Crif::getTradeType(int idx) const {
    QL_REQUIRE(tradeTypeIndex_.left.find(idx) != tradeTypeIndex_.left.end(),
               "Crif::getTradeType() : could not find int index " << idx);
    return tradeTypeIndex_.left.at(idx);
}

const NettingSetDetails& Crif::getNettingSetDetails(int idx) const {
    QL_REQUIRE(nettingSetDetailsIndex_.left.find(idx) != nettingSetDetailsIndex_.left.end(),
               "Crif::getNettingSetDetails() : could not find int index " << idx);
    return nettingSetDetailsIndex_.left.at(idx);
}

const string& Crif::getQualifier(int idx) const {
    QL_REQUIRE(qualifierIndex_.left.find(idx) != qualifierIndex_.left.end(),
               "Crif::getQualifier() : could not find int index " << idx);
    return qualifierIndex_.left.at(idx);
}

const string& Crif::getBucket(int idx) const {
    QL_REQUIRE(bucketIndex_.left.find(idx) != bucketIndex_.left.end(), "Crif::getBucket() : could not find int index " << idx);
    return bucketIndex_.left.at(idx);
}

const string& Crif::getLabel1(int idx) const {
    QL_REQUIRE(label1Index_.left.find(idx) != label1Index_.left.end(), "Crif::getLabel1() : could not find int index " << idx);
    return label1Index_.left.at(idx);
}

const string& Crif::getLabel2(int idx) const {
    QL_REQUIRE(label2Index_.left.find(idx) != label2Index_.left.end(), "Crif::getLabel2() : could not find int index " << idx);
    return label2Index_.left.at(idx);
}

const string& Crif::getResultCurrency(int idx) const {
    QL_REQUIRE(resultCurrencyIndex_.left.find(idx) != resultCurrencyIndex_.left.end(),
               "Crif::getResultCurrency() : could not find int index " << idx);
    return resultCurrencyIndex_.left.at(idx);
}

const string& Crif::getEndDate(int idx) const {
    QL_REQUIRE(endDateIndex_.left.find(idx) != endDateIndex_.left.end(), "Crif::getEndDate() : could not find int index " << idx);
    return endDateIndex_.left.at(idx);
}

const string& Crif::getCurrency(int idx) const {
    QL_REQUIRE(currencyIndex_.left.find(idx) != currencyIndex_.left.end(),
               "Crif::getCurrency() : could not find int index " << idx);
    return currencyIndex_.left.at(idx);
}

const string& Crif::getLabel3(int idx) const {
    QL_REQUIRE(label3Index_.left.find(idx) != label3Index_.left.end(), "Crif::getLabel3() : could not find int index " << idx);
    return label3Index_.left.at(idx);
}

const string& Crif::getCreditQuality(int idx) const {
    QL_REQUIRE(creditQualityIndex_.left.find(idx) != creditQualityIndex_.left.end(),
               "Crif::getCreditQuality() : could not find int index " << idx);
    return creditQualityIndex_.left.at(idx);
}

const string& Crif::getLongShortInd(int idx) const {
    QL_REQUIRE(longShortIndIndex_.left.find(idx) != longShortIndIndex_.left.end(),
               "Crif::getLongShortInd() : could not find int index " << idx);
    return longShortIndIndex_.left.at(idx);
}

const string& Crif::getCoveredBondInd(int idx) const {
    QL_REQUIRE(coveredBondIndIndex_.left.find(idx) != coveredBondIndIndex_.left.end(),
               "Crif::getCoveredBondInd() : could not find int index " << idx);
    return coveredBondIndIndex_.left.at(idx);
}

const string& Crif::getTrancheThickness(int idx) const {
    QL_REQUIRE(trancheThicknessIndex_.left.find(idx) != trancheThicknessIndex_.left.end(),
               "Crif::getTrancheThickness() : could not find int index " << idx);
    return trancheThicknessIndex_.left.at(idx);
}

const string& Crif::getBbRw(int idx) const {
    QL_REQUIRE(bbRwIndex_.left.find(idx) != bbRwIndex_.left.end(), "Crif::getBbRw() : could not find int index " << idx);
    return bbRwIndex_.left.at(idx);
}

int Crif::updateTradeIdIndex(const string& value) {
    auto it = tradeIdIndex_.right.find(value);
    if (it != tradeIdIndex_.right.end()) {
        return it->second;
    } else {
        int key = tradeIdIndex_.empty() ? 0 : (tradeIdIndex_.left.rbegin()->first + 1);
        tradeIdIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateTradeTypeIndex(const std::string& value) {
    auto it = tradeTypeIndex_.right.find(value);
    if (it != tradeTypeIndex_.right.end()) {
        return it->second;
    } else {
        int key = tradeTypeIndex_.empty() ? 0 : (tradeTypeIndex_.left.rbegin()->first + 1);
        tradeTypeIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateNettingSetDetailsIndex(const NettingSetDetails& value) {
    auto it = nettingSetDetailsIndex_.right.find(value);
    if (it != nettingSetDetailsIndex_.right.end()) {
        return it->second;
    } else {
        int key = nettingSetDetailsIndex_.empty() ? 0 : (nettingSetDetailsIndex_.left.rbegin()->first + 1);
        nettingSetDetailsIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateQualifierIndex(const std::string& value) {
    auto it = qualifierIndex_.right.find(value);
    if (it != qualifierIndex_.right.end()) {
        return it->second;
    } else {
        int key = qualifierIndex_.empty() ? 0 : (qualifierIndex_.left.rbegin()->first + 1);
        qualifierIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateBucketIndex(const std::string& value) {
    auto it = bucketIndex_.right.find(value);
    if (it != bucketIndex_.right.end()) {
        return it->second;
    } else {
        int key = bucketIndex_.empty() ? 0 : (bucketIndex_.left.rbegin()->first + 1);
        bucketIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateLabel1Index(const std::string& value) {
    auto it = label1Index_.right.find(value);
    if (it != label1Index_.right.end()) {
        return it->second;
    } else {
        int key = label1Index_.empty() ? 0 : (label1Index_.left.rbegin()->first + 1);
        label1Index_.insert({key, value});
        return key;
    }
}
int Crif::updateLabel2Index(const std::string& value) {
    auto it = label2Index_.right.find(value);
    if (it != label2Index_.right.end()) {
        return it->second;
    } else {
        int key = label2Index_.empty() ? 0 : (label2Index_.left.rbegin()->first + 1);
        label2Index_.insert({key, value});
        return key;
    }
}
int Crif::updateResultCurrencyIndex(const std::string& value) {
    auto it = resultCurrencyIndex_.right.find(value);
    if (it != resultCurrencyIndex_.right.end()) {
        return it->second;
    } else {
        int key = resultCurrencyIndex_.empty() ? 0 : (resultCurrencyIndex_.left.rbegin()->first + 1);
        resultCurrencyIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateEndDateIndex(const std::string& value) {
    auto it = endDateIndex_.right.find(value);
    if (it != endDateIndex_.right.end()) {
        return it->second;
    } else {
        int key = endDateIndex_.empty() ? 0 : (endDateIndex_.left.rbegin()->first + 1);
        endDateIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateCurrencyIndex(const std::string& value) {
    auto it = currencyIndex_.right.find(value);
    if (it != currencyIndex_.right.end()) {
        return it->second;
    } else {
        int key = currencyIndex_.empty() ? 0 : (currencyIndex_.left.rbegin()->first + 1);
        currencyIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateLabel3Index(const std::string& value) {
    auto it = label3Index_.right.find(value);
    if (it != label3Index_.right.end()) {
        return it->second;
    } else {
        int key = label3Index_.empty() ? 0 : (label3Index_.left.rbegin()->first + 1);
        label3Index_.insert({key, value});
        return key;
    }
}
int Crif::updateCreditQualityIndex(const std::string& value) {
    auto it = creditQualityIndex_.right.find(value);
    if (it != creditQualityIndex_.right.end()) {
        return it->second;
    } else {
        int key = creditQualityIndex_.empty() ? 0 : (creditQualityIndex_.left.rbegin()->first + 1);
        creditQualityIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateLongShortIndIndex(const std::string& value) {
    auto it = longShortIndIndex_.right.find(value);
    if (it != longShortIndIndex_.right.end()) {
        return it->second;
    } else {
        int key = longShortIndIndex_.empty() ? 0 : (longShortIndIndex_.left.rbegin()->first + 1);
        longShortIndIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateCoveredBondIndIndex(const std::string& value) {
    auto it = coveredBondIndIndex_.right.find(value);
    if (it != coveredBondIndIndex_.right.end()) {
        return it->second;
    } else {
        int key = coveredBondIndIndex_.empty() ? 0 : (coveredBondIndIndex_.left.rbegin()->first + 1);
        coveredBondIndIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateTrancheThicknessIndex(const std::string& value) {
    auto it = trancheThicknessIndex_.right.find(value);
    if (it != trancheThicknessIndex_.right.end()) {
        return it->second;
    } else {
        int key = trancheThicknessIndex_.empty() ? 0 : (trancheThicknessIndex_.left.rbegin()->first + 1);
        trancheThicknessIndex_.insert({key, value});
        return key;
    }
}
int Crif::updateBbRwIndex(const std::string& value) {
    auto it = bbRwIndex_.right.find(value);
    if (it != bbRwIndex_.right.end()) {
        return it->second;
    } else {
        int key = bbRwIndex_.empty() ? 0 : (bbRwIndex_.left.rbegin()->first + 1);
        bbRwIndex_.insert({key, value});
        return key;
    }
}

void SlimCrifRecord::updateFromCrifRecord(const CrifRecord& cr) {
    QL_REQUIRE(crif_.lock(), "SlimCrifRecord::updateFromCrifRecord() : Must have a Crif pointer before updating from a CrifRecord");

    // Update all fields whose 
    setTradeId(cr.tradeId);
    setTradeType(cr.tradeType);
    setNettingSetDetails(cr.nettingSetDetails);
    setQualifier(cr.qualifier);
    setBucket(cr.bucket);
    setLabel1(cr.label1);
    setLabel2(cr.label2);
    setResultCurrency(cr.resultCurrency);
    setEndDate(cr.endDate);
    setCurrency(cr.amountCurrency);

    productClass_ = cr.productClass;
    riskType_ = cr.riskType;
    imModel_ = cr.imModel;
    collectRegulations_ = cr.collectRegulations;
    postRegulations_ = cr.postRegulations;
    amount_ = cr.amount;
    amountUsd_ = cr.amountUsd;
    amountResultCcy_ = cr.amountResultCcy;
    additionalFields_ = cr.additionalFields;

    if (type() == CrifRecord::RecordType::FRTB) {
        setLabel3(cr.label3);
        setCreditQuality(cr.creditQuality);
        setLongShortInd(cr.longShortInd);
        setCoveredBondInd(cr.coveredBondInd);
        setTrancheThickness(cr.trancheThickness);
        setBbRw(cr.bb_rw);
    }
}

void SlimCrifRecord::updateFromSlimCrifRecord(const SlimCrifRecord& cr) {
    // Update all fields whose
    setTradeId(cr.getTradeId());
    setTradeType(cr.getTradeType());
    setNettingSetDetails(cr.getNettingSetDetails());
    setQualifier(cr.getQualifier());
    setBucket(cr.getBucket());
    setLabel1(cr.getLabel1());
    setLabel2(cr.getLabel2());
    setResultCurrency(cr.getResultCurrency());
    setEndDate(cr.getEndDate());
    setCurrency(cr.getCurrency());

    productClass_ = cr.productClass();
    riskType_ = cr.riskType();
    imModel_ = cr.imModel();
    collectRegulations_ = cr.collectRegulations();
    postRegulations_ = cr.postRegulations();
    amount_ = cr.amount();
    amountUsd_ = cr.amountUsd();
    amountResultCcy_ = cr.amountResultCurrency();
    additionalFields_ = cr.additionalFields();

    if (type() == CrifRecord::RecordType::FRTB) {
        setLabel3(cr.getLabel3());
        setCreditQuality(cr.getCreditQuality());
        setLongShortInd(cr.getLongShortInd());
        setCoveredBondInd(cr.getCoveredBondInd());
        setTrancheThickness(cr.getTrancheThickness());
        setBbRw(cr.getBbRw());
    }
}

SlimCrifRecord::SlimCrifRecord(const QuantLib::ext::weak_ptr<Crif>& crif, const CrifRecord& cr) : SlimCrifRecord(crif) {
    updateFromCrifRecord(cr);
}

SlimCrifRecord::SlimCrifRecord(const QuantLib::ext::weak_ptr<Crif>& crif, const SlimCrifRecord& cr) : SlimCrifRecord(crif) {
    updateFromSlimCrifRecord(cr);
}

CrifRecord::RecordType SlimCrifRecord::type() const {
    switch (riskType_) {
    case CrifRecord::RiskType::Commodity:
    case CrifRecord::CrifRecord::RiskType::CommodityVol:
    case CrifRecord::RiskType::CreditNonQ:
    case CrifRecord::RiskType::CreditQ:
    case CrifRecord::RiskType::CreditVol:
    case CrifRecord::RiskType::CreditVolNonQ:
    case CrifRecord::RiskType::Equity:
    case CrifRecord::RiskType::EquityVol:
    case CrifRecord::RiskType::FX:
    case CrifRecord::RiskType::FXVol:
    case CrifRecord::RiskType::Inflation:
    case CrifRecord::RiskType::IRCurve:
    case CrifRecord::RiskType::IRVol:
    case CrifRecord::RiskType::InflationVol:
    case CrifRecord::RiskType::BaseCorr:
    case CrifRecord::RiskType::XCcyBasis:
    case CrifRecord::RiskType::ProductClassMultiplier:
    case CrifRecord::RiskType::AddOnNotionalFactor:
    case CrifRecord::RiskType::Notional:
    case CrifRecord::RiskType::AddOnFixedAmount:
    case CrifRecord::RiskType::PV:
        return CrifRecord::RecordType::SIMM;
    case CrifRecord::RiskType::GIRR_DELTA:
    case CrifRecord::RiskType::GIRR_VEGA:
    case CrifRecord::RiskType::GIRR_CURV:
    case CrifRecord::RiskType::CSR_NS_DELTA:
    case CrifRecord::RiskType::CSR_NS_VEGA:
    case CrifRecord::RiskType::CSR_NS_CURV:
    case CrifRecord::RiskType::CSR_SNC_DELTA:
    case CrifRecord::RiskType::CSR_SNC_VEGA:
    case CrifRecord::RiskType::CSR_SNC_CURV:
    case CrifRecord::RiskType::CSR_SC_DELTA:
    case CrifRecord::RiskType::CSR_SC_VEGA:
    case CrifRecord::RiskType::CSR_SC_CURV:
    case CrifRecord::RiskType::EQ_DELTA:
    case CrifRecord::RiskType::EQ_VEGA:
    case CrifRecord::RiskType::EQ_CURV:
    case CrifRecord::RiskType::COMM_DELTA:
    case CrifRecord::RiskType::COMM_VEGA:
    case CrifRecord::RiskType::COMM_CURV:
    case CrifRecord::RiskType::FX_DELTA:
    case CrifRecord::RiskType::FX_VEGA:
    case CrifRecord::RiskType::FX_CURV:
    case CrifRecord::RiskType::DRC_NS:
    case CrifRecord::RiskType::DRC_SNC:
    case CrifRecord::RiskType::DRC_SC:
    case CrifRecord::RiskType::RRAO_1_PERCENT:
    case CrifRecord::RiskType::RRAO_01_PERCENT:
        return CrifRecord::RecordType::FRTB;
    case CrifRecord::RiskType::All:
    case CrifRecord::RiskType::Empty:
        return CrifRecord::RecordType::Generic;
    default:
        QL_FAIL("SlimCrifRecord::type(): Unexpected CrifRecord::RiskType " << riskType_);
    }
}

const string& SlimCrifRecord::getTradeId() const { return crif_.lock()->getTradeId(tradeId_); }
const string& SlimCrifRecord::getTradeType() const { return crif_.lock()->getTradeType(tradeType_); }
const NettingSetDetails& SlimCrifRecord::getNettingSetDetails() const {
    return crif_.lock()->getNettingSetDetails(nettingSetDetails_);
}
const string& SlimCrifRecord::getQualifier() const { return crif_.lock()->getQualifier(qualifier_); }
const string& SlimCrifRecord::getBucket() const { return crif_.lock()->getBucket(bucket_); }
const string& SlimCrifRecord::getLabel1() const { return crif_.lock()->getLabel1(label1_); }
const string& SlimCrifRecord::getLabel2() const { return crif_.lock()->getLabel2(label2_); }
const string& SlimCrifRecord::getResultCurrency() const { return crif_.lock()->getResultCurrency(resultCurrency_); }
const string& SlimCrifRecord::getEndDate() const { return crif_.lock()->getEndDate(endDate_); }
const string& SlimCrifRecord::getCurrency() const { return crif_.lock()->getCurrency(currency_); }

const string& SlimCrifRecord::getLabel3() const { return crif_.lock()->getLabel3(label3_); }
const string& SlimCrifRecord::getCreditQuality() const { return crif_.lock()->getCreditQuality(creditQuality_); }
const string& SlimCrifRecord::getLongShortInd() const { return crif_.lock()->getLongShortInd(longShortInd_); }
const string& SlimCrifRecord::getCoveredBondInd() const { return crif_.lock()->getCoveredBondInd(coveredBondInd_); }
const string& SlimCrifRecord::getTrancheThickness() const {
    return crif_.lock()->getTrancheThickness(trancheThickness_);
}
const string& SlimCrifRecord::getBbRw() const { return crif_.lock()->getBbRw(bb_rw_); }

void SlimCrifRecord::setCrif(const QuantLib::ext::weak_ptr<Crif>& crif) {
    auto crifRecord = toCrifRecord();
    crif_ = crif;
    updateFromCrifRecord(crifRecord);
}

void SlimCrifRecord::setTradeId(const string& value) { tradeId_ = crif_.lock()->updateTradeIdIndex(value); }

void SlimCrifRecord::setTradeType(const string& value) { tradeType_ = crif_.lock()->updateTradeTypeIndex(value); }

void SlimCrifRecord::setNettingSetDetails(const ore::data::NettingSetDetails& value) {
    nettingSetDetails_ = crif_.lock()->updateNettingSetDetailsIndex(value);
}

void SlimCrifRecord::setQualifier(const string& value) { qualifier_ = crif_.lock()->updateQualifierIndex(value); }

void SlimCrifRecord::setBucket(const string& value) { bucket_ = crif_.lock()->updateBucketIndex(value); }

void SlimCrifRecord::setLabel1(const string& value) { label1_ = crif_.lock()->updateLabel1Index(value); }

void SlimCrifRecord::setLabel2(const string& value) { label2_ = crif_.lock()->updateLabel2Index(value); }

void SlimCrifRecord::setResultCurrency(const string& value) const {
    resultCurrency_ = crif_.lock()->updateResultCurrencyIndex(value);
}

void SlimCrifRecord::setEndDate(const string& value) { endDate_ = crif_.lock()->updateEndDateIndex(value); }

void SlimCrifRecord::setCurrency(const string& value) { currency_ = crif_.lock()->updateCurrencyIndex(value); }

void SlimCrifRecord::setLabel3(const string& value) { label3_ = crif_.lock()->updateLabel3Index(value); }

void SlimCrifRecord::setCreditQuality(const string& value) {
    creditQuality_ = crif_.lock()->updateCreditQualityIndex(value);
}

void SlimCrifRecord::setLongShortInd(const string& value) {
    longShortInd_ = crif_.lock()->updateLongShortIndIndex(value);
}

void SlimCrifRecord::setCoveredBondInd(const string& value) {
    coveredBondInd_ = crif_.lock()->updateCoveredBondIndIndex(value);
}

void SlimCrifRecord::setTrancheThickness(const string& value) {
    trancheThickness_ = crif_.lock()->updateTrancheThicknessIndex(value);
}

void SlimCrifRecord::setBbRw(const string& value) { bb_rw_ = crif_.lock()->updateBbRwIndex(value); }

ostream& operator<<(ostream& out, const SlimCrifRecord& cr) {
    const NettingSetDetails& n = cr.getNettingSetDetails();
    if (n.empty()) {
        out << "[" << cr.getTradeId() << ", " << cr.getNettingSetDetails().nettingSetId() << ", " << cr.productClass()
            << ", " << cr.riskType() << ", " << cr.getQualifier() << ", " << cr.getBucket() << ", " << cr.getLabel1()
            << ", " << cr.getLabel2() << ", " << cr.getCurrency() << ", " << cr.amount() << ", "
            << cr.amountUsd();
    } else {
        out << "[" << cr.getTradeId() << ", [" << n << "], " << cr.productClass() << ", " << cr.riskType() << ", "
            << cr.getQualifier() << ", " << cr.getBucket() << ", " << cr.getLabel1() << ", " << cr.getLabel2() << ", "
            << cr.getCurrency() << ", " << cr.amount() << ", " << cr.amountUsd();
    }

    if (!cr.collectRegulations().empty())
        out << ", collect_regulations=" << cr.collectRegulations();
    if (!cr.postRegulations().empty())
        out << ", post_regulations=" << cr.postRegulations();

    out << "]";

    return out;
}

CrifRecord SlimCrifRecord::toCrifRecord() const {
    CrifRecord cr(getTradeId(), getTradeType(), getNettingSetDetails(), productClass(), riskType(), getQualifier(),
                  getBucket(), getLabel1(), getLabel2(), getCurrency(), amount(), amountUsd(), imModel(),
                  collectRegulations(), postRegulations(), getEndDate());

    cr.additionalFields.insert(additionalFields_.begin(), additionalFields_.end());

    if (type() == CrifRecord::RecordType::FRTB) {
        cr.label3 = getLabel3();
        cr.creditQuality = getCreditQuality();
        cr.longShortInd = getLongShortInd();
        cr.coveredBondInd = getCoveredBondInd();
        cr.trancheThickness = getTrancheThickness();
        cr.bb_rw = getBbRw();
    }

    cr.resultCurrency = getResultCurrency();
    cr.amountResultCcy = amountResultCurrency();

    return cr;
}


} // namespace analytics
} // namespace ore
