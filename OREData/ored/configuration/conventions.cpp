/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2024 Oleg Kulkov
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

/*! \file configuration/conventions.cpp
    \brief Currency and instrument specific conventions
    \ingroup
*/

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/tokenizer.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

using namespace QuantLib;
using namespace std;
using boost::lexical_cast;
using QuantExt::SubPeriodsCoupon1;

namespace {

// TODO move to parsers
SubPeriodsCoupon1::Type parseSubPeriodsCouponType(const string& s) {
    if (s == "Compounding")
        return SubPeriodsCoupon1::Compounding;
    else if (s == "Averaging")
        return SubPeriodsCoupon1::Averaging;
    else
        QL_FAIL("SubPeriodsCoupon type " << s << " not recognized");
};

void checkContinuationMappings(const map<Natural, Natural>& mp, const string& name) {
    
    Natural previousValue = 0;
    for (const auto& kv : mp) {
        QL_REQUIRE(kv.first <= kv.second, "Not allowed a " << name << " continuation mapping where From (" <<
            kv.first << ") is greater than To (" << kv.second << ").");
        QL_REQUIRE(kv.second > previousValue, "The To " << name << " continuation mappings should be strictly " <<
            "increasing but got " << kv.second << " <= " << previousValue);
        previousValue = kv.second;
    }
}

} // namespace

namespace ore {
namespace data {

Convention::Convention(const string& id, Type type) : type_(type), id_(id) {}

const QuantLib::ext::shared_ptr<ore::data::Conventions>& InstrumentConventions::conventions(QuantLib::Date d) const {
    QL_REQUIRE(!conventions_.empty(), "InstrumentConventions: No conventions provided.");
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    Date dt = d == Date() ? Settings::instance().evaluationDate() : d;
    auto it = conventions_.lower_bound(dt);
    if(it != conventions_.end() && it->first == dt)
        return it->second;
    QL_REQUIRE(it != conventions_.begin(), "InstrumentConventions: Could not find conventions for " << dt);
    --it;
    constexpr std::size_t max_num_warnings = 10;
    if (numberOfEmittedWarnings_ < max_num_warnings) {
        ++numberOfEmittedWarnings_;
        WLOG("InstrumentConventions: Could not find conventions for "
             << dt << ", using conventions from " << it->first
             << (numberOfEmittedWarnings_ == max_num_warnings ? " (no more warnings of this type will be emitted)"
                                                              : ""));
    }
    return it->second;
}

void InstrumentConventions::setConventions(
    const QuantLib::ext::shared_ptr<ore::data::Conventions>& conventions, QuantLib::Date d) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    conventions_[d] = conventions;
}

ZeroRateConvention::ZeroRateConvention(const string& id, const string& dayCounter, const string& compounding,
                                       const string& compoundingFrequency)
    : Convention(id, Type::Zero), tenorBased_(false), strDayCounter_(dayCounter), strCompounding_(compounding),
      strCompoundingFrequency_(compoundingFrequency) {
    build();
}

ZeroRateConvention::ZeroRateConvention(const string& id, const string& dayCounter, const string& tenorCalendar,
                                       const string& compounding, const string& compoundingFrequency,
                                       const string& spotLag, const string& spotCalendar, const string& rollConvention,
                                       const string& eom)
    : Convention(id, Type::Zero), tenorBased_(true), strDayCounter_(dayCounter), strTenorCalendar_(tenorCalendar),
      strCompounding_(compounding), strCompoundingFrequency_(compoundingFrequency), strSpotLag_(spotLag),
      strSpotCalendar_(spotCalendar), strRollConvention_(rollConvention), strEom_(eom) {
    build();
}

void ZeroRateConvention::build() {
    dayCounter_ = parseDayCounter(strDayCounter_);
    compounding_ = strCompounding_.empty() ? Continuous : parseCompounding(strCompounding_);
    compoundingFrequency_ = strCompoundingFrequency_.empty() ? Annual : parseFrequency(strCompoundingFrequency_);
    if (tenorBased_) {
        tenorCalendar_ = parseCalendar(strTenorCalendar_);
        spotLag_ = strSpotLag_.empty() ? 0 : lexical_cast<Natural>(strSpotLag_);
        spotCalendar_ = strSpotCalendar_.empty() ? NullCalendar() : parseCalendar(strSpotCalendar_);
        rollConvention_ = strRollConvention_.empty() ? Following : parseBusinessDayConvention(strRollConvention_);
        eom_ = strEom_.empty() ? false : parseBool(strEom_);
    }
}

void ZeroRateConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Zero");
    type_ = Type::Zero;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    tenorBased_ = XMLUtils::getChildValueAsBool(node, "TenorBased", true);

    // Get string values from xml
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strCompoundingFrequency_ = XMLUtils::getChildValue(node, "CompoundingFrequency", false);
    strCompounding_ = XMLUtils::getChildValue(node, "Compounding", false);
    if (tenorBased_) {
        strTenorCalendar_ = XMLUtils::getChildValue(node, "TenorCalendar", true);
        strSpotLag_ = XMLUtils::getChildValue(node, "SpotLag", false);
        strSpotCalendar_ = XMLUtils::getChildValue(node, "SpotCalendar", false);
        strRollConvention_ = XMLUtils::getChildValue(node, "RollConvention", false);
        strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    }
    build();
}

XMLNode* ZeroRateConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("Zero");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "TenorBased", tenorBased_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "CompoundingFrequency", strCompoundingFrequency_);
    XMLUtils::addChild(doc, node, "Compounding", strCompounding_);
    if (tenorBased_) {
        XMLUtils::addChild(doc, node, "TenorCalendar", strTenorCalendar_);
        XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
        XMLUtils::addChild(doc, node, "SpotCalendar", strSpotCalendar_);
        XMLUtils::addChild(doc, node, "RollConvention", strRollConvention_);
        XMLUtils::addChild(doc, node, "EOM", strEom_);
    }

    return node;
}

DepositConvention::DepositConvention(const string& id, const string& index)
    : Convention(id, Type::Deposit), index_(index), indexBased_(true) {}

DepositConvention::DepositConvention(const string& id, const string& calendar, const string& convention,
                                     const string& eom, const string& dayCounter, const string& settlementDays)
    : Convention(id, Type::Deposit), indexBased_(false), strCalendar_(calendar), strConvention_(convention),
      strEom_(eom), strDayCounter_(dayCounter), strSettlementDays_(settlementDays) {
    build();
}

void DepositConvention::build() {
    calendar_ = parseCalendar(strCalendar_);
    convention_ = parseBusinessDayConvention(strConvention_);
    eom_ = parseBool(strEom_);
    dayCounter_ = parseDayCounter(strDayCounter_);
    settlementDays_ = parseInteger(strSettlementDays_);
}

void DepositConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Deposit");
    type_ = Type::Deposit;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    indexBased_ = XMLUtils::getChildValueAsBool(node, "IndexBased", true);

    // Get string values from xml
    if (indexBased_) {
        index_ = XMLUtils::getChildValue(node, "Index", true);
    } else {
        strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
        strConvention_ = XMLUtils::getChildValue(node, "Convention", true);
        strEom_ = XMLUtils::getChildValue(node, "EOM", true);
        strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
        strSettlementDays_ = XMLUtils::getChildValue(node, "SettlementDays", true);
        build();
    }
}

XMLNode* DepositConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("Deposit");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "IndexBased", indexBased_);
    if (indexBased_) {
        XMLUtils::addChild(doc, node, "Index", index_);
    } else {
        XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
        XMLUtils::addChild(doc, node, "Convention", strConvention_);
        XMLUtils::addChild(doc, node, "EOM", strEom_);
        XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
        XMLUtils::addChild(doc, node, "SettlementDays", strSettlementDays_);
    }

    return node;
}

FutureConvention::FutureConvention(const string& id, const string& index)
    : FutureConvention(id, index, QuantLib::RateAveraging::Type::Compound, DateGenerationRule::IMM) {}

FutureConvention::FutureConvention(const string& id, const string& index,
                                   const QuantLib::RateAveraging::Type overnightIndexFutureNettingType,
                                   const DateGenerationRule dateGenerationRule)
    : Convention(id, Type::Future), strIndex_(index),
      overnightIndexFutureNettingType_(overnightIndexFutureNettingType) {
    parseIborIndex(strIndex_);
}

void FutureConvention::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Future");
    type_ = Type::Future;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    parseIborIndex(strIndex_);
    string nettingTypeStr = XMLUtils::getChildValue(node, "OvernightIndexFutureNettingType", false);
    overnightIndexFutureNettingType_ =
        nettingTypeStr.empty() ? RateAveraging::Type::Compound : parseOvernightIndexFutureNettingType(nettingTypeStr);
    string dateGenerationStr = XMLUtils::getChildValue(node, "DateGenerationRule", false);
    dateGenerationRule_ =
        dateGenerationStr.empty() ? DateGenerationRule::IMM : parseFutureDateGenerationRule(dateGenerationStr);
}

XMLNode* FutureConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("Future");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "OvernightIndexFutureNettingType", ore::data::to_string(overnightIndexFutureNettingType_));
    XMLUtils::addChild(doc, node, "DateGenerationRule", ore::data::to_string(dateGenerationRule_));
    return node;
}

QuantLib::ext::shared_ptr<IborIndex> FutureConvention::index() const { return parseIborIndex(strIndex_); }

FraConvention::FraConvention(const string& id, const string& index) : Convention(id, Type::FRA), strIndex_(index) {
    parseIborIndex(strIndex_);
}

void FraConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "FRA");
    type_ = Type::FRA;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    parseIborIndex(strIndex_);
}

XMLNode* FraConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("FRA");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);

    return node;
}

QuantLib::ext::shared_ptr<IborIndex> FraConvention::index() const { return parseIborIndex(strIndex_); }

OisConvention::OisConvention(const string& id, const string& spotLag, const string& index,
                             const string& fixedDayCounter, const string& fixedCalendar, const string& paymentLag,
                             const string& eom, const string& fixedFrequency, const string& fixedConvention,
                             const string& fixedPaymentConvention, const string& rule, const string& paymentCal,
                             const string& rateCutoff)
    : Convention(id, Type::OIS), strSpotLag_(spotLag), strIndex_(index), strFixedDayCounter_(fixedDayCounter),
      strFixedCalendar_(fixedCalendar), strPaymentLag_(paymentLag), strEom_(eom), strFixedFrequency_(fixedFrequency),
      strFixedConvention_(fixedConvention), strFixedPaymentConvention_(fixedPaymentConvention), strRule_(rule),
      strPaymentCal_(paymentCal), strRateCutoff_(rateCutoff) {
    build();
}

void OisConvention::build() {
    auto tmpIndex = parseIborIndex(strIndex_);
    spotLag_ = lexical_cast<Natural>(strSpotLag_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    fixedCalendar_ = strFixedCalendar_.empty() ? tmpIndex->fixingCalendar() : parseCalendar(strFixedCalendar_);
    paymentLag_ = strPaymentLag_.empty() ? 0 : lexical_cast<Natural>(strPaymentLag_);
    eom_ = strEom_.empty() ? false : parseBool(strEom_);
    fixedFrequency_ = strFixedFrequency_.empty() ? Annual : parseFrequency(strFixedFrequency_);
    fixedConvention_ = strFixedConvention_.empty() ? Following : parseBusinessDayConvention(strFixedConvention_);
    fixedPaymentConvention_ =
        strFixedPaymentConvention_.empty() ? Following : parseBusinessDayConvention(strFixedPaymentConvention_);
    rule_ = strRule_.empty() ? DateGeneration::Backward : parseDateGenerationRule(strRule_);
    paymentCal_ = strPaymentCal_.empty() ? Calendar() : parseCalendar(strPaymentCal_);
    rateCutoff_ = strRateCutoff_.empty() ? 0 : lexical_cast<Natural>(strRateCutoff_);

}

void OisConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "OIS");
    type_ = Type::OIS;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSpotLag_ = XMLUtils::getChildValue(node, "SpotLag", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);
    // bwd compatibility
    strFixedCalendar_ = XMLUtils::getChildValue(node, "FixedCalendar", false);
    strPaymentLag_ = XMLUtils::getChildValue(node, "PaymentLag", false);
    strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    strFixedFrequency_ = XMLUtils::getChildValue(node, "FixedFrequency", false);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", false);
    strFixedPaymentConvention_ = XMLUtils::getChildValue(node, "FixedPaymentConvention", false);
    strRule_ = XMLUtils::getChildValue(node, "Rule", false);
    strPaymentCal_ = XMLUtils::getChildValue(node, "PaymentCalendar", false);
    strRateCutoff_ = XMLUtils::getChildValue(node, "RateCutoff", false);

    build();
}

XMLNode* OisConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("OIS");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    if(!strFixedCalendar_.empty())
        XMLUtils::addChild(doc, node, "FixedCalendar", strFixedCalendar_);
    if (!strPaymentLag_.empty())
        XMLUtils::addChild(doc, node, "PaymentLag", strPaymentLag_);
    if (!strEom_.empty())
        XMLUtils::addChild(doc, node, "EOM", strEom_);
    if (!strFixedFrequency_.empty())
        XMLUtils::addChild(doc, node, "FixedFrequency", strFixedFrequency_);
    if (!strFixedConvention_.empty())
        XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    if (!strFixedPaymentConvention_.empty())
        XMLUtils::addChild(doc, node, "FixedPaymentConvention", strFixedPaymentConvention_);
    if (!strRule_.empty())
        XMLUtils::addChild(doc, node, "Rule", strRule_);
    if (!strPaymentCal_.empty())
        XMLUtils::addChild(doc, node, "PaymentCalendar", strPaymentCal_);
    if (!strRateCutoff_.empty())
        XMLUtils::addChild(doc, node, "RateCutoff", strRateCutoff_);

    return node;
}

QuantLib::ext::shared_ptr<OvernightIndex> OisConvention::index() const {
    auto tmp = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(strIndex_));
    QL_REQUIRE(tmp, "The index string '" << strIndex_ << "' does not represent an overnight index.");
    return tmp;
}

IborIndexConvention::IborIndexConvention(const string& id, const string& fixingCalendar, const string& dayCounter,
                                         const Size settlementDays, const string& businessDayConvention,
                                         const bool endOfMonth)
    : Convention(id, Type::IborIndex), localId_(id), strFixingCalendar_(fixingCalendar), strDayCounter_(dayCounter),
      settlementDays_(settlementDays), strBusinessDayConvention_(businessDayConvention), endOfMonth_(endOfMonth) {
    build();
}

void IborIndexConvention::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "IborIndex");
    type_ = Type::IborIndex;
    localId_ = XMLUtils::getChildValue(node, "Id", true);
    strFixingCalendar_ = XMLUtils::getChildValue(node, "FixingCalendar", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    settlementDays_ = XMLUtils::getChildValueAsInt(node, "SettlementDays", true);
    strBusinessDayConvention_ = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
    endOfMonth_ = XMLUtils::getChildValueAsBool(node, "EndOfMonth", true);
    build();
}

XMLNode* IborIndexConvention::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("IborIndex");
    XMLUtils::addChild(doc, node, "Id", localId_);
    XMLUtils::addChild(doc, node, "FixingCalendar", strFixingCalendar_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "SettlementDays", static_cast<int>(settlementDays_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", strBusinessDayConvention_);
    XMLUtils::addChild(doc, node, "EndOfMonth", endOfMonth_);
    return node;
}

void IborIndexConvention::build() {
    // just a check really that the id is consistent with the ibor index name rules
    vector<string> tokens;
    split(tokens, localId_, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2 || tokens.size() == 3,
               "Two or three tokens required in IborIndexConvention " << localId_ << ": CCY-INDEX or CCY-INDEX-TERM");

    // set the Id - this converts the local id term from "7D" to "1W", "28D" to "1M" etc, so if can be picked
    // up by searches
    id_ = (tokens.size() == 3) ? (tokens[0] + "-" + tokens[1] + "-" + to_string(parsePeriod(tokens[2]))) : localId_;
}

OvernightIndexConvention::OvernightIndexConvention(const string& id, const string& fixingCalendar,
                                                   const string& dayCounter, const Size settlementDays)
    : Convention(id, Type::OvernightIndex), strFixingCalendar_(fixingCalendar), strDayCounter_(dayCounter),
      settlementDays_(settlementDays) {
    build();
}

void OvernightIndexConvention::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "OvernightIndex");
    type_ = Type::OvernightIndex;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strFixingCalendar_ = XMLUtils::getChildValue(node, "FixingCalendar", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    settlementDays_ = XMLUtils::getChildValueAsInt(node, "SettlementDays", true);
    build();
}

XMLNode* OvernightIndexConvention::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("OvernightIndex");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "FixingCalendar", strFixingCalendar_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "SettlementDays", static_cast<int>(settlementDays_));
    return node;
}

void OvernightIndexConvention::build() {
    // just a check really that the id is consistent with the ibor index name rules
    vector<string> tokens;
    split(tokens, id_, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2, "Two tokens required in OvernightIndexConvention " << id_ << ": CCY-INDEX");
}

SwapIndexConvention::SwapIndexConvention(const string& id, const string& conventions, const string& fixingCalendar)
    : Convention(id, Type::SwapIndex), strConventions_(conventions), fixingCalendar_(fixingCalendar) {}

void SwapIndexConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "SwapIndex");
    type_ = Type::SwapIndex;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strConventions_ = XMLUtils::getChildValue(node, "Conventions", true);
    fixingCalendar_ = XMLUtils::getChildValue(node, "FixingCalendar", false);
}

XMLNode* SwapIndexConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("SwapIndex");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Conventions", strConventions_);
    XMLUtils::addChild(doc, node, "FixingCalendar", fixingCalendar_);

    return node;
}

IRSwapConvention::IRSwapConvention(const string& id, const string& fixedCalendar, const string& fixedFrequency,
                                   const string& fixedConvention, const string& fixedDayCounter, const string& index,
                                   bool hasSubPeriod, const string& floatFrequency, const string& subPeriodsCouponType)
    : Convention(id, Type::Swap), hasSubPeriod_(hasSubPeriod), strFixedCalendar_(fixedCalendar),
      strFixedFrequency_(fixedFrequency), strFixedConvention_(fixedConvention), strFixedDayCounter_(fixedDayCounter),
      strIndex_(index), strFloatFrequency_(floatFrequency), strSubPeriodsCouponType_(subPeriodsCouponType) {
    build();
}

void IRSwapConvention::build() {
    fixedFrequency_ = parseFrequency(strFixedFrequency_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    auto ind = parseIborIndex(strIndex_);

    if (strFixedCalendar_.empty())
        fixedCalendar_ = ind->fixingCalendar();
    else
        fixedCalendar_ = parseCalendar(strFixedCalendar_);

    if (strFixedConvention_.empty())
        fixedConvention_ = ind->businessDayConvention();
    else
        fixedConvention_ = parseBusinessDayConvention(strFixedConvention_);

    if (hasSubPeriod_) {
        floatFrequency_ = parseFrequency(strFloatFrequency_);
        subPeriodsCouponType_ = parseSubPeriodsCouponType(strSubPeriodsCouponType_);
    } else {
        floatFrequency_ = NoFrequency;
        subPeriodsCouponType_ = SubPeriodsCoupon1::Compounding;
    }
}

void IRSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Swap");
    type_ = Type::Swap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strFixedFrequency_ = XMLUtils::getChildValue(node, "FixedFrequency", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);

    // optional
    strFixedCalendar_ = XMLUtils::getChildValue(node, "FixedCalendar", false);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", false);
    strFloatFrequency_ = XMLUtils::getChildValue(node, "FloatFrequency", false);
    strSubPeriodsCouponType_ = XMLUtils::getChildValue(node, "SubPeriodsCouponType", false);
    hasSubPeriod_ = (strFloatFrequency_ != "");

    build();
}

XMLNode* IRSwapConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("Swap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "FixedCalendar", strFixedCalendar_);
    XMLUtils::addChild(doc, node, "FixedFrequency", strFixedFrequency_);
    XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    if (hasSubPeriod_) {
        XMLUtils::addChild(doc, node, "FloatFrequency", strFloatFrequency_);
        XMLUtils::addChild(doc, node, "SubPeriodsCouponType", strSubPeriodsCouponType_);
    }

    return node;
}

QuantLib::ext::shared_ptr<IborIndex> IRSwapConvention::index() const { return parseIborIndex(strIndex_); }

AverageOisConvention::AverageOisConvention(const string& id, const string& spotLag, const string& fixedTenor,
                                           const string& fixedDayCounter, const string& fixedCalendar,
                                           const string& fixedConvention, const string& fixedPaymentConvention,
                                           const string& fixedFrequency, const string& index, const string& onTenor,
                                           const string& rateCutoff)
    : Convention(id, Type::AverageOIS), strSpotLag_(spotLag), strFixedTenor_(fixedTenor),
      strFixedDayCounter_(fixedDayCounter), strFixedCalendar_(fixedCalendar), strFixedConvention_(fixedConvention),
      strFixedPaymentConvention_(fixedPaymentConvention), strIndex_(index), strOnTenor_(onTenor),
      strRateCutoff_(rateCutoff) {
    build();
}

void AverageOisConvention::build() {
    parseIborIndex(strIndex_);
    spotLag_ = lexical_cast<Natural>(strSpotLag_);
    fixedTenor_ = parsePeriod(strFixedTenor_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    fixedCalendar_ = parseCalendar(strFixedCalendar_);
    fixedConvention_ = parseBusinessDayConvention(strFixedConvention_);
    fixedPaymentConvention_ = parseBusinessDayConvention(strFixedPaymentConvention_);
    fixedFrequency_ = strFixedFrequency_.empty() ? Annual : parseFrequency(strFixedFrequency_);
    onTenor_ = parsePeriod(strOnTenor_);
    rateCutoff_ = lexical_cast<Natural>(strRateCutoff_);
}

void AverageOisConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "AverageOIS");
    type_ = Type::AverageOIS;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSpotLag_ = XMLUtils::getChildValue(node, "SpotLag", true);
    strFixedTenor_ = XMLUtils::getChildValue(node, "FixedTenor", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);
    strFixedCalendar_ = XMLUtils::getChildValue(node, "FixedCalendar", true);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", true);
    strFixedPaymentConvention_ = XMLUtils::getChildValue(node, "FixedPaymentConvention", true);
    strFixedFrequency_ = XMLUtils::getChildValue(node, "FixedFrequency", false);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strOnTenor_ = XMLUtils::getChildValue(node, "OnTenor", true);
    strRateCutoff_ = XMLUtils::getChildValue(node, "RateCutoff", true);

    build();
}

XMLNode* AverageOisConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("AverageOIS");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
    XMLUtils::addChild(doc, node, "FixedTenor", strFixedTenor_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    XMLUtils::addChild(doc, node, "FixedCalendar", strFixedCalendar_);
    XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    XMLUtils::addChild(doc, node, "FixedPaymentConvention", strFixedPaymentConvention_);
    if (!strFixedFrequency_.empty())
        XMLUtils::addChild(doc, node, "FixedFrequency", strFixedFrequency_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "OnTenor", strOnTenor_);
    XMLUtils::addChild(doc, node, "RateCutoff", strRateCutoff_);

    return node;
}

QuantLib::ext::shared_ptr<OvernightIndex> AverageOisConvention::index() const {
    auto tmp = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(strIndex_));
    QL_REQUIRE(tmp, "The index string '" << strIndex_ << "' does not represent an overnight index.");
    return tmp;
}

TenorBasisSwapConvention::TenorBasisSwapConvention(const string& id, const string& payIndex, const string& receiveIndex,
                                                   const string& receiveFrequency, const string& payFrequency,
                                                   const string& spreadOnRec, const string& includeSpread, 
                                                   const string& subPeriodsCouponType)
    : Convention(id, Type::TenorBasisSwap), strPayIndex_(payIndex), strReceiveIndex_(receiveIndex),
      strReceiveFrequency_(receiveFrequency), strPayFrequency_(payFrequency), strSpreadOnRec_(spreadOnRec),
      strIncludeSpread_(includeSpread), strSubPeriodsCouponType_(subPeriodsCouponType) {
    build();
}

void TenorBasisSwapConvention::build() {
    parseIborIndex(strPayIndex_);
    parseIborIndex(strReceiveIndex_);

    QuantLib::ext::shared_ptr<OvernightIndex> payON = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(payIndex());
    QuantLib::ext::shared_ptr<OvernightIndex> recON = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(receiveIndex());

    if (strReceiveFrequency_.empty()) {
        if (recON) {
            receiveFrequency_ = 1 * Years;
            WLOG("receiveFrequency_ empty and overnight, set to 1 Year");
        } else {
            receiveFrequency_ = receiveIndex()->tenor();
            WLOG("receiveFrequency_ empty set to index tenor.");
        }
    } else
        receiveFrequency_ = parsePeriod(strReceiveFrequency_);

    if (strPayFrequency_.empty()) {
        if (payON) {
            payFrequency_ = 1 * Years;
            WLOG("payFrequency_ empty and overnight, set to 1 Year");
        } else {
            payFrequency_ = payIndex()->tenor();
            WLOG("payFrequency_ empty set to index tenor.");
        }
    } else
        payFrequency_ = parsePeriod(strPayFrequency_);

    spreadOnRec_ = strSpreadOnRec_.empty() ? true : parseBool(strSpreadOnRec_);
    includeSpread_ = strIncludeSpread_.empty() ? false : parseBool(strIncludeSpread_);
    if (includeSpread_ && (payON || recON))
        QL_FAIL("IncludeSpread must be false for overnight index legs.");

    subPeriodsCouponType_ = strSubPeriodsCouponType_.empty() ? SubPeriodsCoupon1::Compounding
                                                             : parseSubPeriodsCouponType(strSubPeriodsCouponType_);
}

void TenorBasisSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "TenorBasisSwap");
    type_ = Type::TenorBasisSwap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strPayIndex_ = XMLUtils::getChildValue(node, "PayIndex", false);
    strReceiveIndex_ = XMLUtils::getChildValue(node, "ReceiveIndex", false);
    strReceiveFrequency_ = XMLUtils::getChildValue(node, "ReceiveFrequency", false);
    strPayFrequency_ = XMLUtils::getChildValue(node, "PayFrequency", false);
    strSpreadOnRec_ = XMLUtils::getChildValue(node, "SpreadOnRec", false);
    strIncludeSpread_ = XMLUtils::getChildValue(node, "IncludeSpread", false);
    strSubPeriodsCouponType_ = XMLUtils::getChildValue(node, "SubPeriodsCouponType", false);

    // handle deprecated fields...
    if (strPayIndex_.empty()) {
        XMLNode* longIndex = XMLUtils::getChildNode(node, "LongIndex");
        if (longIndex) {
            ALOG("TenorBasisSwapConvention: LongIndex is deprecated, fill empty PayIndex");
            strPayIndex_ = XMLUtils::getNodeValue(longIndex);
        } else
            QL_FAIL("TenorBasisSwapConvention : PayIndex field missing.");
    }

    if (strReceiveIndex_.empty()) {
        XMLNode* shortIndex = XMLUtils::getChildNode(node, "ShortIndex");
        if (shortIndex) {
            ALOG("TenorBasisSwapConvention: ShortIndex is deprecated, fill empty ReceiveIndex");
            strReceiveIndex_ = XMLUtils::getNodeValue(shortIndex);
        } else
            QL_FAIL("TenorBasisSwapConvention : ReceiveIndex field missing.");
    }

    XMLNode* longPayTenor = XMLUtils::getChildNode(node, "LongPayTenor");
    if (longPayTenor) {
        ALOG("TenorBasisSwapConvention: LongPayTenor is deprecated, fill empty PayFrequency");
        if (strPayFrequency_.empty())
            strPayFrequency_ = XMLUtils::getNodeValue(longPayTenor);
    }

    XMLNode* shortPayTenor = XMLUtils::getChildNode(node, "ShortPayTenor");
    if (shortPayTenor) {
        ALOG("TenorBasisSwapConvention: ShortPayTenor is deprecated, fill empty ReceiveFrequency");
        if (strReceiveFrequency_.empty())
            strReceiveFrequency_ = XMLUtils::getNodeValue(shortPayTenor);
    }

    XMLNode* spreadOnShort = XMLUtils::getChildNode(node, "SpreadOnShort");
    if (spreadOnShort) {
        ALOG("TenorBasisSwapConvention: SpreadOnShort is deprecated, fill empty SpreadOnRec");
        if (strSpreadOnRec_.empty())
            strSpreadOnRec_ = XMLUtils::getNodeValue(spreadOnShort);
    }

    build();
}

XMLNode* TenorBasisSwapConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("TenorBasisSwap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "PayIndex", strPayIndex_);
    XMLUtils::addChild(doc, node, "ReceiveIndex", strReceiveIndex_);
    if (!strReceiveFrequency_.empty())
        XMLUtils::addChild(doc, node, "ReceiveFrequency", strReceiveFrequency_);
    if (!strPayFrequency_.empty())
        XMLUtils::addChild(doc, node, "PayFrequency", strPayFrequency_);
    if (!strSpreadOnRec_.empty())
        XMLUtils::addChild(doc, node, "SpreadOnRec", strSpreadOnRec_);
    if (!strIncludeSpread_.empty())
        XMLUtils::addChild(doc, node, "IncludeSpread", strIncludeSpread_);
    if (!strSubPeriodsCouponType_.empty())
        XMLUtils::addChild(doc, node, "SubPeriodsCouponType", strSubPeriodsCouponType_);
    return node;
}

QuantLib::ext::shared_ptr<IborIndex> TenorBasisSwapConvention::payIndex() const { return parseIborIndex(strPayIndex_); }
QuantLib::ext::shared_ptr<IborIndex> TenorBasisSwapConvention::receiveIndex() const { return parseIborIndex(strReceiveIndex_); }

TenorBasisTwoSwapConvention::TenorBasisTwoSwapConvention(
    const string& id, const string& calendar, const string& longFixedFrequency, const string& longFixedConvention,
    const string& longFixedDayCounter, const string& longIndex, const string& shortFixedFrequency,
    const string& shortFixedConvention, const string& shortFixedDayCounter, const string& shortIndex,
    const string& longMinusShort)
    : Convention(id, Type::TenorBasisTwoSwap), strCalendar_(calendar), strLongFixedFrequency_(longFixedFrequency),
      strLongFixedConvention_(longFixedConvention), strLongFixedDayCounter_(longFixedDayCounter),
      strLongIndex_(longIndex), strShortFixedFrequency_(shortFixedFrequency),
      strShortFixedConvention_(shortFixedConvention), strShortFixedDayCounter_(shortFixedDayCounter),
      strShortIndex_(shortIndex), strLongMinusShort_(longMinusShort) {
    build();
}

void TenorBasisTwoSwapConvention::build() {
    calendar_ = parseCalendar(strCalendar_);
    longFixedFrequency_ = parseFrequency(strLongFixedFrequency_);
    longFixedConvention_ = parseBusinessDayConvention(strLongFixedConvention_);
    longFixedDayCounter_ = parseDayCounter(strLongFixedDayCounter_);
    parseIborIndex(strLongIndex_);
    shortFixedFrequency_ = parseFrequency(strShortFixedFrequency_);
    shortFixedConvention_ = parseBusinessDayConvention(strShortFixedConvention_);
    shortFixedDayCounter_ = parseDayCounter(strShortFixedDayCounter_);
    parseIborIndex(strShortIndex_);
    longMinusShort_ = strLongMinusShort_.empty() ? true : parseBool(strLongMinusShort_);
}

void TenorBasisTwoSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "TenorBasisTwoSwap");
    type_ = Type::TenorBasisTwoSwap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
    strLongFixedFrequency_ = XMLUtils::getChildValue(node, "LongFixedFrequency", true);
    strLongFixedConvention_ = XMLUtils::getChildValue(node, "LongFixedConvention", true);
    strLongFixedDayCounter_ = XMLUtils::getChildValue(node, "LongFixedDayCounter", true);
    strLongIndex_ = XMLUtils::getChildValue(node, "LongIndex", true);
    strShortFixedFrequency_ = XMLUtils::getChildValue(node, "ShortFixedFrequency", true);
    strShortFixedConvention_ = XMLUtils::getChildValue(node, "ShortFixedConvention", true);
    strShortFixedDayCounter_ = XMLUtils::getChildValue(node, "ShortFixedDayCounter", true);
    strShortIndex_ = XMLUtils::getChildValue(node, "ShortIndex", true);
    strLongMinusShort_ = XMLUtils::getChildValue(node, "LongMinusShort", false);

    build();
}

QuantLib::ext::shared_ptr<IborIndex> TenorBasisTwoSwapConvention::longIndex() const { return parseIborIndex(strLongIndex_); }
QuantLib::ext::shared_ptr<IborIndex> TenorBasisTwoSwapConvention::shortIndex() const { return parseIborIndex(strShortIndex_); }

XMLNode* TenorBasisTwoSwapConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("TenorBasisTwoSwap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
    XMLUtils::addChild(doc, node, "LongFixedFrequency", strLongFixedFrequency_);
    XMLUtils::addChild(doc, node, "LongFixedConvention", strLongFixedConvention_);
    XMLUtils::addChild(doc, node, "LongFixedDayCounter", strLongFixedDayCounter_);
    XMLUtils::addChild(doc, node, "LongIndex", strLongIndex_);
    XMLUtils::addChild(doc, node, "ShortFixedFrequency", strShortFixedFrequency_);
    XMLUtils::addChild(doc, node, "ShortFixedConvention", strShortFixedConvention_);
    XMLUtils::addChild(doc, node, "ShortFixedDayCounter", strShortFixedDayCounter_);
    XMLUtils::addChild(doc, node, "ShortIndex", strShortIndex_);
    XMLUtils::addChild(doc, node, "LongMinusShort", strLongMinusShort_);

    return node;
}

BMABasisSwapConvention::BMABasisSwapConvention(const string& id, const string& longIndex, const string& shortIndex)
    : Convention(id, Type::BMABasisSwap), strLiborIndex_(longIndex), strBmaIndex_(shortIndex) {
    build();
}

void BMABasisSwapConvention::build() {
     parseIborIndex(strLiborIndex_);
}

void BMABasisSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "BMABasisSwap");
    type_ = Type::BMABasisSwap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strLiborIndex_ = XMLUtils::getChildValue(node, "LiborIndex", true);
    strBmaIndex_ = XMLUtils::getChildValue(node, "BMAIndex", true);

    build();
}

XMLNode* BMABasisSwapConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("BMABasisSwap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "LiborIndex", strLiborIndex_);
    XMLUtils::addChild(doc, node, "BMAIndex", strBmaIndex_);

    return node;
}

QuantLib::ext::shared_ptr<QuantExt::BMAIndexWrapper> BMABasisSwapConvention::bmaIndex() const {
    auto tmp = QuantLib::ext::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(parseIborIndex(strBmaIndex_));
    QL_REQUIRE(tmp, "the index string '" << strBmaIndex_ << "' does not represent a BMA / SIFMA index.");
    return tmp;
}

QuantLib::ext::shared_ptr<IborIndex> BMABasisSwapConvention::liborIndex() const { return parseIborIndex(strLiborIndex_); }

FXConvention::FXConvention(const string& id, const string& spotDays, const string& sourceCurrency,
                           const string& targetCurrency, const string& pointsFactor, const string& advanceCalendar,
                           const string& spotRelative, const string& endOfMonth, const string& convention)
    : Convention(id, Type::FX), strSpotDays_(spotDays), strSourceCurrency_(sourceCurrency),
      strTargetCurrency_(targetCurrency), strPointsFactor_(pointsFactor), strAdvanceCalendar_(advanceCalendar),
      strSpotRelative_(spotRelative), strEndOfMonth_(endOfMonth), strConvention_(convention) {
    build();
}

void FXConvention::build() {
    spotDays_ = lexical_cast<Natural>(strSpotDays_);
    sourceCurrency_ = parseCurrency(strSourceCurrency_);
    targetCurrency_ = parseCurrency(strTargetCurrency_);
    pointsFactor_ = parseReal(strPointsFactor_);
    advanceCalendar_ = strAdvanceCalendar_.empty() ? NullCalendar() : parseCalendar(strAdvanceCalendar_);
    spotRelative_ = strSpotRelative_.empty() ? true : parseBool(strSpotRelative_);
    endOfMonth_ = strEndOfMonth_.empty() ? false : parseBool(strEndOfMonth_);
    convention_ = strConvention_.empty() ? Following : parseBusinessDayConvention(strConvention_);
}

void FXConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "FX");
    type_ = Type::FX;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSpotDays_ = XMLUtils::getChildValue(node, "SpotDays", true);
    strSourceCurrency_ = XMLUtils::getChildValue(node, "SourceCurrency", true);
    strTargetCurrency_ = XMLUtils::getChildValue(node, "TargetCurrency", true);
    strPointsFactor_ = XMLUtils::getChildValue(node, "PointsFactor", true);
    strAdvanceCalendar_ = XMLUtils::getChildValue(node, "AdvanceCalendar", false);
    strSpotRelative_ = XMLUtils::getChildValue(node, "SpotRelative", false);
    strEndOfMonth_ = XMLUtils::getChildValue(node, "EOM", false);
    strConvention_ = XMLUtils::getChildValue(node, "Convention", false);

    build();
}

XMLNode* FXConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("FX");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotDays", strSpotDays_);
    XMLUtils::addChild(doc, node, "SourceCurrency", strSourceCurrency_);
    XMLUtils::addChild(doc, node, "TargetCurrency", strTargetCurrency_);
    XMLUtils::addChild(doc, node, "PointsFactor", strPointsFactor_);

    if (!strAdvanceCalendar_.empty())
        XMLUtils::addChild(doc, node, "AdvanceCalendar", strAdvanceCalendar_);
    if (!strSpotRelative_.empty())
        XMLUtils::addChild(doc, node, "SpotRelative", strSpotRelative_);
    if (!strEndOfMonth_.empty())
        XMLUtils::addChild(doc, node, "EOM", strEndOfMonth_);
    if (!strConvention_.empty())
        XMLUtils::addChild(doc, node, "Convention", strConvention_);

    return node;
}

CrossCcyBasisSwapConvention::CrossCcyBasisSwapConvention(
    const string& id, const string& strSettlementDays, const string& strSettlementCalendar,
    const string& strRollConvention, const string& flatIndex, const string& spreadIndex, const string& strEom,
    const string& strIsResettable, const string& strFlatIndexIsResettable, const string& strFlatTenor,
    const string& strSpreadTenor, const string& strPaymentLag, const string& strFlatPaymentLag,
    const string& strIncludeSpread, const string& strLookback, const string& strFixingDays, const string& strRateCutoff,
    const string& strIsAveraged, const string& strFlatIncludeSpread, const string& strFlatLookback,
    const string& strFlatFixingDays, const string& strFlatRateCutoff, const string& strFlatIsAveraged,
    const Conventions* conventions)
    : Convention(id, Type::CrossCcyBasis), strSettlementDays_(strSettlementDays),
      strSettlementCalendar_(strSettlementCalendar), strRollConvention_(strRollConvention), strFlatIndex_(flatIndex),
      strSpreadIndex_(spreadIndex), strEom_(strEom), strIsResettable_(strIsResettable),
      strFlatIndexIsResettable_(strFlatIndexIsResettable), strFlatTenor_(strFlatTenor), strSpreadTenor_(strSpreadTenor),
      strPaymentLag_(strPaymentLag), strFlatPaymentLag_(strFlatPaymentLag), strIncludeSpread_(strIncludeSpread),
      strLookback_(strLookback), strFixingDays_(strFixingDays), strRateCutoff_(strRateCutoff),
      strIsAveraged_(strIsAveraged), strFlatIncludeSpread_(strFlatIncludeSpread), strFlatLookback_(strFlatLookback),
      strFlatFixingDays_(strFlatFixingDays), strFlatRateCutoff_(strFlatRateCutoff),
      strFlatIsAveraged_(strFlatIsAveraged) {
    build();
}

void CrossCcyBasisSwapConvention::build() {
    settlementDays_ = lexical_cast<Natural>(strSettlementDays_);
    settlementCalendar_ = parseCalendar(strSettlementCalendar_);
    rollConvention_ = parseBusinessDayConvention(strRollConvention_);
    parseIborIndex(strFlatIndex_);
    parseIborIndex(strSpreadIndex_);
    eom_ = strEom_.empty() ? false : parseBool(strEom_);
    isResettable_ = strIsResettable_.empty() ? false : parseBool(strIsResettable_);
    flatIndexIsResettable_ = strFlatIndexIsResettable_.empty() ? true : parseBool(strFlatIndexIsResettable_);

    // default to index tenor, except for ON indices, where we default to 3M since the index tenor 1D does not make sense for them

    if (strFlatTenor_.empty()) {
        auto tmp = flatIndex();
        if (QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(tmp))
            flatTenor_ = 3 * Months;
        else
            flatTenor_ = tmp->tenor();
    } else {
        flatTenor_ = parsePeriod(strFlatTenor_);
    }

    if (strSpreadTenor_.empty()) {
        auto tmp = spreadIndex();
        if (QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(tmp))
            spreadTenor_ = 3 * Months;
        else
            spreadTenor_ = tmp->tenor();
    } else {
        spreadTenor_ = parsePeriod(strSpreadTenor_);
    }

    paymentLag_ = flatPaymentLag_ = 0;
    if (!strPaymentLag_.empty())
        paymentLag_ = parseInteger(strPaymentLag_);
    if (!strFlatPaymentLag_.empty())
        flatPaymentLag_ = parseInteger(strFlatPaymentLag_);

    // only OIS
    if (!strIncludeSpread_.empty())
        includeSpread_ = parseBool(strIncludeSpread_);
    if (!strLookback_.empty())
        lookback_ = parsePeriod(strLookback_);
    if (!strFixingDays_.empty())
        fixingDays_ = parseInteger(strFixingDays_);
    if (!strRateCutoff_.empty())
        rateCutoff_ = parseInteger(strRateCutoff_);
    if (!strIsAveraged_.empty())
        isAveraged_ = parseBool(strIsAveraged_);
    if (!strFlatIncludeSpread_.empty())
        flatIncludeSpread_ = parseBool(strFlatIncludeSpread_);
    if (!strFlatLookback_.empty())
        flatLookback_ = parsePeriod(strFlatLookback_);
    if (!strFlatFixingDays_.empty())
        flatFixingDays_ = parseInteger(strFlatFixingDays_);
    if (!strFlatRateCutoff_.empty())
        flatRateCutoff_ = parseInteger(strFlatRateCutoff_);
    if (!strFlatIsAveraged_.empty())
        flatIsAveraged_ = parseBool(strFlatIsAveraged_);
}

void CrossCcyBasisSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CrossCurrencyBasis");
    type_ = Type::CrossCcyBasis;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSettlementDays_ = XMLUtils::getChildValue(node, "SettlementDays", true);
    strSettlementCalendar_ = XMLUtils::getChildValue(node, "SettlementCalendar", true);
    strRollConvention_ = XMLUtils::getChildValue(node, "RollConvention", true);
    strFlatIndex_ = XMLUtils::getChildValue(node, "FlatIndex", true);
    strSpreadIndex_ = XMLUtils::getChildValue(node, "SpreadIndex", true);
    strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    strIsResettable_ = XMLUtils::getChildValue(node, "IsResettable", false);
    strFlatIndexIsResettable_ = XMLUtils::getChildValue(node, "FlatIndexIsResettable", false, "true");
    strFlatTenor_ = XMLUtils::getChildValue(node, "FlatTenor", false);
    strSpreadTenor_ = XMLUtils::getChildValue(node, "SpreadTenor", false);

    strPaymentLag_ = XMLUtils::getChildValue(node, "SpreadPaymentLag", false);
    strFlatPaymentLag_ = XMLUtils::getChildValue(node, "FlatPaymentLag", false);

    // OIS specific conventions

    strIncludeSpread_ = XMLUtils::getChildValue(node, "SpreadIncludeSpread", false);
    strLookback_ = XMLUtils::getChildValue(node, "SpreadLookback", false);
    strFixingDays_ = XMLUtils::getChildValue(node, "SpreadFixingDays", false);
    strRateCutoff_ = XMLUtils::getChildValue(node, "SpreadRateCutoff", false);
    strIsAveraged_ = XMLUtils::getChildValue(node, "SpreadIsAveraged", false);

    strFlatIncludeSpread_ = XMLUtils::getChildValue(node, "FlatIncludeSpread", false);
    strFlatLookback_ = XMLUtils::getChildValue(node, "FlatLookback", false);
    strFlatFixingDays_ = XMLUtils::getChildValue(node, "FlatFixingDays", false);
    strFlatRateCutoff_ = XMLUtils::getChildValue(node, "FlatRateCutoff", false);
    strFlatIsAveraged_ = XMLUtils::getChildValue(node, "FlatIsAveraged", false);

    build();
}

XMLNode* CrossCcyBasisSwapConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CrossCurrencyBasis");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SettlementDays", strSettlementDays_);
    XMLUtils::addChild(doc, node, "SettlementCalendar", strSettlementCalendar_);
    XMLUtils::addChild(doc, node, "RollConvention", strRollConvention_);
    XMLUtils::addChild(doc, node, "FlatIndex", strFlatIndex_);
    XMLUtils::addChild(doc, node, "SpreadIndex", strSpreadIndex_);

    if (!strEom_.empty())
        XMLUtils::addChild(doc, node, "EOM", strEom_);
    if (!strIsResettable_.empty())
        XMLUtils::addChild(doc, node, "IsResettable", strIsResettable_);
    if (!strFlatIndexIsResettable_.empty())
        XMLUtils::addChild(doc, node, "FlatIndexIsResettable", strFlatIndexIsResettable_);
    if (!strFlatTenor_.empty())
        XMLUtils::addChild(doc, node, "FlatTenor", strFlatTenor_);
    if (!strSpreadTenor_.empty())
        XMLUtils::addChild(doc, node, "SpreadTenor", strSpreadTenor_);
    if (!strPaymentLag_.empty())
        XMLUtils::addChild(doc, node, "SpreadPaymentLag", strPaymentLag_);
    if (!strFlatPaymentLag_.empty())
        XMLUtils::addChild(doc, node, "FlatPaymentLag", strFlatPaymentLag_);

    if (!strIncludeSpread_.empty())
        XMLUtils::addChild(doc, node, "SpreadIncludeSpread", strIncludeSpread_);
    if (!strLookback_.empty())
        XMLUtils::addChild(doc, node, "SpreadLookback", strLookback_);
    if (!strFixingDays_.empty())
        XMLUtils::addChild(doc, node, "SpreadFixingDays", strFixingDays_);
    if (!strRateCutoff_.empty())
        XMLUtils::addChild(doc, node, "SpreadRateCutoff", strRateCutoff_);
    if (!strIsAveraged_.empty())
        XMLUtils::addChild(doc, node, "SpreadIsAveraged", strIsAveraged_);

    if (!strFlatIncludeSpread_.empty())
        XMLUtils::addChild(doc, node, "FlatIncludeSpread", strFlatIncludeSpread_);
    if (!strFlatLookback_.empty())
        XMLUtils::addChild(doc, node, "FlatLookback", strFlatLookback_);
    if (!strFlatFixingDays_.empty())
        XMLUtils::addChild(doc, node, "FlatFixingDays", strFlatFixingDays_);
    if (!strFlatRateCutoff_.empty())
        XMLUtils::addChild(doc, node, "FlatRateCutoff", strFlatRateCutoff_);
    if (!strFlatIsAveraged_.empty())
        XMLUtils::addChild(doc, node, "FlatIsAveraged", strFlatIsAveraged_);

    return node;
}

QuantLib::ext::shared_ptr<IborIndex> CrossCcyBasisSwapConvention::flatIndex() const { return parseIborIndex(strFlatIndex_); }
QuantLib::ext::shared_ptr<IborIndex> CrossCcyBasisSwapConvention::spreadIndex() const {
    return parseIborIndex(strSpreadIndex_);
}

CrossCcyFixFloatSwapConvention::CrossCcyFixFloatSwapConvention(
    const string& id, const string& settlementDays, const string& settlementCalendar,
    const string& settlementConvention, const string& fixedCurrency, const string& fixedFrequency,
    const string& fixedConvention, const string& fixedDayCounter, const string& index, const string& eom,
    const std::string& strIsResettable, const std::string& strFloatIndexIsResettable)
    : Convention(id, Type::CrossCcyFixFloat), strSettlementDays_(settlementDays),
      strSettlementCalendar_(settlementCalendar), strSettlementConvention_(settlementConvention),
      strFixedCurrency_(fixedCurrency), strFixedFrequency_(fixedFrequency), strFixedConvention_(fixedConvention),
      strFixedDayCounter_(fixedDayCounter), strIndex_(index), strEom_(eom),
      strIsResettable_(strIsResettable), strFloatIndexIsResettable_(strFloatIndexIsResettable){

    build();
}

void CrossCcyFixFloatSwapConvention::build() {
    settlementDays_ = lexical_cast<Natural>(strSettlementDays_);
    settlementCalendar_ = parseCalendar(strSettlementCalendar_);
    settlementConvention_ = parseBusinessDayConvention(strSettlementConvention_);
    fixedCurrency_ = parseCurrency(strFixedCurrency_);
    fixedFrequency_ = parseFrequency(strFixedFrequency_);
    fixedConvention_ = parseBusinessDayConvention(strFixedConvention_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    parseIborIndex(strIndex_);
    eom_ = strEom_.empty() ? false : parseBool(strEom_);
    isResettable_ = strIsResettable_.empty() ? false : parseBool(strIsResettable_);
    floatIndexIsResettable_ = strFloatIndexIsResettable_.empty() ? true : parseBool(strFloatIndexIsResettable_);
}

void CrossCcyFixFloatSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CrossCurrencyFixFloat");
    type_ = Type::CrossCcyFixFloat;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSettlementDays_ = XMLUtils::getChildValue(node, "SettlementDays", true);
    strSettlementCalendar_ = XMLUtils::getChildValue(node, "SettlementCalendar", true);
    strSettlementConvention_ = XMLUtils::getChildValue(node, "SettlementConvention", true);
    strFixedCurrency_ = XMLUtils::getChildValue(node, "FixedCurrency", true);
    strFixedFrequency_ = XMLUtils::getChildValue(node, "FixedFrequency", true);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);

    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    strIsResettable_ = XMLUtils::getChildValue(node, "IsResettable", false);
    strFloatIndexIsResettable_ = XMLUtils::getChildValue(node, "FloatIndexIsResettable", false);

    build();
}

XMLNode* CrossCcyFixFloatSwapConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CrossCurrencyFixFloat");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SettlementDays", strSettlementDays_);
    XMLUtils::addChild(doc, node, "SettlementCalendar", strSettlementCalendar_);
    XMLUtils::addChild(doc, node, "SettlementConvention", strSettlementConvention_);
    XMLUtils::addChild(doc, node, "FixedCurrency", strFixedCurrency_);
    XMLUtils::addChild(doc, node, "FixedFrequency", strFixedFrequency_);
    XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);

    if (!strEom_.empty())
        XMLUtils::addChild(doc, node, "EOM", strEom_);
    if (!strIsResettable_.empty())
        XMLUtils::addChild(doc, node, "IsResettable", strIsResettable_);
    if (!strFloatIndexIsResettable_.empty())
        XMLUtils::addChild(doc, node, "FloatIndexIsResettable", strFloatIndexIsResettable_);

    return node;
}

QuantLib::ext::shared_ptr<QuantLib::IborIndex> CrossCcyFixFloatSwapConvention::index() const {
    return parseIborIndex(strIndex_);
}

CdsConvention::CdsConvention() : settlementDays_(0), frequency_(Quarterly), paymentConvention_(Following),
    rule_(DateGeneration::CDS2015), settlesAccrual_(true), paysAtDefaultTime_(true), upfrontSettlementDays_(3) {}

CdsConvention::CdsConvention(const string& id, const string& strSettlementDays, const string& strCalendar,
                             const string& strFrequency, const string& strPaymentConvention, const string& strRule,
                             const string& strDayCounter, const string& strSettlesAccrual,
                             const string& strPaysAtDefaultTime, const string& strUpfrontSettlementDays,
                             const string& lastPeriodDayCounter)
    : Convention(id, Type::CDS), strSettlementDays_(strSettlementDays), strCalendar_(strCalendar),
      strFrequency_(strFrequency), strPaymentConvention_(strPaymentConvention), strRule_(strRule),
      strDayCounter_(strDayCounter), strSettlesAccrual_(strSettlesAccrual),
      strPaysAtDefaultTime_(strPaysAtDefaultTime), strUpfrontSettlementDays_(strUpfrontSettlementDays),
      strLastPeriodDayCounter_(lastPeriodDayCounter) {
    build();
}

void CdsConvention::build() {
    settlementDays_ = lexical_cast<Natural>(strSettlementDays_);
    calendar_ = parseCalendar(strCalendar_);
    frequency_ = parseFrequency(strFrequency_);
    paymentConvention_ = parseBusinessDayConvention(strPaymentConvention_);
    rule_ = parseDateGenerationRule(strRule_);
    dayCounter_ = parseDayCounter(strDayCounter_);
    settlesAccrual_ = parseBool(strSettlesAccrual_);
    paysAtDefaultTime_ = parseBool(strPaysAtDefaultTime_);

    upfrontSettlementDays_ = 3;
    if (!strUpfrontSettlementDays_.empty())
        upfrontSettlementDays_ = lexical_cast<Natural>(strUpfrontSettlementDays_);

    lastPeriodDayCounter_ = DayCounter();
    if (!strLastPeriodDayCounter_.empty())
        lastPeriodDayCounter_ = parseDayCounter(strLastPeriodDayCounter_);
}

void CdsConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CDS");
    type_ = Type::CDS;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSettlementDays_ = XMLUtils::getChildValue(node, "SettlementDays", true);
    strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
    strFrequency_ = XMLUtils::getChildValue(node, "Frequency", true);
    strPaymentConvention_ = XMLUtils::getChildValue(node, "PaymentConvention", true);
    strRule_ = XMLUtils::getChildValue(node, "Rule", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strSettlesAccrual_ = XMLUtils::getChildValue(node, "SettlesAccrual", true);
    strPaysAtDefaultTime_ = XMLUtils::getChildValue(node, "PaysAtDefaultTime", true);
    strUpfrontSettlementDays_ = XMLUtils::getChildValue(node, "UpfrontSettlementDays", false);
    strLastPeriodDayCounter_ = XMLUtils::getChildValue(node, "LastPeriodDayCounter", false);

    build();
}

XMLNode* CdsConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CDS");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SettlementDays", strSettlementDays_);
    XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
    XMLUtils::addChild(doc, node, "Frequency", strFrequency_);
    XMLUtils::addChild(doc, node, "PaymentConvention", strPaymentConvention_);
    XMLUtils::addChild(doc, node, "Rule", strRule_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "SettlesAccrual", strSettlesAccrual_);
    XMLUtils::addChild(doc, node, "PaysAtDefaultTime", strPaysAtDefaultTime_);
    if (!strUpfrontSettlementDays_.empty())
        XMLUtils::addChild(doc, node, "UpfrontSettlementDays", strUpfrontSettlementDays_);
    if (!strLastPeriodDayCounter_.empty())
        XMLUtils::addChild(doc, node, "LastPeriodDayCounter", strLastPeriodDayCounter_);
    return node;
}

InflationSwapConvention::InflationSwapConvention()
    : fixConvention_(Following), interpolated_(false), adjustInfObsDates_(false),
      infConvention_(Following), publicationRoll_(PublicationRoll::None) {}

InflationSwapConvention::InflationSwapConvention(const string& id, const string& strFixCalendar,
                                                 const string& strFixConvention, const string& strDayCounter,
                                                 const string& strIndex, const string& strInterpolated,
                                                 const string& strObservationLag, const string& strAdjustInfObsDates,
                                                 const string& strInfCalendar, const string& strInfConvention,
                                                 PublicationRoll publicationRoll,
                                                 const QuantLib::ext::shared_ptr<ScheduleData>& publicationScheduleData)
    : Convention(id, Type::InflationSwap), strFixCalendar_(strFixCalendar), strFixConvention_(strFixConvention),
      strDayCounter_(strDayCounter), strIndex_(strIndex), strInterpolated_(strInterpolated),
      strObservationLag_(strObservationLag), strAdjustInfObsDates_(strAdjustInfObsDates),
      strInfCalendar_(strInfCalendar), strInfConvention_(strInfConvention),
      publicationRoll_(publicationRoll), publicationScheduleData_(publicationScheduleData) {
    build();
}

void InflationSwapConvention::build() {
    interpolated_ = parseBool(strInterpolated_);
    parseZeroInflationIndex(strIndex_, Handle<ZeroInflationTermStructure>());
    fixCalendar_ = parseCalendar(strFixCalendar_);
    fixConvention_ = parseBusinessDayConvention(strFixConvention_);
    dayCounter_ = parseDayCounter(strDayCounter_);
    index_ = parseZeroInflationIndex(strIndex_, Handle<ZeroInflationTermStructure>());
    observationLag_ = parsePeriod(strObservationLag_);
    adjustInfObsDates_ = parseBool(strAdjustInfObsDates_);
    infCalendar_ = parseCalendar(strInfCalendar_);
    infConvention_ = parseBusinessDayConvention(strInfConvention_);
    if (publicationRoll_ != PublicationRoll::None) {
        QL_REQUIRE(publicationScheduleData_, "Publication roll is " << publicationRoll_ << " for " << id() <<
            " so expect non-null publication schedule data.");
        publicationSchedule_ = makeSchedule(*publicationScheduleData_);
    }
}

void InflationSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "InflationSwap");
    type_ = Type::InflationSwap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strFixCalendar_ = XMLUtils::getChildValue(node, "FixCalendar", true);
    strFixConvention_ = XMLUtils::getChildValue(node, "FixConvention", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strInterpolated_ = XMLUtils::getChildValue(node, "Interpolated", true);
    strObservationLag_ = XMLUtils::getChildValue(node, "ObservationLag", true);
    strAdjustInfObsDates_ = XMLUtils::getChildValue(node, "AdjustInflationObservationDates", true);
    strInfCalendar_ = XMLUtils::getChildValue(node, "InflationCalendar", true);
    strInfConvention_ = XMLUtils::getChildValue(node, "InflationConvention", true);

    publicationRoll_ = PublicationRoll::None;
    if (XMLNode* n = XMLUtils::getChildNode(node, "PublicationRoll")) {
        publicationRoll_ = parseInflationSwapPublicationRoll(XMLUtils::getNodeValue(n));
    }

    if (publicationRoll_ != PublicationRoll::None) {
        XMLNode* n = XMLUtils::getChildNode(node, "PublicationSchedule");
        QL_REQUIRE(n, "PublicationRoll is " << publicationRoll_ << " for " << id() <<
            " so expect non-empty PublicationSchedule.");
        publicationScheduleData_ = QuantLib::ext::make_shared<ScheduleData>();
        publicationScheduleData_->fromXML(n);
    }

    build();
}

XMLNode* InflationSwapConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("InflationSwap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "FixCalendar", strFixCalendar_);
    XMLUtils::addChild(doc, node, "FixConvention", strFixConvention_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "Interpolated", strInterpolated_);
    XMLUtils::addChild(doc, node, "ObservationLag", strObservationLag_);
    XMLUtils::addChild(doc, node, "AdjustInflationObservationDates", strAdjustInfObsDates_);
    XMLUtils::addChild(doc, node, "InflationCalendar", strInfCalendar_);
    XMLUtils::addChild(doc, node, "InflationConvention", strInfConvention_);

    if (publicationRoll_ != PublicationRoll::None) {
        XMLUtils::addChild(doc, node, "PublicationRoll", to_string(publicationRoll_));
        QL_REQUIRE(publicationScheduleData_, "PublicationRoll is " << publicationRoll_ << " for "
            << id() << " so expect PublicationSchedule.");

        // Need to change the name from ScheduleData to PublicationSchedule.
        XMLNode* n = publicationScheduleData_->toXML(doc);
        XMLUtils::setNodeName(doc, n, "PublicationSchedule");
        XMLUtils::appendNode(node, n);
    }

    return node;
}

QuantLib::ext::shared_ptr<ZeroInflationIndex> InflationSwapConvention::index() const {
    return parseZeroInflationIndex(strIndex_, Handle<ZeroInflationTermStructure>());
}

SecuritySpreadConvention::SecuritySpreadConvention(const string& id, const string& dayCounter,
                                                   const string& compounding, const string& compoundingFrequency)
    : Convention(id, Type::SecuritySpread), tenorBased_(false), strDayCounter_(dayCounter),
      strCompounding_(compounding), strCompoundingFrequency_(compoundingFrequency) {
    build();
}

SecuritySpreadConvention::SecuritySpreadConvention(const string& id, const string& dayCounter,
                                                   const string& tenorCalendar, const string& compounding,
                                                   const string& compoundingFrequency, const string& spotLag,
                                                   const string& spotCalendar, const string& rollConvention,
                                                   const string& eom)
    : Convention(id, Type::SecuritySpread), tenorBased_(true), strDayCounter_(dayCounter),
      strTenorCalendar_(tenorCalendar), strCompounding_(compounding), strCompoundingFrequency_(compoundingFrequency),
      strSpotLag_(spotLag), strSpotCalendar_(spotCalendar), strRollConvention_(rollConvention), strEom_(eom) {
    build();
}

void SecuritySpreadConvention::build() {
    dayCounter_ = parseDayCounter(strDayCounter_);
    compounding_ = strCompounding_.empty() ? Continuous : parseCompounding(strCompounding_);
    compoundingFrequency_ = strCompoundingFrequency_.empty() ? Annual : parseFrequency(strCompoundingFrequency_);
    if (tenorBased_) {
        tenorCalendar_ = parseCalendar(strTenorCalendar_);
        spotLag_ = strSpotLag_.empty() ? 0 : lexical_cast<Natural>(strSpotLag_);
        spotCalendar_ = strSpotCalendar_.empty() ? NullCalendar() : parseCalendar(strSpotCalendar_);
        rollConvention_ = strRollConvention_.empty() ? Following : parseBusinessDayConvention(strRollConvention_);
        eom_ = strEom_.empty() ? false : parseBool(strEom_);
    }
}

void SecuritySpreadConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "BondSpread");
    type_ = Type::SecuritySpread;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    tenorBased_ = XMLUtils::getChildValueAsBool(node, "TenorBased", true);

    // Get string values from xml
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strCompoundingFrequency_ = XMLUtils::getChildValue(node, "CompoundingFrequency", false);
    strCompounding_ = XMLUtils::getChildValue(node, "Compounding", false);
    if (tenorBased_) {
        strTenorCalendar_ = XMLUtils::getChildValue(node, "TenorCalendar", true);
        strSpotLag_ = XMLUtils::getChildValue(node, "SpotLag", false);
        strSpotCalendar_ = XMLUtils::getChildValue(node, "SpotCalendar", false);
        strRollConvention_ = XMLUtils::getChildValue(node, "RollConvention", false);
        strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    }
    build();
}

XMLNode* SecuritySpreadConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("BondSpread");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "TenorBased", tenorBased_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    if (!strCompoundingFrequency_.empty())
        XMLUtils::addChild(doc, node, "CompoundingFrequency", strCompoundingFrequency_);
    if (!strCompounding_.empty())
        XMLUtils::addChild(doc, node, "Compounding", strCompounding_);
    if (tenorBased_) {
        XMLUtils::addChild(doc, node, "TenorCalendar", strTenorCalendar_);
        if (!strSpotLag_.empty())
            XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
        if (!strSpotCalendar_.empty())
            XMLUtils::addChild(doc, node, "SpotCalendar", strSpotCalendar_);
        if (!strRollConvention_.empty())
            XMLUtils::addChild(doc, node, "RollConvention", strRollConvention_);
        if (!strEom_.empty())
            XMLUtils::addChild(doc, node, "EOM", strEom_);
    }

    return node;
}

CmsSpreadOptionConvention::CmsSpreadOptionConvention(const string& id, const string& strForwardStart,
                                                     const string& strSpotDays, const string& strSwapTenor,
                                                     const string& strFixingDays, const string& strCalendar,
                                                     const string& strDayCounter, const string& strConvention)
    : Convention(id, Type::CMSSpreadOption), strForwardStart_(strForwardStart), strSpotDays_(strSpotDays),
      strSwapTenor_(strSwapTenor), strFixingDays_(strFixingDays), strCalendar_(strCalendar),
      strDayCounter_(strDayCounter), strRollConvention_(strConvention) {
    build();
}

void CmsSpreadOptionConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CmsSpreadOption");
    type_ = Type::CMSSpreadOption;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strForwardStart_ = XMLUtils::getChildValue(node, "ForwardStart", true);
    strSpotDays_ = XMLUtils::getChildValue(node, "SpotDays", true);
    strSwapTenor_ = XMLUtils::getChildValue(node, "SwapTenor", true);
    strFixingDays_ = XMLUtils::getChildValue(node, "FixingDays", true);
    strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strRollConvention_ = XMLUtils::getChildValue(node, "RollConvention", true);

    build();
}

void CmsSpreadOptionConvention::build() {

    forwardStart_ = parsePeriod(strForwardStart_);
    spotDays_ = parsePeriod(strSpotDays_);
    swapTenor_ = parsePeriod(strSwapTenor_);
    fixingDays_ = lexical_cast<Natural>(strFixingDays_);
    calendar_ = parseCalendar(strCalendar_);
    dayCounter_ = parseDayCounter(strDayCounter_);
    rollConvention_ = parseBusinessDayConvention(strRollConvention_);
}

XMLNode* CmsSpreadOptionConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CmsSpreadOption");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "ForwardStart", strForwardStart_);
    XMLUtils::addChild(doc, node, "SpotDays", strSpotDays_);
    XMLUtils::addChild(doc, node, "SwapTenor", strSwapTenor_);
    XMLUtils::addChild(doc, node, "FixingDays", strFixingDays_);
    XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "RollConvention", strRollConvention_);

    return node;
}

CommodityForwardConvention::CommodityForwardConvention(const string& id, const string& spotDays,
                                                       const string& pointsFactor, const string& advanceCalendar,
                                                       const string& spotRelative, BusinessDayConvention bdc,
                                                       bool outright)
    : Convention(id, Type::CommodityForward), bdc_(bdc), outright_(outright), strSpotDays_(spotDays),
      strPointsFactor_(pointsFactor), strAdvanceCalendar_(advanceCalendar), strSpotRelative_(spotRelative) {
    build();
}

void CommodityForwardConvention::build() {
    spotDays_ = strSpotDays_.empty() ? 2 : lexical_cast<Natural>(strSpotDays_);
    pointsFactor_ = strPointsFactor_.empty() ? 1.0 : parseReal(strPointsFactor_);
    advanceCalendar_ = strAdvanceCalendar_.empty() ? NullCalendar() : parseCalendar(strAdvanceCalendar_);
    spotRelative_ = strSpotRelative_.empty() ? true : parseBool(strSpotRelative_);
}

void CommodityForwardConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CommodityForward");
    type_ = Type::CommodityForward;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    strSpotDays_ = XMLUtils::getChildValue(node, "SpotDays", false);
    strPointsFactor_ = XMLUtils::getChildValue(node, "PointsFactor", false);
    strAdvanceCalendar_ = XMLUtils::getChildValue(node, "AdvanceCalendar", false);
    strSpotRelative_ = XMLUtils::getChildValue(node, "SpotRelative", false);

    bdc_ = Following;
    if (XMLNode* n = XMLUtils::getChildNode(node, "BusinessDayConvention")) {
        bdc_ = parseBusinessDayConvention(XMLUtils::getNodeValue(n));
    }

    outright_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "Outright")) {
        outright_ = parseBool(XMLUtils::getNodeValue(n));
    }

    build();
}

XMLNode* CommodityForwardConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CommodityForward");
    XMLUtils::addChild(doc, node, "Id", id_);

    if (!strSpotDays_.empty())
        XMLUtils::addChild(doc, node, "SpotDays", strSpotDays_);
    if (!strPointsFactor_.empty())
        XMLUtils::addChild(doc, node, "PointsFactor", strPointsFactor_);
    if (!strAdvanceCalendar_.empty())
        XMLUtils::addChild(doc, node, "AdvanceCalendar", strAdvanceCalendar_);
    if (!strSpotRelative_.empty())
        XMLUtils::addChild(doc, node, "SpotRelative", strSpotRelative_);


    XMLUtils::addChild(doc, node, "BusinessDayConvention", ore::data::to_string(bdc_));
    XMLUtils::addChild(doc, node, "Outright", outright_);

    return node;
}

CommodityFutureConvention::AveragingData::AveragingData()
    : useBusinessDays_(true), deliveryRollDays_(0), futureMonthOffset_(0),
      dailyExpiryOffset_(Null<Natural>()), period_(CalculationPeriod::ExpiryToExpiry) {}

CommodityFutureConvention::AveragingData::AveragingData(const string& commodityName, const string& period,
    const string& pricingCalendar, bool useBusinessDays, const string& conventionsId,
    Natural deliveryRollDays, Natural futureMonthOffset, Natural dailyExpiryOffset)
    : commodityName_(commodityName), strPeriod_(period), strPricingCalendar_(pricingCalendar),
      useBusinessDays_(useBusinessDays), conventionsId_(conventionsId), deliveryRollDays_(deliveryRollDays),
      futureMonthOffset_(futureMonthOffset), dailyExpiryOffset_(dailyExpiryOffset),
      period_(CalculationPeriod::ExpiryToExpiry) {
    build();
}

const string& CommodityFutureConvention::AveragingData::commodityName() const {
    return commodityName_;
}

CommodityFutureConvention::AveragingData::CalculationPeriod CommodityFutureConvention::AveragingData::period() const {
    return period_;
}

const Calendar& CommodityFutureConvention::AveragingData::pricingCalendar() const {
    return pricingCalendar_;
}

bool CommodityFutureConvention::AveragingData::useBusinessDays() const {
    return useBusinessDays_;
}

const string& CommodityFutureConvention::AveragingData::conventionsId() const {
    return conventionsId_;
}

Natural CommodityFutureConvention::AveragingData::deliveryRollDays() const {
    return deliveryRollDays_;
}

Natural CommodityFutureConvention::AveragingData::futureMonthOffset() const {
    return futureMonthOffset_;
}

Natural CommodityFutureConvention::AveragingData::dailyExpiryOffset() const {
    return dailyExpiryOffset_;
}

bool CommodityFutureConvention::AveragingData::empty() const {
    return commodityName_.empty();
}

void CommodityFutureConvention::AveragingData::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "AveragingData");
    commodityName_ = XMLUtils::getChildValue(node, "CommodityName", true);
    strPeriod_ = XMLUtils::getChildValue(node, "Period", true);
    strPricingCalendar_ = XMLUtils::getChildValue(node, "PricingCalendar", true);
    useBusinessDays_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "UseBusinessDays")) {
        useBusinessDays_ = parseBool(XMLUtils::getNodeValue(n));
    }
    conventionsId_ = XMLUtils::getChildValue(node, "Conventions", false);

    deliveryRollDays_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "DeliveryRollDays")) {
        deliveryRollDays_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    futureMonthOffset_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "FutureMonthOffset")) {
        futureMonthOffset_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    dailyExpiryOffset_ = Null<Natural>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "DailyExpiryOffset")) {
        dailyExpiryOffset_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    build();
}

XMLNode* CommodityFutureConvention::AveragingData::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("AveragingData");
    XMLUtils::addChild(doc, node, "CommodityName", commodityName_);
    XMLUtils::addChild(doc, node, "Period", strPeriod_);
    XMLUtils::addChild(doc, node, "PricingCalendar", strPricingCalendar_);
    XMLUtils::addChild(doc, node, "UseBusinessDays", useBusinessDays_);
    if (!conventionsId_.empty())
        XMLUtils::addChild(doc, node, "Conventions", conventionsId_);
    if (deliveryRollDays_ != 0)
        XMLUtils::addChild(doc, node, "DeliveryRollDays", static_cast<int>(deliveryRollDays_));
    if (futureMonthOffset_ != 0)
        XMLUtils::addChild(doc, node, "FutureMonthOffset", static_cast<int>(futureMonthOffset_));
    if (dailyExpiryOffset_ != Null<Natural>())
        XMLUtils::addChild(doc, node, "DailyExpiryOffset", static_cast<int>(dailyExpiryOffset_));

    return node;
}

void CommodityFutureConvention::AveragingData::build() {
    period_ = parseAveragingDataPeriod(strPeriod_);
    pricingCalendar_ = parseCalendar(strPricingCalendar_);
}

CommodityFutureConvention::OffPeakPowerIndexData::OffPeakPowerIndexData() : offPeakHours_(0.0) {}

CommodityFutureConvention::OffPeakPowerIndexData::OffPeakPowerIndexData(
    const string& offPeakIndex,
    const string& peakIndex,
    const string& offPeakHours,
    const string& peakCalendar)
    : offPeakIndex_(offPeakIndex),
      peakIndex_(peakIndex),
      strOffPeakHours_(offPeakHours),
      strPeakCalendar_(peakCalendar) {
    build();
}

void CommodityFutureConvention::OffPeakPowerIndexData::build() {
    offPeakHours_ = parseReal(strOffPeakHours_);
    peakCalendar_ = parseCalendar(strPeakCalendar_);
}

void CommodityFutureConvention::OffPeakPowerIndexData::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "OffPeakPowerIndexData");
    offPeakIndex_ = XMLUtils::getChildValue(node, "OffPeakIndex", true);
    peakIndex_ = XMLUtils::getChildValue(node, "PeakIndex", true);
    strOffPeakHours_ = XMLUtils::getChildValue(node, "OffPeakHours", true);
    strPeakCalendar_ = XMLUtils::getChildValue(node, "PeakCalendar", true);

    build();
}

XMLNode* CommodityFutureConvention::OffPeakPowerIndexData::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("OffPeakPowerIndexData");
    XMLUtils::addChild(doc, node, "OffPeakIndex", offPeakIndex_);
    XMLUtils::addChild(doc, node, "PeakIndex", peakIndex_);
    XMLUtils::addChild(doc, node, "OffPeakHours", strOffPeakHours_);
    XMLUtils::addChild(doc, node, "PeakCalendar", strPeakCalendar_);

    return node;
}

CommodityFutureConvention::ProhibitedExpiry::ProhibitedExpiry()
    : forFuture_(true), futureBdc_(Preceding), forOption_(true), optionBdc_(Preceding) {}

CommodityFutureConvention::ProhibitedExpiry::ProhibitedExpiry(const Date& expiry,
    bool forFuture,
    BusinessDayConvention futureBdc,
    bool forOption,
    BusinessDayConvention optionBdc)
    : expiry_(expiry), forFuture_(true), futureBdc_(Preceding),
      forOption_(true), optionBdc_(Preceding) {}

void CommodityFutureConvention::ProhibitedExpiry::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Date");
    expiry_ = parseDate(XMLUtils::getNodeValue(node));
    string tmp = XMLUtils::getAttribute(node, "forFuture");
    forFuture_ = tmp.empty() ? true : parseBool(tmp);
    tmp = XMLUtils::getAttribute(node, "convention");
    futureBdc_ = tmp.empty() ? Preceding : parseBusinessDayConvention(tmp);
    tmp = XMLUtils::getAttribute(node, "forOption");
    forOption_ = tmp.empty() ? true : parseBool(tmp);
    tmp = XMLUtils::getAttribute(node, "optionConvention");
    optionBdc_ = tmp.empty() ? Preceding : parseBusinessDayConvention(tmp);
}

XMLNode* CommodityFutureConvention::ProhibitedExpiry::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("Date", to_string(expiry_));
    XMLUtils::addAttribute(doc, node, "forFuture", to_string(forFuture_));
    XMLUtils::addAttribute(doc, node, "convention", to_string(futureBdc_));
    XMLUtils::addAttribute(doc, node, "forOption", to_string(forOption_));
    XMLUtils::addAttribute(doc, node, "optionConvention", to_string(optionBdc_));

    return node;
}

bool operator<(const CommodityFutureConvention::ProhibitedExpiry& lhs,
    const CommodityFutureConvention::ProhibitedExpiry& rhs) {
    return lhs.expiry() < rhs.expiry();
}

CommodityFutureConvention::CommodityFutureConvention()
    : anchorType_(AnchorType::DayOfMonth),
      dayOfMonth_(1),
      nth_(1),
      weekday_(Mon),
      calendarDaysBefore_(0),
      contractFrequency_(Monthly),
      oneContractMonth_(Jan),
      offsetDays_(0),
      bdc_(Following),
      expiryMonthLag_(0),
      adjustBeforeOffset_(false),
      isAveraging_(false),
      optionExpiryMonthLag_(0),
      optionBdc_(Preceding),
      hoursPerDay_(Null<Natural>()),
      optionAnchorType_(OptionAnchorType::BusinessDaysBefore), 
      optionContractFrequency_(Monthly), 
      optionExpiryOffset_(0),
      optionNth_(1), 
      optionWeekday_(Mon),
      optionExpiryDay_(Null<Natural>()), 
      balanceOfTheMonth_(false) {}

CommodityFutureConvention::CommodityFutureConvention(const string& id, const DayOfMonth& dayOfMonth,
                                                     const string& contractFrequency, const string& calendar,
                                                     const string& expiryCalendar, Size expiryMonthLag,
                                                     const string& oneContractMonth, const string& offsetDays,
                                                     const string& bdc, bool adjustBeforeOffset, bool isAveraging,
                                                     const OptionExpiryAnchorDateRule& optionExpiryDateRule,
                                                     const set<ProhibitedExpiry>& prohibitedExpiries,
                                                     Size optionExpiryMonthLag,
                                                     const string& optionBdc,
                                                     const map<Natural, Natural>& futureContinuationMappings,
                                                     const map<Natural, Natural>& optionContinuationMappings,
                                                     const AveragingData& averagingData,
                                                     Natural hoursPerDay,
                                                     const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData, 
                                                     const string& indexName,
                                                     const std::string& optionFrequency)
    : Convention(id, Type::CommodityFuture), anchorType_(AnchorType::DayOfMonth),
      strDayOfMonth_(dayOfMonth.dayOfMonth_), strContractFrequency_(contractFrequency), strCalendar_(calendar),
      strExpiryCalendar_(expiryCalendar), expiryMonthLag_(expiryMonthLag), strOneContractMonth_(oneContractMonth),
      strOffsetDays_(offsetDays), strBdc_(bdc), adjustBeforeOffset_(adjustBeforeOffset), isAveraging_(isAveraging),
      prohibitedExpiries_(prohibitedExpiries),optionExpiryMonthLag_(optionExpiryMonthLag), strOptionBdc_(optionBdc),
      futureContinuationMappings_(futureContinuationMappings), optionContinuationMappings_(optionContinuationMappings),
      averagingData_(averagingData), hoursPerDay_(hoursPerDay), offPeakPowerIndexData_(offPeakPowerIndexData),
      indexName_(indexName), strOptionContractFrequency_(optionFrequency), optionAnchorType_(optionExpiryDateRule.type_), strOptionExpiryOffset_(optionExpiryDateRule.daysBefore_),
      strOptionExpiryDay_(optionExpiryDateRule.expiryDay_), strOptionNth_(optionExpiryDateRule.nth_),
      strOptionWeekday_(optionExpiryDateRule.weekday_), balanceOfTheMonth_(false) {
    build();
}

CommodityFutureConvention::CommodityFutureConvention(const string& id, const string& nth, const string& weekday,
                                                     const string& contractFrequency, const string& calendar,
                                                     const string& expiryCalendar, Size expiryMonthLag,
                                                     const string& oneContractMonth, const string& offsetDays,
                                                     const string& bdc, bool adjustBeforeOffset, bool isAveraging,
                                                     const OptionExpiryAnchorDateRule& optionExpiryDateRule,
                                                     const set<ProhibitedExpiry>& prohibitedExpiries,
                                                     Size optionExpiryMonthLag,
                                                     const string& optionBdc,
                                                     const map<Natural, Natural>& futureContinuationMappings,
                                                     const map<Natural, Natural>& optionContinuationMappings,
                                                     const AveragingData& averagingData,
                                                     Natural hoursPerDay,
                                                     const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData,
                                                     const string& indexName,
                                                     const std::string& optionFrequency)
    : Convention(id, Type::CommodityFuture), anchorType_(AnchorType::NthWeekday), strNth_(nth), strWeekday_(weekday),
      strContractFrequency_(contractFrequency), strCalendar_(calendar), strExpiryCalendar_(expiryCalendar),
      expiryMonthLag_(expiryMonthLag), strOneContractMonth_(oneContractMonth), strOffsetDays_(offsetDays), strBdc_(bdc),
      adjustBeforeOffset_(adjustBeforeOffset), isAveraging_(isAveraging),
      prohibitedExpiries_(prohibitedExpiries), optionExpiryMonthLag_(optionExpiryMonthLag), strOptionBdc_(optionBdc),
      futureContinuationMappings_(futureContinuationMappings), optionContinuationMappings_(optionContinuationMappings),
      averagingData_(averagingData), hoursPerDay_(hoursPerDay), offPeakPowerIndexData_(offPeakPowerIndexData),
      indexName_(indexName), strOptionContractFrequency_(optionFrequency),
      optionAnchorType_(optionExpiryDateRule.type_), strOptionExpiryOffset_(optionExpiryDateRule.daysBefore_),
      strOptionExpiryDay_(optionExpiryDateRule.expiryDay_), strOptionNth_(optionExpiryDateRule.nth_),
      strOptionWeekday_(optionExpiryDateRule.weekday_), balanceOfTheMonth_(false) {
    build();
}

CommodityFutureConvention::CommodityFutureConvention(const string& id, const CalendarDaysBefore& calendarDaysBefore,
                                                     const string& contractFrequency, const string& calendar,
                                                     const string& expiryCalendar, Size expiryMonthLag,
                                                     const string& oneContractMonth, const string& offsetDays,
                                                     const string& bdc, bool adjustBeforeOffset, bool isAveraging,
                                                     const OptionExpiryAnchorDateRule& optionExpiryDateRule,
                                                     const set<ProhibitedExpiry>& prohibitedExpiries,
                                                     Size optionExpiryMonthLag,
                                                     const string& optionBdc,
                                                     const map<Natural, Natural>& futureContinuationMappings,
                                                     const map<Natural, Natural>& optionContinuationMappings,
                                                     const AveragingData& averagingData,
                                                     Natural hoursPerDay,
                                                     const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData,
                                                     const string& indexName,
                                                     const std::string& optionFrequency)
    : Convention(id, Type::CommodityFuture), anchorType_(AnchorType::CalendarDaysBefore),
      strCalendarDaysBefore_(calendarDaysBefore.calendarDaysBefore_), strContractFrequency_(contractFrequency),
      strCalendar_(calendar), strExpiryCalendar_(expiryCalendar), expiryMonthLag_(expiryMonthLag),
      strOneContractMonth_(oneContractMonth), strOffsetDays_(offsetDays), strBdc_(bdc),
      adjustBeforeOffset_(adjustBeforeOffset), isAveraging_(isAveraging), 
      prohibitedExpiries_(prohibitedExpiries), optionExpiryMonthLag_(optionExpiryMonthLag), strOptionBdc_(optionBdc),
      futureContinuationMappings_(futureContinuationMappings),
      optionContinuationMappings_(optionContinuationMappings), averagingData_(averagingData), hoursPerDay_(hoursPerDay), offPeakPowerIndexData_(offPeakPowerIndexData),
      indexName_(indexName), strOptionContractFrequency_(optionFrequency),
      optionAnchorType_(optionExpiryDateRule.type_), strOptionExpiryOffset_(optionExpiryDateRule.daysBefore_), 
      strOptionExpiryDay_(optionExpiryDateRule.expiryDay_), strOptionNth_(optionExpiryDateRule.nth_), 
      strOptionWeekday_(optionExpiryDateRule.weekday_), balanceOfTheMonth_(false) {
    build();
}

CommodityFutureConvention::CommodityFutureConvention(const string& id, const BusinessDaysAfter& businessDaysAfter,
                                                     const string& contractFrequency, const string& calendar,
                                                     const string& expiryCalendar, Size expiryMonthLag,
                                                     const string& oneContractMonth, const string& offsetDays,
                                                     const string& bdc, bool adjustBeforeOffset, bool isAveraging,
                                                     const OptionExpiryAnchorDateRule& optionExpiryDateRule,
                                                     const set<ProhibitedExpiry>& prohibitedExpiries,
                                                     Size optionExpiryMonthLag,
                                                     const string& optionBdc,
                                                     const map<Natural, Natural>& futureContinuationMappings,
                                                     const map<Natural, Natural>& optionContinuationMappings,
                                                     const AveragingData& averagingData,
                                                     Natural hoursPerDay,
                                                     const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData,
                                                     const string& indexName,
                                                     const std::string& optionFrequency)
    : Convention(id, Type::CommodityFuture), anchorType_(AnchorType::BusinessDaysAfter),
      strBusinessDaysAfter_(businessDaysAfter.businessDaysAfter_), strContractFrequency_(contractFrequency),
      strCalendar_(calendar), strExpiryCalendar_(expiryCalendar), expiryMonthLag_(expiryMonthLag),
      strOneContractMonth_(oneContractMonth), strOffsetDays_(offsetDays), strBdc_(bdc),
      adjustBeforeOffset_(adjustBeforeOffset), isAveraging_(isAveraging), 
      prohibitedExpiries_(prohibitedExpiries), optionExpiryMonthLag_(optionExpiryMonthLag), strOptionBdc_(optionBdc),
      futureContinuationMappings_(futureContinuationMappings),
      optionContinuationMappings_(optionContinuationMappings), averagingData_(averagingData), hoursPerDay_(hoursPerDay), offPeakPowerIndexData_(offPeakPowerIndexData),
      indexName_(indexName), strOptionContractFrequency_(optionFrequency),
      optionAnchorType_(optionExpiryDateRule.type_), strOptionExpiryOffset_(optionExpiryDateRule.daysBefore_), 
      strOptionExpiryDay_(optionExpiryDateRule.expiryDay_), strOptionNth_(optionExpiryDateRule.nth_), 
      strOptionWeekday_(optionExpiryDateRule.weekday_), balanceOfTheMonth_(false) {
    build();
}


void CommodityFutureConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CommodityFuture");
    type_ = Type::CommodityFuture;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Parse contract frequency first. If it is Daily, we do not need the AnchorDay node.
    strContractFrequency_ = XMLUtils::getChildValue(node, "ContractFrequency", true);
    strOptionContractFrequency_ = XMLUtils::getChildValue(node, "OptionContractFrequency", false);
    if (strOptionContractFrequency_.empty())
        strOptionContractFrequency_ = strContractFrequency_;

    contractFrequency_ = parseAndValidateFrequency(strContractFrequency_);
    optionContractFrequency_ = parseAndValidateFrequency(strOptionContractFrequency_);
   
    // Variables related to the anchor day in a given month. Not needed if daily.
    if (contractFrequency_ == Weekly) {
        XMLNode* anchorNode = XMLUtils::getChildNode(node, "AnchorDay");
        if (XMLNode* tmp = XMLUtils::getChildNode(anchorNode, "WeeklyDayOfTheWeek")) {
            anchorType_ = AnchorType::WeeklyDayOfTheWeek;
            strWeekday_ = XMLUtils::getNodeValue(tmp);
        } else {
            QL_FAIL("Failed to parse AnchorDay node, the WeeklyDayOfTheWeek node is required for weekly contract expiries.");
        }
    } else if (contractFrequency_ != Daily || optionContractFrequency_ != Daily) {
        XMLNode* anchorNode = XMLUtils::getChildNode(node, "AnchorDay");
        QL_REQUIRE(anchorNode, "Expected an AnchorDay node in the FutureExpiry convention");
        if (XMLNode* nthNode = XMLUtils::getChildNode(anchorNode, "NthWeekday")) {
            anchorType_ = AnchorType::NthWeekday;
            strNth_ = XMLUtils::getChildValue(nthNode, "Nth", true);
            strWeekday_ = XMLUtils::getChildValue(nthNode, "Weekday", true);
        } else if (XMLNode* tmp = XMLUtils::getChildNode(anchorNode, "DayOfMonth")) {
            anchorType_ = AnchorType::DayOfMonth;
            strDayOfMonth_ = XMLUtils::getNodeValue(tmp);
        } else if (XMLNode* tmp = XMLUtils::getChildNode(anchorNode, "CalendarDaysBefore")) {
            anchorType_ = AnchorType::CalendarDaysBefore;
            strCalendarDaysBefore_ = XMLUtils::getNodeValue(tmp);
        } else if (XMLNode* tmp = XMLUtils::getChildNode(anchorNode, "LastWeekday")) {
            anchorType_ = AnchorType::LastWeekday;
            strWeekday_ = XMLUtils::getNodeValue(tmp);
        } else if (XMLNode* tmp = XMLUtils::getChildNode(anchorNode, "BusinessDaysAfter")) {
            anchorType_ = AnchorType::BusinessDaysAfter;
            strBusinessDaysAfter_ = XMLUtils::getNodeValue(tmp);
        } else {
            QL_FAIL("Failed to parse AnchorDay node");
        }
    }

    strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
    strExpiryCalendar_ = XMLUtils::getChildValue(node, "ExpiryCalendar", false);

    expiryMonthLag_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "ExpiryMonthLag")) {
        expiryMonthLag_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    strOneContractMonth_ = XMLUtils::getChildValue(node, "OneContractMonth", false);
    strOffsetDays_ = XMLUtils::getChildValue(node, "OffsetDays", false);
    strBdc_ = XMLUtils::getChildValue(node, "BusinessDayConvention", false);

    adjustBeforeOffset_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "AdjustBeforeOffset")) {
        adjustBeforeOffset_ = parseBool(XMLUtils::getNodeValue(n));
    }

    isAveraging_ = false;
    if (XMLNode* n = XMLUtils::getChildNode(node, "IsAveraging")) {
        isAveraging_ = parseBool(XMLUtils::getNodeValue(n));
    }

    optionExpiryMonthLag_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "OptionExpiryMonthLag")) {
        optionExpiryMonthLag_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    bool foundOptionExpiryRule = false;
    std::string previouslyFoundOptionExpiryRule = "";

    if (XMLNode* n = XMLUtils::getChildNode(node, "OptionExpiryOffset")) {
        QL_REQUIRE(optionContractFrequency_ != Weekly, "OptionExpiryOffset is not allowed for weekly option expiries");
        optionAnchorType_ = OptionAnchorType::BusinessDaysBefore;
        strOptionExpiryOffset_ = XMLUtils::getNodeValue(n);
        foundOptionExpiryRule = true;
        previouslyFoundOptionExpiryRule = "OptionExpiryOffset";
    }
   
    if (XMLNode* n = XMLUtils::getChildNode(node, "OptionExpiryDay")) {
        QL_REQUIRE(!foundOptionExpiryRule, "Expect exactly one option expiry anchor date rule, found "
                                               << previouslyFoundOptionExpiryRule << " and OptionExpiryDay");
        QL_REQUIRE(optionContractFrequency_ != Weekly, "OptionExpiryDay is not allowed for weekly option expiries");
        optionAnchorType_ = OptionAnchorType::DayOfMonth;
        strOptionExpiryDay_ = XMLUtils::getNodeValue(n);
        foundOptionExpiryRule = true;
        previouslyFoundOptionExpiryRule = "OptionExpiryDay";
    }

    if (XMLNode* optionNthNode = XMLUtils::getChildNode(node, "OptionNthWeekday")) {
        QL_REQUIRE(!foundOptionExpiryRule, "Expect exactly one option expiry anchor date rule, found "
                                               << previouslyFoundOptionExpiryRule << " and OptionNthWeekday");
        QL_REQUIRE(optionContractFrequency_ != Weekly,
                   "OptionNthWeekday is not allowed for weekly option expiries");
        optionAnchorType_ = OptionAnchorType::NthWeekday;
        strOptionNth_ = XMLUtils::getChildValue(optionNthNode, "Nth", true);
        strOptionWeekday_ = XMLUtils::getChildValue(optionNthNode, "Weekday", true);
        foundOptionExpiryRule = true;
        previouslyFoundOptionExpiryRule = "OptionNthWeekday";
    }  

    if (XMLNode* n = XMLUtils::getChildNode(node, "OptionExpiryLastWeekdayOfMonth")) {
        QL_REQUIRE(!foundOptionExpiryRule, "Expect exactly one option expiry anchor date rule, found "
                                               << previouslyFoundOptionExpiryRule << " and OptionExpiryLastWeekdayOfMonth");
        QL_REQUIRE(optionContractFrequency_ != Weekly,
                   "OptionExpiryLastWeekdayOfMonth is not allowed for weekly option expiries");
        optionAnchorType_ = OptionAnchorType::LastWeekday;
        strOptionWeekday_ = XMLUtils::getNodeValue(n);
        foundOptionExpiryRule = true;
        previouslyFoundOptionExpiryRule = "OptionExpiryLastWeekdayOfMonth";
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "OptionExpiryWeeklyDayOfTheWeek")) {
        QL_REQUIRE(!foundOptionExpiryRule, "Expect exactly one option expiry anchor date rule, found "
                                               << previouslyFoundOptionExpiryRule
                                               << " and OptionExpiryWeeklyDayOfTheWeek");
        QL_REQUIRE(optionContractFrequency_ == Weekly,
                   "OptionExpiryWeeklyDayOfTheWeek only allowed for weekly option expiries");
        optionAnchorType_ = OptionAnchorType::WeeklyDayOfTheWeek;
        strOptionWeekday_ = XMLUtils::getNodeValue(n);
        foundOptionExpiryRule = true;
        previouslyFoundOptionExpiryRule = "OptionExpiryWeeklyDayOfTheWeek";
    }


    if (!foundOptionExpiryRule && optionContractFrequency_ == Weekly) {
        optionAnchorType_ = OptionAnchorType::WeeklyDayOfTheWeek;
        strOptionWeekday_ = strWeekday_;
    } else if (!foundOptionExpiryRule) {
        optionAnchorType_ = OptionAnchorType::BusinessDaysBefore;
        strOptionExpiryOffset_ = "";
    }



    if (XMLNode* n = XMLUtils::getChildNode(node, "ProhibitedExpiries")) {
        auto datesNode = XMLUtils::getChildNode(n, "Dates");
        QL_REQUIRE(datesNode, "ProhibitedExpiries node must have a Dates node.");
        auto dateNodes = XMLUtils::getChildrenNodes(datesNode, "Date");
        for (const auto& dateNode : dateNodes) {
            ProhibitedExpiry pe;
            pe.fromXML(dateNode);
            if (validateBdc(pe)) {
                // First date is inserted, subsequent duplicates are ignored.
                prohibitedExpiries_.insert(pe);
            }
        }
    }

    if (contractFrequency_ == Monthly) {
        auto validContractMonthNode = XMLUtils::getChildNode(node, "ValidContractMonths");
        if (validContractMonthNode) {
            auto monthNodes = XMLUtils::getChildrenNodes(validContractMonthNode, "Month");
            for (const auto& month : monthNodes) {
                validContractMonths_.insert(parseMonth(XMLUtils::getNodeValue(month)));
            }
        }
    }
    
    strOptionBdc_ = XMLUtils::getChildValue(node, "OptionBusinessDayConvention", false);

    futureContinuationMappings_.clear();
    auto tmp = XMLUtils::getChildrenValues(node, "FutureContinuationMappings",
        "ContinuationMapping", "From", "To", false);
    for (const auto& kv : tmp) {
        futureContinuationMappings_[parseInteger(kv.first)] = parseInteger(kv.second);
    }

    optionContinuationMappings_.clear();
    tmp = XMLUtils::getChildrenValues(node, "OptionContinuationMappings",
        "ContinuationMapping", "From", "To", false);
    for (const auto& kv : tmp) {
        optionContinuationMappings_[parseInteger(kv.first)] = parseInteger(kv.second);
    }

    if (isAveraging_) {
        if (XMLNode* n = XMLUtils::getChildNode(node, "AveragingData")) {
            averagingData_.fromXML(n);
        }
    }

    hoursPerDay_ = Null<Natural>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "HoursPerDay")) {
        hoursPerDay_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "OffPeakPowerIndexData")) {
        offPeakPowerIndexData_ = OffPeakPowerIndexData();
        offPeakPowerIndexData_->fromXML(n);
    }

    indexName_ = XMLUtils::getChildValue(node, "IndexName", false);

    savingsTime_ = XMLUtils::getChildValue(node, "SavingsTime", false, "US");

    balanceOfTheMonth_ = XMLUtils::getChildValueAsBool(node, "BalanceOfTheMonth", false, false);

    balanceOfTheMonthPricingCalendarStr_ = XMLUtils::getChildValue(node, "BalanceOfTheMonthPricingCalendar", false, "");

    optionUnderlyingFutureConvention_ = XMLUtils::getChildValue(node, "OptionUnderlyingFutureConvention", false, "");

    build();
}

XMLNode* CommodityFutureConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CommodityFuture");
    XMLUtils::addChild(doc, node, "Id", id_);

    if (contractFrequency_ != Daily || optionContractFrequency_ != Daily) {
        XMLNode* anchorNode = doc.allocNode("AnchorDay");
        if (anchorType_ == AnchorType::DayOfMonth) {
            XMLUtils::addChild(doc, anchorNode, "DayOfMonth", strDayOfMonth_);
        } else if (anchorType_ == AnchorType::NthWeekday) {
            XMLNode* nthNode = doc.allocNode("NthWeekday");
            XMLUtils::addChild(doc, nthNode, "Nth", strNth_);
            XMLUtils::addChild(doc, nthNode, "Weekday", strWeekday_);
            XMLUtils::appendNode(anchorNode, nthNode);
        } else if (anchorType_ == AnchorType::LastWeekday) {
            XMLUtils::addChild(doc, anchorNode, "LastWeekday", strWeekday_);
        } else if (anchorType_ == AnchorType::BusinessDaysAfter) {
            XMLUtils::addChild(doc, anchorNode, "BusinessDaysAfter", strBusinessDaysAfter_);
        } else if (anchorType_ == AnchorType::WeeklyDayOfTheWeek) {
            XMLUtils::addChild(doc, anchorNode, "WeeklyDayOfTheWeek", strWeekday_);
        } else {
            XMLUtils::addChild(doc, anchorNode, "CalendarDaysBefore", strCalendarDaysBefore_);
        } 
        XMLUtils::appendNode(node, anchorNode);
    }

    XMLUtils::addChild(doc, node, "ContractFrequency", strContractFrequency_);
    XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
    if (!strExpiryCalendar_.empty())
        XMLUtils::addChild(doc, node, "ExpiryCalendar", strExpiryCalendar_);
    XMLUtils::addChild(doc, node, "ExpiryMonthLag", static_cast<int>(expiryMonthLag_));

    if (!strOneContractMonth_.empty())
        XMLUtils::addChild(doc, node, "OneContractMonth", strOneContractMonth_);

    if (!strOffsetDays_.empty())
        XMLUtils::addChild(doc, node, "OffsetDays", strOffsetDays_);

    if (!strBdc_.empty())
        XMLUtils::addChild(doc, node, "BusinessDayConvention", strBdc_);

    XMLUtils::addChild(doc, node, "AdjustBeforeOffset", adjustBeforeOffset_);
    XMLUtils::addChild(doc, node, "IsAveraging", isAveraging_);

    if (optionAnchorType_ == OptionAnchorType::BusinessDaysBefore) {
        if (optionExpiryOffset_>0)
            XMLUtils::addChild(doc, node, "OptionExpiryOffset", strOptionExpiryOffset_);
    } else if (optionAnchorType_ == OptionAnchorType::DayOfMonth) {
        XMLUtils::addChild(doc, node, "OptionExpiryDay", static_cast<int>(optionExpiryDay_));
    } else if (optionAnchorType_ == OptionAnchorType::NthWeekday) {
        XMLNode* nthNode = doc.allocNode("OptionNthWeekday");
        XMLUtils::addChild(doc, nthNode, "Nth", strOptionNth_);
        XMLUtils::addChild(doc, nthNode, "Weekday", strOptionWeekday_);
        XMLUtils::appendNode(node, nthNode);
    } else if (optionAnchorType_ == OptionAnchorType::LastWeekday) {
        XMLUtils::addChild(doc, node, "OptionExpiryLastWeekdayOfMonth", strOptionWeekday_);
    } else if (optionAnchorType_ == OptionAnchorType::WeeklyDayOfTheWeek) {
        XMLUtils::addChild(doc, node, "OptionExpiryWeeklyDayOfTheWeek", strOptionWeekday_);
    }

    if (!prohibitedExpiries_.empty()) {
        XMLNode* prohibitedExpiriesNode = doc.allocNode("ProhibitedExpiries");
        XMLNode* datesNode = XMLUtils::addChild(doc, prohibitedExpiriesNode, "Dates");
        for (ProhibitedExpiry pe : prohibitedExpiries_) {
            XMLUtils::appendNode(datesNode, pe.toXML(doc));
        }
        XMLUtils::appendNode(node, prohibitedExpiriesNode);
    }

    XMLUtils::addChild(doc, node, "OptionExpiryMonthLag", static_cast<int>(optionExpiryMonthLag_));
    

    if (!strOptionBdc_.empty())
        XMLUtils::addChild(doc, node, "OptionBusinessDayConvention", strOptionBdc_);

    if (!futureContinuationMappings_.empty()) {
        map<string, string> tmp;
        for (const auto& kv : futureContinuationMappings_) {
            tmp[to_string(kv.first)] = to_string(kv.second);
        }
        XMLUtils::addChildren(doc, node, "FutureContinuationMappings",
            "ContinuationMapping", "From", "To", tmp);
    }

    if (!optionContinuationMappings_.empty()) {
        map<string, string> tmp;
        for (const auto& kv : optionContinuationMappings_) {
            tmp[to_string(kv.first)] = to_string(kv.second);
        }
        XMLUtils::addChildren(doc, node, "OptionContinuationMappings",
            "ContinuationMapping", "From", "To", tmp);
    }

    if (!averagingData_.empty()) {
        XMLUtils::appendNode(node, averagingData_.toXML(doc));
    }

    if (hoursPerDay_ != Null<Natural>())
        XMLUtils::addChild(doc, node, "HoursPerDay", static_cast<int>(hoursPerDay_));

    if (offPeakPowerIndexData_) {
        XMLUtils::appendNode(node, offPeakPowerIndexData_->toXML(doc));
    }

    if (!indexName_.empty())
        XMLUtils::addChild(doc, node, "IndexName", indexName_);

    if (!savingsTime_.empty())
        XMLUtils::addChild(doc, node, "SavingsTime", savingsTime_);

    if (contractFrequency_ == Monthly && !validContractMonths_.empty() && validContractMonths_.size() < 12) {
        XMLNode* validContractMonthNode = doc.allocNode("ValidContractMonths");
        for (auto  month : validContractMonths_) {
            XMLUtils::addChild(doc, validContractMonthNode, "Month", to_string(month));
        }
        XMLUtils::appendNode(node, validContractMonthNode);
    }

    if (balanceOfTheMonth_) {
        XMLUtils::addChild(doc, node, "BalanceOfTheMonth", balanceOfTheMonth_);
    }

    if (balanceOfTheMonthPricingCalendar_ != Calendar()) {
        XMLUtils::addChild(doc, node, "BalanceOfTheMonthPricingCalendar", to_string(balanceOfTheMonthPricingCalendar_));
    }

    if (!optionUnderlyingFutureConvention_.empty()) {
        XMLUtils::addChild(doc, node, "OptionUnderlyingFutureConvention", optionUnderlyingFutureConvention_);
        XMLUtils::addChild(doc, node, "OptionContractFrequency", strOptionContractFrequency_);
    }

    return node;
}

void CommodityFutureConvention::build() {

    contractFrequency_ = parseAndValidateFrequency(strContractFrequency_);
    if (strOptionContractFrequency_.empty()) {
        optionContractFrequency_ = contractFrequency_;
    } else {
        optionContractFrequency_ = parseAndValidateFrequency(strOptionContractFrequency_);
    }

    if (contractFrequency_ != Daily || optionContractFrequency_ != Daily) {
        if (anchorType_ == AnchorType::DayOfMonth) {
            dayOfMonth_ = lexical_cast<Natural>(strDayOfMonth_);
        } else if (anchorType_ == AnchorType::CalendarDaysBefore) {
            calendarDaysBefore_ = lexical_cast<Natural>(strCalendarDaysBefore_);
        } else if (anchorType_ == AnchorType::LastWeekday) {
            weekday_ = parseWeekday(strWeekday_);
        } else if (anchorType_ == AnchorType::BusinessDaysAfter) {
            businessDaysAfter_ =  lexical_cast<Integer>(strBusinessDaysAfter_);
        } else if (anchorType_ == AnchorType::WeeklyDayOfTheWeek) {
            weekday_ = parseWeekday(strWeekday_);
        } else {
            nth_ = lexical_cast<Natural>(strNth_);
            weekday_ = parseWeekday(strWeekday_);
        }
    }

    calendar_ = parseCalendar(strCalendar_);
    expiryCalendar_ = strExpiryCalendar_.empty() ? calendar_ : parseCalendar(strExpiryCalendar_);

    // Optional entries
    oneContractMonth_ = strOneContractMonth_.empty() ? Month::Jan : parseMonth(strOneContractMonth_);
    offsetDays_ = strOffsetDays_.empty() ? 0 : lexical_cast<Integer>(strOffsetDays_);
    bdc_ = strBdc_.empty() ? Preceding : parseBusinessDayConvention(strBdc_);

    if (optionAnchorType_ == OptionAnchorType::BusinessDaysBefore) {
        if (strOptionExpiryOffset_.empty())
            optionExpiryOffset_ = 0;
        else
            optionExpiryOffset_ = lexical_cast<Natural>(strOptionExpiryOffset_);
    } else if (optionAnchorType_ == OptionAnchorType::NthWeekday) {
        optionNth_ = lexical_cast<Natural>(strOptionNth_);
        optionWeekday_ = parseWeekday(strOptionWeekday_);
    } else if (optionAnchorType_ == OptionAnchorType::DayOfMonth) {
        optionAnchorType_ = OptionAnchorType::DayOfMonth;
        optionExpiryDay_ = lexical_cast<Natural>(strOptionExpiryDay_);
    } else if (optionAnchorType_ == OptionAnchorType::LastWeekday) {
        optionWeekday_ = parseWeekday(strOptionWeekday_);
    } else if (optionAnchorType_ == OptionAnchorType::WeeklyDayOfTheWeek) {
        optionWeekday_ = parseWeekday(strOptionWeekday_);
    } else {
        optionAnchorType_ = OptionAnchorType::BusinessDaysBefore;
        optionExpiryOffset_ = 0;
    }

    optionBdc_ = strOptionBdc_.empty() ? Preceding : parseBusinessDayConvention(strOptionBdc_);

    // Check the continuation mappings
    checkContinuationMappings(futureContinuationMappings_, "future");
    checkContinuationMappings(optionContinuationMappings_, "option");

    // Check that neither of the indexes in OffPeakPowerIndexData self reference
    if (offPeakPowerIndexData_) {
        const string& opIdx = offPeakPowerIndexData_->offPeakIndex();
        QL_REQUIRE(id_ != opIdx, "The off-peak index (" << opIdx << ") cannot equal the index for which" <<
            " we are providing conventions (" << id_ << ").");
        const string& pIdx = offPeakPowerIndexData_->peakIndex();
        QL_REQUIRE(id_ != pIdx, "The peak index (" << pIdx << ") cannot equal the index for which" <<
            " we are providing conventions (" << id_ << ").");
    }

    if (balanceOfTheMonthPricingCalendarStr_.empty()) {
        balanceOfTheMonthPricingCalendar_ = Calendar();
    } else {
        balanceOfTheMonthPricingCalendar_ = parseCalendar(balanceOfTheMonthPricingCalendarStr_);
    }

    QL_REQUIRE(!balanceOfTheMonth_ || isAveraging(), "Balance of the month make only sense for averaging futures");

}

Frequency CommodityFutureConvention::parseAndValidateFrequency(const std::string & strFrequency) {
    auto freq = parseFrequency(strFrequency);
    QL_REQUIRE(freq == Annual || freq == Quarterly || freq == Monthly || freq == Weekly || freq == Daily,
               "Contract frequency should be annual, quarterly, monthly, weekly or daily but got " << freq);
    return freq;
}

bool CommodityFutureConvention::validateBdc(const ProhibitedExpiry& pe) const {
    vector<BusinessDayConvention> bdcs{ pe.futureBdc(), pe.optionBdc() };
    for (auto bdc : bdcs) {
        if (!(bdc == Preceding || bdc == Following || bdc == ModifiedPreceding || bdc == ModifiedFollowing)) {
            WLOG("Prohibited expiry bdc must be one of {Preceding, Following, ModifiedPreceding," <<
                " ModifiedFollowing} but got " << bdc << " for date " << io::iso_date(pe.expiry()) << ".");
            return false;
        }
    }
    return true;
}

FxOptionConvention::FxOptionConvention(const string& id, const string& atmType, const string& deltaType,
                                       const string& switchTenor, const string& longTermAtmType,
                                       const string& longTermDeltaType, const string& riskReversalInFavorOf,
                                       const string& butterflyStyle, const string& fxConventionID)
    : Convention(id, Type::FxOption), fxConventionID_(fxConventionID), strAtmType_(atmType), strDeltaType_(deltaType),
      strSwitchTenor_(switchTenor), strLongTermAtmType_(longTermAtmType), strLongTermDeltaType_(longTermDeltaType),
      strRiskReversalInFavorOf_(riskReversalInFavorOf), strButterflyStyle_(butterflyStyle) {
    build();
}

void FxOptionConvention::build() {
    atmType_ = parseAtmType(strAtmType_);
    deltaType_ = parseDeltaType(strDeltaType_);
    if (!strSwitchTenor_.empty()) {
        switchTenor_ = parsePeriod(strSwitchTenor_);
        longTermAtmType_ = parseAtmType(strLongTermAtmType_);
        longTermDeltaType_ = parseDeltaType(strLongTermDeltaType_);
    } else {
        switchTenor_ = 0 * Days;
        longTermAtmType_ = atmType_;
        longTermDeltaType_ = deltaType_;
    }
    if (!strRiskReversalInFavorOf_.empty()) {
        riskReversalInFavorOf_ = parseOptionType(strRiskReversalInFavorOf_);
    } else {
        riskReversalInFavorOf_ = Option::Call;
    }
    if (strButterflyStyle_.empty() || strButterflyStyle_ == "Broker") {
        butterflyIsBrokerStyle_ = true;
    } else if (strButterflyStyle_ == "Smile") {
        butterflyIsBrokerStyle_ = false;
    } else {
        QL_FAIL("invalid butterfly style '" << strButterflyStyle_ << "', expected Broker or Smile");
    }
}

void FxOptionConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "FxOption");
    type_ = Type::FxOption;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    fxConventionID_ = XMLUtils::getChildValue(node, "FXConventionID", false);

    // Get string values from xml
    strAtmType_ = XMLUtils::getChildValue(node, "AtmType", true);
    strDeltaType_ = XMLUtils::getChildValue(node, "DeltaType", true);
    strSwitchTenor_ = XMLUtils::getChildValue(node, "SwitchTenor", false);
    strLongTermAtmType_ = XMLUtils::getChildValue(node, "LongTermAtmType", false);
    strLongTermDeltaType_ = XMLUtils::getChildValue(node, "LongTermDeltaType", false);
    strRiskReversalInFavorOf_ = XMLUtils::getChildValue(node, "RiskReversalInFavorOf", false);
    strButterflyStyle_ = XMLUtils::getChildValue(node, "ButterflyStyle", false);
    build();
}

XMLNode* FxOptionConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("FxOption");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "FXConventionID", fxConventionID_);
    XMLUtils::addChild(doc, node, "AtmType", strAtmType_);
    XMLUtils::addChild(doc, node, "DeltaType", strDeltaType_);
    XMLUtils::addChild(doc, node, "SwitchTenor", strSwitchTenor_);
    XMLUtils::addChild(doc, node, "LongTermAtmType", strLongTermAtmType_);
    XMLUtils::addChild(doc, node, "LongTermDeltaType", strLongTermDeltaType_);
    XMLUtils::addChild(doc, node, "RiskReversalInFavorOf", strRiskReversalInFavorOf_);
    XMLUtils::addChild(doc, node, "ButterflyStyle", strButterflyStyle_);
    return node;
}

BondYieldConvention::BondYieldConvention()
  : compounding_(Compounded), compoundingName_("Compounded"),
    frequency_(Annual), frequencyName_("Annual"),
    priceType_(QuantLib::Bond::Price::Type::Clean), priceTypeName_("Clean"),
    accuracy_(1.0e-8), maxEvaluations_(100), guess_(0.05) {}

BondYieldConvention::BondYieldConvention(
    const string& id,
    const string& compoundingName,
    const string& frequencyName,
    const string& priceTypeName,
    Real accuracy,
    Size maxEvaluations,
    Real guess)
    : Convention(id, Type::BondYield),
      compoundingName_(compoundingName),
      frequencyName_(frequencyName),
      priceTypeName_(priceTypeName),
      accuracy_(accuracy),
      maxEvaluations_(maxEvaluations),
      guess_(guess) {
    build();
}

void BondYieldConvention::build() {
    compounding_ = parseCompounding(compoundingName_);
    frequency_ = parseFrequency(frequencyName_);
    priceType_ = parseBondPriceType(priceTypeName_);
}

void BondYieldConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "BondYield");
    type_ = Type::BondYield;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    compoundingName_ = XMLUtils::getChildValue(node, "Compounding", true);
    frequencyName_ = XMLUtils::getChildValue(node, "Frequency", false, "Annual");
    priceTypeName_ = XMLUtils::getChildValue(node, "PriceType", false, "Clean");
    accuracy_ = XMLUtils::getChildValueAsDouble(node, "Accuracy", false, 1.0e-8);
    maxEvaluations_ = XMLUtils::getChildValueAsInt(node, "MaxEvaluations", false, 100);
    guess_ = XMLUtils::getChildValueAsDouble(node, "Guess", false, 0.05);
   
    build();
}

XMLNode* BondYieldConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("BondYield");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Compounding", compoundingName_);
    XMLUtils::addChild(doc, node, "Frequency", frequencyName_);
    XMLUtils::addChild(doc, node, "PriceType", priceTypeName_);
    XMLUtils::addChild(doc, node, "Accuracy", accuracy_);
    XMLUtils::addChild(doc, node, "MaxEvaluations", Integer(maxEvaluations_));
    XMLUtils::addChild(doc, node, "Guess", guess_);

    return node;
}

ZeroInflationIndexConvention::ZeroInflationIndexConvention()
    : revised_(false), frequency_(Monthly) {}

ZeroInflationIndexConvention::ZeroInflationIndexConvention(
    const string& id,
    const string& regionName,
    const string& regionCode,
    bool revised,
    const string& frequency,
    const string& availabilityLag,
    const string& currency)
    : Convention(id, Type::ZeroInflationIndex),
      regionName_(regionName),
      regionCode_(regionCode),
      revised_(revised),
      strFrequency_(frequency),
      strAvailabilityLag_(availabilityLag),
      strCurrency_(currency),
      frequency_(Monthly) {
    build();
}

QuantLib::Region ZeroInflationIndexConvention::region() const {
    return QuantLib::CustomRegion(regionName_, regionCode_);
}

void ZeroInflationIndexConvention::build() {
    frequency_ = parseFrequency(strFrequency_);
    availabilityLag_ = parsePeriod(strAvailabilityLag_);
    currency_ = parseCurrency(strCurrency_);
}

void ZeroInflationIndexConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "ZeroInflationIndex");
    type_ = Type::ZeroInflationIndex;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    regionName_ = XMLUtils::getChildValue(node, "RegionName", true);
    regionCode_ = XMLUtils::getChildValue(node, "RegionCode", true);
    revised_ = parseBool(XMLUtils::getChildValue(node, "Revised", true));
    strFrequency_ = XMLUtils::getChildValue(node, "Frequency", true);
    strAvailabilityLag_ = XMLUtils::getChildValue(node, "AvailabilityLag", true);
    strCurrency_ = XMLUtils::getChildValue(node, "Currency", true);

    build();
}

XMLNode* ZeroInflationIndexConvention::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("ZeroInflationIndex");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "RegionName", regionName_);
    XMLUtils::addChild(doc, node, "RegionCode", regionCode_);
    XMLUtils::addChild(doc, node, "Revised", revised_);
    XMLUtils::addChild(doc, node, "Frequency", strFrequency_);
    XMLUtils::addChild(doc, node, "AvailabilityLag", strAvailabilityLag_);
    XMLUtils::addChild(doc, node, "Currency", strCurrency_);

    return node;
}

void Conventions::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Conventions");

    for (XMLNode* child = XMLUtils::getChildNode(node); child; child = XMLUtils::getNextSibling(child)) {

        string type = XMLUtils::getNodeName(child);
        QuantLib::ext::shared_ptr<Convention> convention;

        /* we need to build conventions of type

           - IborIndex
           - OvernightIndex
           - FX

           immediately because

           - for IborIndex other conventions depend on it via parseIborIndex() calls
           - the id of IborIndex convention is changed during build (period is normalized)
           - FX conventions are searched by currencies, not id */

        if (type == "IborIndex") {
            convention = QuantLib::ext::make_shared<IborIndexConvention>();
        } else if (type == "FX") {
            convention = QuantLib::ext::make_shared<FXConvention>();
        } else if (type == "OvernightIndex") {
            convention = QuantLib::ext::make_shared<OvernightIndexConvention>();
        }

        string id = "unknown";
        if (convention) {
            try {
                id = XMLUtils::getChildValue(child, "Id", true);
                DLOG("Building Convention " << id);
                convention->fromXML(child);
                add(convention);
            } catch (const std::exception& e) {
                WLOG("Exception parsing convention "
                     << id << ": " << e.what() << ". This is only a problem if this convention is used later on.");
            }
        } else {
            try {
                id = XMLUtils::getChildValue(child, "Id", true);
                DLOG("Reading Convention " << id);
                unparsed_[id] = std::make_pair(type, XMLUtils::toString(child));
            } catch (const std::exception& e) {
                WLOG("Exception during retrieval of convention "
                     << id << ": " << e.what() << ". This is only a problem if this convention is used later on.");
            }
        }
    }
}

XMLNode* Conventions::toXML(XMLDocument& doc) const {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    XMLNode* conventionsNode = doc.allocNode("Conventions");

    map<string, QuantLib::ext::shared_ptr<Convention>>::const_iterator it;
    for (it = data_.begin(); it != data_.end(); ++it) {
        if (used_.find(it->first) != used_.end())
            XMLUtils::appendNode(conventionsNode, it->second->toXML(doc));
    }
    return conventionsNode;
}

void Conventions::clear() const {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    data_.clear();
}

namespace {
std::string flip(const std::string& id, const std::string& sep = "-") {
    boost::tokenizer<boost::escaped_list_separator<char>> tokenSplit(
        id, boost::escaped_list_separator<char>("\\", sep, "\""));
    std::vector<std::string> tokens(tokenSplit.begin(), tokenSplit.end());

    bool eligible = false;
    for (auto const& t : tokens) {
        eligible = eligible || (t == "XCCY" || t == "FX" || t == "FXOPTION");
    }

    if (eligible && (tokens.size() > 2 && tokens[0].size() == 3 && tokens[1].size() == 3)) {
        std::string id2 = tokens[1] + sep + tokens[0];
        for (Size i = 2; i < tokens.size(); ++i)
            id2 += sep + tokens[i];
        return id2;
    }

    return id;
}
}

QuantLib::ext::shared_ptr<Convention> Conventions::get(const string& id) const {

    {
        boost::shared_lock<boost::shared_mutex> lock(mutex_);
        if (auto it = data_.find(id); it != data_.end()) {
            used_.insert(id);
            return it->second;
        }
        if (auto it = data_.find(flip(id)); it != data_.end()) {
            used_.insert(flip(id));
            return it->second;
        }
    }

    std::string type, unparsed;
    {
        boost::unique_lock<boost::shared_mutex> lock(mutex_);
        if (auto it = unparsed_.find(id); it != unparsed_.end()) {
            std::tie(type, unparsed) = it->second;
            unparsed_.erase(id);
        } else if (auto it = unparsed_.find(flip(id)); it != unparsed_.end()) {
            std::tie(type, unparsed) = it->second;
            unparsed_.erase(flip(id));
        }
    }

    if (unparsed.empty()) {
        QL_FAIL("Convention '" << id << "' not found.");
    }

    QuantLib::ext::shared_ptr<Convention> convention;
    if (type == "Zero") {
        convention = QuantLib::ext::make_shared<ZeroRateConvention>();
    } else if (type == "Deposit") {
        convention = QuantLib::ext::make_shared<DepositConvention>();
    } else if (type == "Future") {
        convention = QuantLib::ext::make_shared<FutureConvention>();
    } else if (type == "FRA") {
        convention = QuantLib::ext::make_shared<FraConvention>();
    } else if (type == "OIS") {
        convention = QuantLib::ext::make_shared<OisConvention>();
    } else if (type == "Swap") {
        convention = QuantLib::ext::make_shared<IRSwapConvention>();
    } else if (type == "AverageOIS") {
        convention = QuantLib::ext::make_shared<AverageOisConvention>();
    } else if (type == "TenorBasisSwap") {
        convention = QuantLib::ext::make_shared<TenorBasisSwapConvention>();
    } else if (type == "TenorBasisTwoSwap") {
        convention=QuantLib::ext::make_shared<TenorBasisTwoSwapConvention>();
    } else if (type == "BMABasisSwap") {
        convention = QuantLib::ext::make_shared<BMABasisSwapConvention>();
    } else if (type == "CrossCurrencyBasis") {
        convention=QuantLib::ext::make_shared<CrossCcyBasisSwapConvention>();
    } else if (type == "CrossCurrencyFixFloat") {
        convention=QuantLib::ext::make_shared<CrossCcyFixFloatSwapConvention>();
    } else if (type == "CDS") {
        convention=QuantLib::ext::make_shared<CdsConvention>();
    } else if (type == "SwapIndex") {
        convention=QuantLib::ext::make_shared<SwapIndexConvention>();
    } else if (type == "InflationSwap") {
        convention=QuantLib::ext::make_shared<InflationSwapConvention>();
    } else if (type == "CmsSpreadOption") {
        convention=QuantLib::ext::make_shared<CmsSpreadOptionConvention>();
    } else if (type == "CommodityForward") {
        convention = QuantLib::ext::make_shared<CommodityForwardConvention>();
    } else if (type == "CommodityFuture") {
        convention = QuantLib::ext::make_shared<CommodityFutureConvention>();
    } else if (type == "FxOption") {
        convention = QuantLib::ext::make_shared<FxOptionConvention>();
    } else if (type == "ZeroInflationIndex") {
        convention = QuantLib::ext::make_shared<ZeroInflationIndexConvention>();
    } else if (type == "BondYield") {
        convention = QuantLib::ext::make_shared<BondYieldConvention>();
    } else {
        QL_FAIL("Convention '" << id << "' has unknown type '" + type + "' not recognized.");
    }

    try {
        DLOG("Building Convention " << id);
        convention->fromXMLString(unparsed);
        add(convention);
        used_.insert(id);
    } catch (exception& e) {
        WLOG("Convention '" << id << "' could not be built: " << e.what());
        QL_FAIL("Convention '" << id << "' could not be built: " << e.what());
    }

    return convention;
}

QuantLib::ext::shared_ptr<Convention> Conventions::getFxConvention(const string& ccy1, const string& ccy2) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    for (auto c : data_) {
        auto fxCon = QuantLib::ext::dynamic_pointer_cast<FXConvention>(c.second);
        if (fxCon) {
            string source = fxCon->sourceCurrency().code();
            string target = fxCon->targetCurrency().code();
            if ((source == ccy1 && target == ccy2) || (target == ccy1 && source == ccy2)) {
                used_.insert(c.first);
                return fxCon;
            }
        }
    }
    QL_FAIL("FX convention for ccys '" << ccy1 << "' and '" << ccy2 << "' not found.");
}

pair<bool, QuantLib::ext::shared_ptr<Convention>> Conventions::get(const string& id, const Convention::Type& type) const {
    try {
        auto c = get(id);
        if (c->type() == type) {
            used_.insert(id);
            return std::make_pair(true, c);
        }
    } catch (const std::exception& e) {
    }
    return make_pair(false, nullptr);
}

std::set<QuantLib::ext::shared_ptr<Convention>> Conventions::get(const Convention::Type& type) const {
    std::set<QuantLib::ext::shared_ptr<Convention>> result;
    std::set<std::string> unparsedIds;
    std::string typeStr = ore::data::to_string(type);
    {
        boost::shared_lock<boost::shared_mutex> lock(mutex_);
        for (auto const& d : data_) {
            if (d.second->type() == type) {
                used_.insert(d.first);
                result.insert(d.second);
            }
        }
        for (auto const& u : unparsed_) {
            if (u.second.first == typeStr)
                unparsedIds.insert(u.first);
        }
    }
    for (auto const& id : unparsedIds) {
        result.insert(get(id));
    }
    return result;
}

bool Conventions::has(const string& id) const {
    try {
        get(id);
    } catch (const std::exception& e) {
        return false;
    }
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return data_.find(id) != data_.end() || unparsed_.find(id) != unparsed_.end() ||
           data_.find(flip(id)) != data_.end() || unparsed_.find(flip(id)) != unparsed_.end();
}

bool Conventions::has(const std::string& id, const Convention::Type& type) const {
    return get(id, type).first;
}

void Conventions::add(const QuantLib::ext::shared_ptr<Convention>& convention) const {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    const string& id = convention->id();
    QL_REQUIRE(data_.find(id) == data_.end(), "Convention already exists for id " << id);
    data_[id] = convention;
}

std::ostream& operator<<(std::ostream& out, Convention::Type type) {
    switch (type) {
    case Convention::Type::Zero:
        return out << "Zero";
    case Convention::Type::Deposit:
        return out << "Deposit";
    case Convention::Type::Future:
        return out << "Future";
    case Convention::Type::FRA:
        return out << "FRA";
    case Convention::Type::OIS:
        return out << "OIS";
    case Convention::Type::Swap:
        return out << "Swap";
    case Convention::Type::AverageOIS:
        return out << "AverageOIS";
    case Convention::Type::TenorBasisSwap:
        return out << "TenorBasisSwap";
    case Convention::Type::TenorBasisTwoSwap:
        return out << "TenorBasisTwoSwap";
    case Convention::Type::BMABasisSwap:
        return out << "BMABasisSwap";
    case Convention::Type::FX:
        return out << "FX";
    case Convention::Type::CrossCcyBasis:
        return out << "CrossCcyBasis";
    case Convention::Type::CrossCcyFixFloat:
        return out << "CrossCcyFixFloat";
    case Convention::Type::CDS:
        return out << "CDS";
    case Convention::Type::IborIndex:
        return out << "IborIndex";
    case Convention::Type::OvernightIndex:
        return out << "OvernightIndex";
    case Convention::Type::SwapIndex:
        return out << "SwapIndex";
    case Convention::Type::ZeroInflationIndex:
        return out << "ZeroInflationIndex";
    case Convention::Type::InflationSwap:
        return out << "InflationSwap";
    case Convention::Type::SecuritySpread:
        return out << "SecuritySpread";
    case Convention::Type::CMSSpreadOption:
        return out << "CMSSpreadOption";
    case Convention::Type::CommodityForward:
        return out << "CommodityForward";
    case Convention::Type::CommodityFuture:
        return out << "CommodityFuture";
    case Convention::Type::FxOption:
        return out << "FxOption";
    case Convention::Type::BondYield:
        return out << "BondYield";
    default:
        return out << "unknown convention type (" << static_cast<int>(type) << ")";
    }
}

} // namespace data
} // namespace ore
