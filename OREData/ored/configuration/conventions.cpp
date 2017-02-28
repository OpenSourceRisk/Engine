/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file wrap/data/conventions.cpp
    \brief Currency and instrument specific conventions
    \ingroup
*/

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ored/utilities/indexparser.hpp>

using namespace QuantLib;
using namespace std;
using QuantExt::SubPeriodsCoupon;
using boost::lexical_cast;

namespace {

// TODO move to parsers
QuantExt::SubPeriodsCoupon::Type parseSubPeriodsCouponType(const string& s) {
    if (s == "Compounding")
        return QuantExt::SubPeriodsCoupon::Compounding;
    else if (s == "Averaging")
        return QuantExt::SubPeriodsCoupon::Averaging;
    else
        QL_FAIL("SubPeriodsCoupon type " << s << " not recognized");
};
}

namespace ore {
namespace data {

Convention::Convention(const string& id, Type type) : type_(type), id_(id) {}

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

XMLNode* ZeroRateConvention::toXML(XMLDocument& doc) {

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
                                     const string& eom, const string& dayCounter)
    : Convention(id, Type::Deposit), indexBased_(false), strCalendar_(calendar), strConvention_(convention),
      strEom_(eom), strDayCounter_(dayCounter) {
    build();
}

void DepositConvention::build() {
    calendar_ = parseCalendar(strCalendar_);
    convention_ = parseBusinessDayConvention(strConvention_);
    eom_ = parseBool(strEom_);
    dayCounter_ = parseDayCounter(strDayCounter_);
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
        build();
    }
}

XMLNode* DepositConvention::toXML(XMLDocument& doc) {

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
    }

    return node;
}

FutureConvention::FutureConvention(const string& id, const string& index)
    : Convention(id, Type::Future), strIndex_(index), index_(parseIborIndex(index)) {}

void FutureConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Future");
    type_ = Type::Future;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    index_ = parseIborIndex(strIndex_);
}

XMLNode* FutureConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("Future");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);

    return node;
}

FraConvention::FraConvention(const string& id, const string& index)
    : Convention(id, Type::FRA), strIndex_(index), index_(parseIborIndex(index)) {}

void FraConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "FRA");
    type_ = Type::FRA;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    index_ = parseIborIndex(strIndex_);
}

XMLNode* FraConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("FRA");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);

    return node;
}

OisConvention::OisConvention(const string& id, const string& spotLag, const string& index,
                             const string& fixedDayCounter, const string& paymentLag, const string& eom,
                             const string& fixedFrequency, const string& fixedConvention,
                             const string& fixedPaymentConvention, const string& rule)
    : Convention(id, Type::OIS), strSpotLag_(spotLag), strIndex_(index), strFixedDayCounter_(fixedDayCounter),
      strPaymentLag_(paymentLag), strEom_(eom), strFixedFrequency_(fixedFrequency),
      strFixedConvention_(fixedConvention), strFixedPaymentConvention_(fixedPaymentConvention), strRule_(rule) {
    build();
}

void OisConvention::build() {
    // First check that we have an overnight index.
    index_ = boost::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(strIndex_));
    QL_REQUIRE(index_, "The index string, " << strIndex_ << ", does not represent an overnight index.");

    spotLag_ = lexical_cast<Natural>(strSpotLag_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    paymentLag_ = strPaymentLag_.empty() ? 0 : lexical_cast<Natural>(strPaymentLag_);
    eom_ = strEom_.empty() ? false : parseBool(strEom_);
    fixedFrequency_ = strFixedFrequency_.empty() ? Annual : parseFrequency(strFixedFrequency_);
    fixedConvention_ = strFixedConvention_.empty() ? Following : parseBusinessDayConvention(strFixedConvention_);
    fixedPaymentConvention_ =
        strFixedPaymentConvention_.empty() ? Following : parseBusinessDayConvention(strFixedPaymentConvention_);
    rule_ = strRule_.empty() ? DateGeneration::Backward : parseDateGenerationRule(strRule_);
}

void OisConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "OIS");
    type_ = Type::OIS;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSpotLag_ = XMLUtils::getChildValue(node, "SpotLag", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);
    strPaymentLag_ = XMLUtils::getChildValue(node, "PaymentLag", false);
    strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    strFixedFrequency_ = XMLUtils::getChildValue(node, "FixedFrequency", false);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", false);
    strFixedPaymentConvention_ = XMLUtils::getChildValue(node, "FixedPaymentConvention", false);
    strRule_ = XMLUtils::getChildValue(node, "Rule", false);

    build();
}

XMLNode* OisConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("OIS");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    XMLUtils::addChild(doc, node, "PaymentLag", strPaymentLag_);
    XMLUtils::addChild(doc, node, "EOM", strEom_);
    XMLUtils::addChild(doc, node, "FixedFrequency", strFixedFrequency_);
    XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    XMLUtils::addChild(doc, node, "FixedPaymentConvention", strFixedPaymentConvention_);
    XMLUtils::addChild(doc, node, "Rule", strRule_);

    return node;
}

SwapIndexConvention::SwapIndexConvention(const string& id, const string& conventions)
    : Convention(id, Type::SwapIndex), strConventions_(conventions) {}

void SwapIndexConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "SwapIndex");
    type_ = Type::SwapIndex;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strConventions_ = XMLUtils::getChildValue(node, "Conventions", true);
}

XMLNode* SwapIndexConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("SwapIndex");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Conventions", strConventions_);

    return node;
}
IRSwapConvention::IRSwapConvention(const string& id, const string& fixedCalendar, const string& fixedFrequency,
                                   const string& fixedConvention, const string& fixedDayCounter, const string& index,
                                   bool hasSubPeriod, const string& floatFrequency, const string& subPeriodsCouponType)

    : Convention(id, Type::Swap), strFixedCalendar_(fixedCalendar), strFixedFrequency_(fixedFrequency),
      strFixedConvention_(fixedConvention), strFixedDayCounter_(fixedDayCounter), strIndex_(index),
      hasSubPeriod_(hasSubPeriod), strFloatFrequency_(floatFrequency), strSubPeriodsCouponType_(subPeriodsCouponType) {
    build();
}

void IRSwapConvention::build() {
    fixedCalendar_ = parseCalendar(strFixedCalendar_);
    fixedFrequency_ = parseFrequency(strFixedFrequency_);
    fixedConvention_ = parseBusinessDayConvention(strFixedConvention_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    index_ = parseIborIndex(strIndex_);

    if (hasSubPeriod_) {
        floatFrequency_ = parseFrequency(strFloatFrequency_);
        subPeriodsCouponType_ = parseSubPeriodsCouponType(strSubPeriodsCouponType_);
    } else {
        floatFrequency_ = NoFrequency;
        subPeriodsCouponType_ = QuantExt::SubPeriodsCoupon::Compounding;
    }
}

void IRSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Swap");
    type_ = Type::Swap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strFixedCalendar_ = XMLUtils::getChildValue(node, "FixedCalendar", true);
    strFixedFrequency_ = XMLUtils::getChildValue(node, "FixedFrequency", true);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);

    // optional
    strFloatFrequency_ = XMLUtils::getChildValue(node, "FloatFrequency", false);
    strSubPeriodsCouponType_ = XMLUtils::getChildValue(node, "SubPeriodsCouponType", false);
    hasSubPeriod_ = (strFloatFrequency_ != "");

    build();
}

XMLNode* IRSwapConvention::toXML(XMLDocument& doc) {

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

AverageOisConvention::AverageOisConvention(const string& id, const string& spotLag, const string& fixedTenor,
                                           const string& fixedDayCounter, const string& fixedCalendar,
                                           const string& fixedConvention, const string& fixedPaymentConvention,
                                           const string& index, const string& onTenor, const string& rateCutoff)
    : Convention(id, Type::AverageOIS), strSpotLag_(spotLag), strFixedTenor_(fixedTenor),
      strFixedDayCounter_(fixedDayCounter), strFixedCalendar_(fixedCalendar), strFixedConvention_(fixedConvention),
      strFixedPaymentConvention_(fixedPaymentConvention), strIndex_(index), strOnTenor_(onTenor),
      strRateCutoff_(rateCutoff) {
    build();
}

void AverageOisConvention::build() {
    // First check that we have an overnight index.
    index_ = boost::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(strIndex_));
    QL_REQUIRE(index_, "The index string, " << strIndex_ << ", does not represent an overnight index.");

    spotLag_ = lexical_cast<Natural>(strSpotLag_);
    fixedTenor_ = parsePeriod(strFixedTenor_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    fixedCalendar_ = parseCalendar(strFixedCalendar_);
    fixedConvention_ = parseBusinessDayConvention(strFixedConvention_);
    fixedPaymentConvention_ = parseBusinessDayConvention(strFixedPaymentConvention_);
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
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strOnTenor_ = XMLUtils::getChildValue(node, "OnTenor", true);
    strRateCutoff_ = XMLUtils::getChildValue(node, "RateCutoff", true);

    build();
}

XMLNode* AverageOisConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("AverageOIS");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
    XMLUtils::addChild(doc, node, "FixedTenor", strFixedTenor_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    XMLUtils::addChild(doc, node, "FixedCalendar", strFixedCalendar_);
    XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    XMLUtils::addChild(doc, node, "FixedPaymentConvention", strFixedPaymentConvention_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "OnTenor", strOnTenor_);
    XMLUtils::addChild(doc, node, "RateCutoff", strRateCutoff_);

    return node;
}

TenorBasisSwapConvention::TenorBasisSwapConvention(const string& id, const string& longIndex, const string& shortIndex,
                                                   const string& shortPayTenor, const string& spreadOnShort,
                                                   const string& includeSpread, const string& subPeriodsCouponType)
    : Convention(id, Type::TenorBasisSwap), strLongIndex_(longIndex), strShortIndex_(shortIndex),
      strShortPayTenor_(shortPayTenor), strSpreadOnShort_(spreadOnShort), strIncludeSpread_(includeSpread),
      strSubPeriodsCouponType_(subPeriodsCouponType) {
    build();
}

void TenorBasisSwapConvention::build() {
    longIndex_ = parseIborIndex(strLongIndex_);
    shortIndex_ = parseIborIndex(strShortIndex_);
    shortPayTenor_ = strShortPayTenor_.empty() ? shortIndex_->tenor() : parsePeriod(strShortPayTenor_);
    spreadOnShort_ = strSpreadOnShort_.empty() ? true : parseBool(strSpreadOnShort_);
    includeSpread_ = strIncludeSpread_.empty() ? false : parseBool(strIncludeSpread_);
    subPeriodsCouponType_ = strSubPeriodsCouponType_.empty() ? SubPeriodsCoupon::Compounding
                                                             : parseSubPeriodsCouponType(strSubPeriodsCouponType_);
}

void TenorBasisSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "TenorBasisSwap");
    type_ = Type::TenorBasisSwap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strLongIndex_ = XMLUtils::getChildValue(node, "LongIndex", true);
    strShortIndex_ = XMLUtils::getChildValue(node, "ShortIndex", true);
    strShortPayTenor_ = XMLUtils::getChildValue(node, "ShortPayTenor", false);
    strSpreadOnShort_ = XMLUtils::getChildValue(node, "SpreadOnShort", false);
    strIncludeSpread_ = XMLUtils::getChildValue(node, "IncludeSpread", false);
    strSubPeriodsCouponType_ = XMLUtils::getChildValue(node, "SubPeriodsCouponType", false);

    build();
}

XMLNode* TenorBasisSwapConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("TenorBasisSwap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "LongIndex", strLongIndex_);
    XMLUtils::addChild(doc, node, "ShortIndex", strShortIndex_);
    XMLUtils::addChild(doc, node, "ShortPayTenor", strShortPayTenor_);
    XMLUtils::addChild(doc, node, "SpreadOnShort", strSpreadOnShort_);
    XMLUtils::addChild(doc, node, "IncludeSpread", strIncludeSpread_);
    XMLUtils::addChild(doc, node, "SubPeriodsCouponType", strSubPeriodsCouponType_);

    return node;
}

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
    longIndex_ = parseIborIndex(strLongIndex_);
    shortFixedFrequency_ = parseFrequency(strShortFixedFrequency_);
    shortFixedConvention_ = parseBusinessDayConvention(strShortFixedConvention_);
    shortFixedDayCounter_ = parseDayCounter(strShortFixedDayCounter_);
    shortIndex_ = parseIborIndex(strShortIndex_);
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

XMLNode* TenorBasisTwoSwapConvention::toXML(XMLDocument& doc) {

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

FXConvention::FXConvention(const string& id, const string& spotDays, const string& sourceCurrency,
                           const string& targetCurrency, const string& pointsFactor, const string& advanceCalendar,
                           const string& spotRelative)
    : Convention(id, Type::FX), strSpotDays_(spotDays), strSourceCurrency_(sourceCurrency),
      strTargetCurrency_(targetCurrency), strPointsFactor_(pointsFactor), strAdvanceCalendar_(advanceCalendar),
      strSpotRelative_(spotRelative) {
    build();
}

void FXConvention::build() {
    spotDays_ = lexical_cast<Natural>(strSpotDays_);
    sourceCurrency_ = parseCurrency(strSourceCurrency_);
    targetCurrency_ = parseCurrency(strTargetCurrency_);
    pointsFactor_ = parseReal(strPointsFactor_);
    advanceCalendar_ = strAdvanceCalendar_.empty() ? NullCalendar() : parseCalendar(strAdvanceCalendar_);
    spotRelative_ = strSpotRelative_.empty() ? true : parseBool(strSpotRelative_);
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

    build();
}

XMLNode* FXConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("FX");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotDays", strSpotDays_);
    XMLUtils::addChild(doc, node, "SourceCurrency", strSourceCurrency_);
    XMLUtils::addChild(doc, node, "TargetCurrency", strTargetCurrency_);
    XMLUtils::addChild(doc, node, "PointsFactor", strPointsFactor_);
    XMLUtils::addChild(doc, node, "AdvanceCalendar", strAdvanceCalendar_);
    XMLUtils::addChild(doc, node, "SpotRelative", strSpotRelative_);

    return node;
}

CrossCcyBasisSwapConvention::CrossCcyBasisSwapConvention(const string& id, const string& strSettlementDays,
                                                         const string& strSettlementCalendar,
                                                         const string& strRollConvention, const string& flatIndex,
                                                         const string& spreadIndex, const string& strEom)
    : Convention(id, Type::CrossCcyBasis), strSettlementDays_(strSettlementDays),
      strSettlementCalendar_(strSettlementCalendar), strRollConvention_(strRollConvention), strFlatIndex_(flatIndex),
      strSpreadIndex_(spreadIndex), strEom_(strEom) {
    build();
}

void CrossCcyBasisSwapConvention::build() {
    settlementDays_ = lexical_cast<Natural>(strSettlementDays_);
    settlementCalendar_ = parseCalendar(strSettlementCalendar_);
    rollConvention_ = parseBusinessDayConvention(strRollConvention_);
    flatIndex_ = parseIborIndex(strFlatIndex_);
    spreadIndex_ = parseIborIndex(strSpreadIndex_);
    eom_ = strEom_.empty() ? false : parseBool(strEom_);
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

    build();
}

XMLNode* CrossCcyBasisSwapConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CrossCurrencyBasis");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SettlementDays", strSettlementDays_);
    XMLUtils::addChild(doc, node, "SettlementCalendar", strSettlementCalendar_);
    XMLUtils::addChild(doc, node, "RollConvention", strRollConvention_);
    XMLUtils::addChild(doc, node, "FlatIndex", strFlatIndex_);
    XMLUtils::addChild(doc, node, "SpreadIndex", strSpreadIndex_);
    XMLUtils::addChild(doc, node, "EOM", strEom_);

    return node;
}

CdsConvention::CdsConvention(const string& id, const string& strSettlementDays, const string& strCalendar,
                             const string& strFrequency, const string& strPaymentConvention, const string& strRule,
                             const string& strDayCounter, const string& strSettlesAccrual,
                             const string& strPaysAtDefaultTime)
    : Convention(id, Type::CDS), strSettlementDays_(strSettlementDays), strCalendar_(strCalendar),
      strFrequency_(strFrequency), strPaymentConvention_(strPaymentConvention), strRule_(strRule),
      strDayCounter_(strDayCounter), strSettlesAccrual_(strSettlesAccrual),
      strPaysAtDefaultTime_(strPaysAtDefaultTime) {
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
    build();
}

XMLNode* CdsConvention::toXML(XMLDocument& doc) {

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
    return node;
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

XMLNode* SecuritySpreadConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("BondSpread");
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

void Conventions::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Conventions");

    for (XMLNode* child = XMLUtils::getChildNode(node); child; child = XMLUtils::getNextSibling(child)) {

        boost::shared_ptr<Convention> convention;
        string childName = XMLUtils::getNodeName(child);

        if (childName == "Zero") {
            convention.reset(new ZeroRateConvention());
        } else if (childName == "Deposit") {
            convention.reset(new DepositConvention());
        } else if (childName == "Future") {
            convention.reset(new FutureConvention());
        } else if (childName == "FRA") {
            convention.reset(new FraConvention());
        } else if (childName == "OIS") {
            convention.reset(new OisConvention());
        } else if (childName == "Swap") {
            convention.reset(new IRSwapConvention());
        } else if (childName == "AverageOIS") {
            convention.reset(new AverageOisConvention());
        } else if (childName == "TenorBasisSwap") {
            convention.reset(new TenorBasisSwapConvention());
        } else if (childName == "TenorBasisTwoSwap") {
            convention.reset(new TenorBasisTwoSwapConvention());
        } else if (childName == "FX") {
            convention.reset(new FXConvention());
        } else if (childName == "CrossCurrencyBasis") {
            convention.reset(new CrossCcyBasisSwapConvention());
        } else if (childName == "CDS") {
            convention.reset(new CdsConvention());
        } else if (childName == "SwapIndex") {
            convention.reset(new SwapIndexConvention());
        } else {
            QL_FAIL("Convention name, " << childName << ", not recognized.");
        }

        string id = XMLUtils::getChildValue(child, "Id", true);

        try {
            DLOG("Loading Convention " << id);
            convention->fromXML(child);
            add(convention);
        } catch (exception& e) {
            LOG("Exception parsing convention "
                "XML Node (id = "
                << id << ") : " << e.what());
        }
    }
}

XMLNode* Conventions::toXML(XMLDocument& doc) {

    XMLNode* conventionsNode = doc.allocNode("Conventions");

    map<string, boost::shared_ptr<Convention>>::const_iterator it;
    for (it = data_.begin(); it != data_.end(); ++it) {
        XMLUtils::appendNode(conventionsNode, data_[it->first]->toXML(doc));
    }
    return conventionsNode;
}

void Conventions::clear() { data_.clear(); }

boost::shared_ptr<Convention> Conventions::get(const string& id) const {
    auto it = data_.find(id);
    QL_REQUIRE(it != data_.end(), "Cannot find conventions for id " << id);
    return it->second;
}

void Conventions::add(const boost::shared_ptr<Convention>& convention) {
    const string& id = convention->id();
    QL_REQUIRE(data_.find(id) == data_.end(), "Convention already exists for id " << id);
    data_[id] = convention;
}
}
}
