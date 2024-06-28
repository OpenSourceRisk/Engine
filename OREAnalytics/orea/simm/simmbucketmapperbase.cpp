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

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <orea/simm/simmbucketmapperbase.hpp>

#include <ql/errors.hpp>

#include <ostream>

using namespace QuantLib;

using ore::data::checkCurrency;
using ore::data::BasicReferenceDataManager;
using ore::data::XMLDocument;
using ore::data::XMLNode;
using ore::data::XMLUtils;
using std::map;
using std::string;

namespace ore {
namespace analytics {

// Ease syntax
using RiskType = CrifRecord::RiskType;

// Helper variable for switching risk type before lookup
const map<RiskType, RiskType> nonVolRiskTypeMap = {{RiskType::IRVol, RiskType::IRCurve},
                                                   {RiskType::InflationVol, RiskType::IRCurve},
                                                   {RiskType::CreditVol, RiskType::CreditQ},
                                                   {RiskType::CreditVolNonQ, RiskType::CreditNonQ},
                                                   {RiskType::EquityVol, RiskType::Equity},
                                                   {RiskType::CommodityVol, RiskType::Commodity}};

SimmBucketMapperBase::SimmBucketMapperBase(
    const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& refDataManager,
    const QuantLib::ext::shared_ptr<SimmBasicNameMapper>& nameMapper) :
    refDataManager_(refDataManager), nameMapper_(nameMapper){

    // Fill the set of risk types that have buckets
    rtWithBuckets_ = {RiskType::IRCurve,       RiskType::CreditQ,      RiskType::CreditNonQ,   RiskType::Equity,
                      RiskType::Commodity,     RiskType::IRVol,        RiskType::InflationVol, RiskType::CreditVol,
                      RiskType::CreditVolNonQ, RiskType::EquityVol,    RiskType::CommodityVol};
}

std::string BucketMapping::name() const {
    std::ostringstream o;
    o << bucket_ << "-" << validFrom_ << "-" << validTo_ << "-" << fallback_;
    return o.str();
}   

bool operator<(const BucketMapping &a, const BucketMapping &b) {
    return a.name() < b.name();
}
    
QuantLib::Date BucketMapping::validToDate() const {
    if (validTo_.empty())
        return Date::maxDate();
    else
        return ore::data::parseDate(validTo_);
}

QuantLib::Date BucketMapping::validFromDate() const {
    if (validFrom_.empty())
        return Date::minDate();
    else
        return ore::data::parseDate(validFrom_);
}

string SimmBucketMapperBase::bucket(const RiskType& riskType, const string& qualifier) const {

    auto key = std::make_pair(riskType, qualifier);
    if (auto b = cache_.find(key); b != cache_.end())
        return b->second;

    QL_REQUIRE(hasBuckets(riskType), "The risk type " << riskType << " does not have buckets");

    // Vol risk type bucket mappings are stored in their non-vol counterparts
    auto lookupRiskType = riskType;
    auto nv = nonVolRiskTypeMap.find(riskType);
    if (nv != nonVolRiskTypeMap.end()) {
        lookupRiskType = nv->second;
    }

    // Deal with RiskType::IRCurve
    if (lookupRiskType == RiskType::IRCurve || lookupRiskType == RiskType::GIRR_DELTA) {
        auto tmp = irBucket(qualifier);
        cache_[key] = tmp;
        return tmp;
    }

    string bucket;
    string lookupName = qualifier;

    bool haveMapping = has(lookupRiskType, lookupName, false);
    bool haveFallback = has(lookupRiskType, lookupName, true);
    bool noBucket = !haveMapping && !haveFallback;

    if (noBucket && nameMapper_ != nullptr &&
        (lookupRiskType == RiskType::Equity || lookupRiskType == RiskType::EQ_DELTA)) {
        // if we have a simm name mapping we do a reverse lookup on the name as the CRIF qualifier isn't in reference data
        lookupName = nameMapper_->externalName(qualifier);
        haveMapping = has(lookupRiskType, lookupName, false);
        noBucket = !haveMapping && !haveFallback;
    }

    // Do some checks and return the lookup
    if (noBucket) {
        if (lookupRiskType == RiskType::Commodity) {
            bool commIndex = false;
            // first check is there is an entry under equity reference, this may tell us if it is an index
            if (refDataManager_ != nullptr && refDataManager_->hasData("Equity", lookupName)) {
                auto refData = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityReferenceDatum>(
                    refDataManager_->getData("Equity", lookupName));
                if (refData->equityData().isIndex)
                    commIndex = true;
            }

            
            // Commodity does not allow "Residual", but bucket 16 is "Other", bucket 17 for Indices
            // FAQ Methodology and implementation 20180124 (H3) says to use "Other"
            bucket = commIndex ? "17" : "16";                        
            TLOG("Don't have any bucket mappings for the "
                << "combination of risk type " << riskType << " and qualifier " << lookupName
                << " - assigning to bucket " << bucket);

        } else if (lookupRiskType == RiskType::Equity) {
            // check if it is an index
            if (refDataManager_ != nullptr && refDataManager_->hasData("Equity", lookupName)) {
                auto refData = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityReferenceDatum>(refDataManager_->getData("Equity", lookupName));
                // if the equity is an index we assign to bucket 11
                if (refData->equityData().isIndex) {
                    TLOG("Don't have any bucket mappings for the "
                        << "combination of risk type " << riskType << " and qualifier " << lookupName
                        << " - assigning to bucket 11");
                    bucket = "11";
                }
            }
        } 
        
        if (bucket.empty()) {
            TLOG("Don't have any bucket mappings for the "
                << "combination of risk type " << riskType << " and qualifier " << lookupName
                << " - assigning to Residual/Other bucket");
            bucket = "Residual";
        }

        FailedMapping fm;
        fm.name = qualifier;
        fm.lookupName = lookupName;
        fm.riskType = riskType;
        fm.lookupRiskType = lookupRiskType;
        failedMappings_.insert(fm);

    } else {
        // We may have several mappings per qualifier, pick the first valid one that matches the fallback flag
        Date today = Settings::instance().evaluationDate();
        for (auto m : bucketMapping_.at(lookupRiskType).at(lookupName)) {
            if (m.validToDate() >= today && m.validFromDate() <= today && m.fallback() == !haveMapping) {
                bucket = m.bucket();
                cache_[key] = bucket;
                return bucket;
            }
        }
        TLOG("bucket mapping for risk type " << riskType << " and qualifier " << qualifier << " inactive, return Residual");
        bucket = "Residual";
    }

    cache_[key] = bucket;
    return bucket;
}

bool SimmBucketMapperBase::hasBuckets(const RiskType& riskType) const { return rtWithBuckets_.count(riskType) > 0; }

bool SimmBucketMapperBase::has(const RiskType& riskType, const string& qualifier,
                               boost::optional<bool> fallback) const {

    // Vol risk type bucket mappings are stored in their non-vol counterparts
    auto lookupRiskType = riskType;
    auto nv = nonVolRiskTypeMap.find(riskType);
    if (nv != nonVolRiskTypeMap.end()) {
        lookupRiskType = nv->second;
    }

    if (lookupRiskType == RiskType::IRCurve || lookupRiskType == RiskType::GIRR_DELTA)
        return true;

    auto bm = bucketMapping_.find(lookupRiskType);
    if (bm == bucketMapping_.end())
        return false;

    auto q = bm->second.find(qualifier);
    if (q == bm->second.end())
        return false;

    Date today = Settings::instance().evaluationDate();

    // We may have several mappings (several periods, override and fallback mappings)
    // So pick the first valid one

    for (const auto& m : q->second) {
        Date validTo = m.validToDate();
        Date validFrom = m.validFromDate();
        if (validTo >= today && validFrom <= today && (fallback ? *fallback == m.fallback() : true))
            return true;
    }
    return false;
}

void SimmBucketMapperBase::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "SIMMBucketMappings");

    // Every time a call to fromXML is made, reset the bucket mapper to its initial state
    reset();

    LOG("Start parsing SIMMBucketMappings");

    for (XMLNode* rtNode = XMLUtils::getChildNode(node); rtNode; rtNode = XMLUtils::getNextSibling(rtNode)) {
        // Get the risk type that we are dealing with
        string strRiskType = XMLUtils::getNodeName(rtNode);
        RiskType riskType = parseRiskType(strRiskType);

        checkRiskType(riskType);

        QL_REQUIRE(bucketMapping_.count(riskType) == 0,
                   "Can only have one node for each risk type. " << strRiskType << " appears more than once.");
        bucketMapping_[riskType] = {};

        // Loop over and add the bucket mappings for this risk type
        for (XMLNode* mappingNode = XMLUtils::getChildNode(rtNode, "Mapping"); mappingNode;
             mappingNode = XMLUtils::getNextSibling(mappingNode, "Mapping")) {

            string validTo = XMLUtils::getChildValue(mappingNode, "ValidTo", false);
            string validFrom = XMLUtils::getChildValue(mappingNode, "ValidFrom", false);
            string qualifier = XMLUtils::getChildValue(mappingNode, "Qualifier", false);
            string bucket = XMLUtils::getChildValue(mappingNode, "Bucket", false);
            if (bucket == "" || qualifier == "") {
                ALOG("skip bucket mapping for quaifier '" << qualifier << "' and bucket '" << bucket << "'");
                continue;
            }
            string fallbackString = XMLUtils::getChildValue(mappingNode, "Fallback", false);
            bool fallback = false;
            if (fallbackString != "")
                fallback = ore::data::parseBool(fallbackString);

            if (validTo != "") {
                // Check whether the provided validTo element is a valid date, ALERT and ignore if it is not
                try {
                    ore::data::parseDate(validTo);
                } catch(std::exception&) {
                    ALOG("Cannot parse bucket mapping validTo " << validTo << " for qualifier " << qualifier << ", ignore");
                    validTo = "";
                }
            }  
            if (validFrom != "") {
                // Check whether the provided validFrom element is a valid date, ALERT and ignore if it is not
                try {
                    ore::data::parseDate(validFrom);
                } catch(std::exception&) {
                    ALOG("Cannot parse bucket mapping validFrom " << validFrom << " for qualifier " << qualifier << ", ignore");
                    validFrom = "";
                }
            }  

            // Add mapping
            BucketMapping mapping(bucket, validFrom, validTo, fallback);
            if (bucketMapping_[riskType].find(qualifier) == bucketMapping_[riskType].end())
                bucketMapping_[riskType][qualifier] = std::set<BucketMapping>();
            bucketMapping_[riskType][qualifier].insert(mapping);
            TLOG("Added SIMM bucket mapping: {" << riskType << ": {" << qualifier << ", " << bucket << ", " << validFrom << ", " << validTo << ", " << fallback << "}}");
            if (bucket != "Residual") {
                int bucketInt = ore::data::parseInteger(bucket);
                QL_REQUIRE(bucketInt >= 1, "found bucket " << bucket << ", expected >= 1"); 
            }
        }
    }

    LOG("Finished parsing SIMMBucketMappings");
}

XMLNode* SimmBucketMapperBase::toXML(ore::data::XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("SIMMBucketMappings");
    for (auto const& b : bucketMapping_) {
        XMLNode* n = doc.allocNode(ore::data::to_string(b.first));
        XMLUtils::appendNode(node, n);
        for (auto const& bms : b.second) {
            for (auto const& bm : bms.second) {
                XMLNode* m = doc.allocNode("Mapping");
                XMLUtils::appendNode(n, m);
                if (!bms.first.empty())
                    XMLUtils::addChild(doc, m, "Qualifier", bms.first);
                if (!bm.validTo().empty())
                    XMLUtils::addChild(doc, m, "ValidTo", bm.validTo());
                if (!bm.validFrom().empty())
                    XMLUtils::addChild(doc, m, "ValidFrom", bm.validFrom());
                if (!bm.bucket().empty())
                    XMLUtils::addChild(doc, m, "Bucket", bm.bucket());
                if (bm.fallback())
                    XMLUtils::addChild(doc, m, "Fallback", bm.fallback());
            }
        }
    }
    return node;
}

void SimmBucketMapperBase::addMapping(const RiskType& riskType, const string& qualifier, const string& bucket,
                                      const string& validFrom, const string& validTo, bool fallback) {

    cache_.clear();

    // Possibly map to non-vol counterpart for lookup
    RiskType rt = riskType;
    if (nonVolRiskTypeMap.count(riskType) > 0) {
        rt = nonVolRiskTypeMap.at(riskType);
    }

    if (rt == RiskType::IRCurve) {
        // IR has internal mapping so return early - no need for warning
        // Could catch commodity here but allow extra mappings there
        return;
    }

    QL_REQUIRE(hasBuckets(riskType),
               "Tried to add a bucket mapping for risk type " << riskType << " but it does not have buckets.");

    if (bucketMapping_[rt].find(qualifier) == bucketMapping_[rt].end())
        bucketMapping_[rt][qualifier] = std::set<BucketMapping>();

    std::string vf = validFrom, vt = validTo;
    if (vf != "") {
        try {
            ore::data::parseDate(vf);
        } catch(std::exception& e) {
            ALOG("Error parsing validFrom date, ignore: " << e.what());
            vf = "";
        }
    }
    if (vt != "") {
        try {
            ore::data::parseDate(vt);
        } catch(std::exception& e) {
            ALOG("Error parsing validTo date, ignore: " << e.what());
            vt = "";
        }
    }
    
    bucketMapping_[rt][qualifier].insert(BucketMapping(bucket, vt, vf, fallback));
}

string SimmBucketMapperBase::irBucket(const string& qualifier) const {
    if (qualifier == "USD" || qualifier == "EUR" || qualifier == "GBP" || qualifier == "AUD" || qualifier == "CAD" ||
        qualifier == "CHF" || qualifier == "DKK" || qualifier == "HKD" || qualifier == "KRW" || qualifier == "NOK" ||
        qualifier == "NZD" || qualifier == "SEK" || qualifier == "SGD" || qualifier == "TWD")
        return "1";
    if (qualifier == "JPY")
        return "2";
    return "3";
}

void SimmBucketMapperBase::checkRiskType(const RiskType& riskType) const {
    QL_REQUIRE(riskType != RiskType::IRCurve, "Risk type " << RiskType::IRCurve << " is mapped to buckets internally.");

    QL_REQUIRE(hasBuckets(riskType), "The risk type " << riskType << " does not have buckets.");

    QL_REQUIRE(nonVolRiskTypeMap.count(riskType) == 0, "The vol risk type "
                                                           << "mappings are stored in their non-vol counterparts. Use "
                                                           << nonVolRiskTypeMap.at(riskType) << " instead of "
                                                           << riskType << ".");
}

void SimmBucketMapperBase::reset() {
    cache_.clear();
    // Clear the bucket mapper and add back the commodity mappings
    bucketMapping_.clear();
    failedMappings_.clear();
}

} // namespace analytics
} // namespace ore
