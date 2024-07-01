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

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/builders/capflooredaveragebmacouponleg.hpp>
#include <ored/portfolio/builders/capflooredaverageonindexedcouponleg.hpp>
#include <ored/portfolio/builders/capflooredcpileg.hpp>
#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ored/portfolio/builders/capfloorednonstandardyoyleg.hpp>
#include <ored/portfolio/builders/capflooredovernightindexedcouponleg.hpp>
#include <ored/portfolio/builders/capflooredyoyleg.hpp>
#include <ored/portfolio/builders/cms.hpp>
#include <ored/portfolio/builders/cmsspread.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/makenonstandardlegs.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/types.hpp>
#include <ored/utilities/bondindexbuilder.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/vectorutils.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/brlcdicouponpricer.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/cmbcoupon.hpp>
#include <qle/cashflows/couponpricer.hpp>
#include <qle/cashflows/cpicoupon.hpp>
#include <qle/cashflows/cpicouponpricer.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/floatingannuitycoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/nonstandardcapflooredyoyinflationcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/strippedcapflooredcpicoupon.hpp>
#include <qle/cashflows/strippedcapflooredyoyinflationcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/cashflows/subperiodscouponpricer.hpp>
#include <qle/cashflows/yoyinflationcoupon.hpp>
#include <qle/cashflows/zerofixedcoupon.hpp>
#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/indexes/bondindex.hpp>
#include <qle/instruments/forwardbond.hpp>

#include <ql/cashflow.hpp>
#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/cpicouponpricer.hpp>
#include <ql/cashflows/digitalcmscoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/errors.hpp>
#include <ql/experimental/coupons/digitalcmsspreadcoupon.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/utilities/vectors.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <boost/range/adaptor/transformed.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

bool lessThan(const string& s1, const string& s2) { return s1 < s2; }

void CashflowData::fromXML(XMLNode* node) {
    // allow for empty Cashflow legs without any payments
    if(node == nullptr)
	return;
    XMLUtils::checkNode(node, legNodeName());
    amounts_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Cashflow", "Amount", "date", dates_, &parseReal, false);

    auto p = sort_permutation(dates_, lessThan);
    apply_permutation_in_place(dates_, p);
    apply_permutation_in_place(amounts_, p);
}

XMLNode* CashflowData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Cashflow", "Amount", amounts_, "date", dates_);
    return node;
}

void FixedLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    rates_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Rates", "Rate", "startDate", rateDates_, parseReal,
                                                             true);
}

XMLNode* FixedLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    return node;
}

void ZeroCouponFixedLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    rates_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Rates", "Rate", "startDate", rateDates_, &parseReal,
                                                             true);
    XMLNode* compNode = XMLUtils::getChildNode(node, "Compounding");
    if (compNode)
        compounding_ = XMLUtils::getChildValue(node, "Compounding", true);
    else
        compounding_ = "Compounded";
    QL_REQUIRE(compounding_ == "Compounded" || compounding_ == "Simple",
               "Compounding method " << compounding_ << " not supported");
    XMLNode* subtractNotionalNode = XMLUtils::getChildNode(node, "SubtractNotional");
    if (subtractNotionalNode)
        subtractNotional_ = XMLUtils::getChildValueAsBool(node, "SubtractNotional", true);
    else
        subtractNotional_ = true;
}

XMLNode* ZeroCouponFixedLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    XMLUtils::addChild(doc, node, "Compounding", compounding_);
    XMLUtils::addChild(doc, node, "SubtractNotional", subtractNotional_);
    return node;
}

void FloatingLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = internalIndexName(XMLUtils::getChildValue(node, "Index", true));
    indices_.insert(index_);
    // These are all optional
    spreads_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Spreads", "Spread", "startDate", spreadDates_,
                                                               &parseReal);
    isInArrears_ = boost::none;
    lastRecentPeriod_ = boost::none;
    isAveraged_ = hasSubPeriods_ = includeSpread_ = false;
    if (XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears"))
        isInArrears_ = parseBool(XMLUtils::getNodeValue(arrNode));
    if (XMLNode* lrNode = XMLUtils::getChildNode(node, "LastRecentPeriod"))
        lastRecentPeriod_ = parsePeriod(XMLUtils::getNodeValue(lrNode));
    lastRecentPeriodCalendar_ = XMLUtils::getChildValue(node, "LastRecentPeriodCalendar", false);
    if (XMLNode* avgNode = XMLUtils::getChildNode(node, "IsAveraged"))
        isAveraged_ = parseBool(XMLUtils::getNodeValue(avgNode));
    if (XMLNode* spNode = XMLUtils::getChildNode(node, "HasSubPeriods"))
        hasSubPeriods_ = parseBool(XMLUtils::getNodeValue(spNode));
    if (XMLNode* incSpNode = XMLUtils::getChildNode(node, "IncludeSpread"))
        includeSpread_ = parseBool(XMLUtils::getNodeValue(incSpNode));
    if (auto n = XMLUtils::getChildNode(node, "FixingDays"))
        fixingDays_ = parseInteger(XMLUtils::getNodeValue(n));
    else
        fixingDays_ = Null<Size>();
    if (auto n = XMLUtils::getChildNode(node, "Lookback"))
        lookback_ = parsePeriod(XMLUtils::getNodeValue(n));
    else
        lookback_ = 0 * Days;
    if (auto n = XMLUtils::getChildNode(node, "RateCutoff"))
        rateCutoff_ = parseInteger(XMLUtils::getNodeValue(n));
    else
        rateCutoff_ = Null<Size>();
    caps_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Caps", "Cap", "startDate", capDates_, &parseReal);
    floors_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Floors", "Floor", "startDate", floorDates_, &parseReal);
    gearings_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Gearings", "Gearing", "startDate", gearingDates_,
                                                                &parseReal, false);
    if (XMLUtils::getChildNode(node, "NakedOption"))
        nakedOption_ = XMLUtils::getChildValueAsBool(node, "NakedOption", false);
    else
        nakedOption_ = false;

    if (XMLUtils::getChildNode(node, "LocalCapFloor"))
        localCapFloor_ = XMLUtils::getChildValueAsBool(node, "LocalCapFloor", false);
    else
        localCapFloor_ = false;

    if (auto tmp = XMLUtils::getChildNode(node, "FixingSchedule")) {
        fixingSchedule_.fromXML(tmp);
    }
    if (auto tmp = XMLUtils::getChildNode(node, "ResetSchedule")) {
        resetSchedule_.fromXML(tmp);
    }
    vector<std::string> histFixingDates;
    vector<QuantLib::Real> histFixingValues = XMLUtils::getChildrenValuesWithAttributes<Real>(
        node, "HistoricalFixings", "Fixing", "fixingDate", histFixingDates,
                                                  &parseReal);

    QL_REQUIRE(histFixingDates.size() == histFixingValues.size(), "Mismatch Fixing values and dates");
    for (size_t i = 0; i < histFixingDates.size(); ++i) {
        auto dt = parseDate(histFixingDates[i]);
        historicalFixings_[dt] = histFixingValues[i];
    }
}

XMLNode* FloatingLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", index_);
    if (isInArrears_)
        XMLUtils::addChild(doc, node, "IsInArrears", *isInArrears_);
    if (lastRecentPeriod_)
        XMLUtils::addChild(doc, node, "LastRecentPeriod", *lastRecentPeriod_);
    if (!lastRecentPeriodCalendar_.empty())
        XMLUtils::addChild(doc, node, "LastRecentPeriodCalendar", lastRecentPeriodCalendar_);
    XMLUtils::addChild(doc, node, "IsAveraged", isAveraged_);
    XMLUtils::addChild(doc, node, "HasSubPeriods", hasSubPeriods_);
    XMLUtils::addChild(doc, node, "IncludeSpread", includeSpread_);
    if (fixingDays_ != Null<Size>())
        XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
    if (lookback_ != 0 * Days)
        XMLUtils::addChild(doc, node, "Lookback", ore::data::to_string(lookback_));
    if (rateCutoff_ != Null<Size>())
        XMLUtils::addChild(doc, node, "RateCutoff", static_cast<int>(rateCutoff_));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                gearingDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate", spreadDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    if (localCapFloor_)
        XMLUtils::addChild(doc, node, "LocalCapFloor", localCapFloor_);
    if (fixingSchedule_.hasData()) {
        auto tmp = fixingSchedule_.toXML(doc);
        XMLUtils::setNodeName(doc, tmp, "FixingSchedule");
        XMLUtils::appendNode(node, tmp);
    }
    if (resetSchedule_.hasData()) {
        auto tmp = resetSchedule_.toXML(doc);
        XMLUtils::setNodeName(doc, tmp, "ResetSchedule");
        XMLUtils::appendNode(node, tmp);
    }
    if (!historicalFixings_.empty()) {
        auto histFixings = XMLUtils::addChild(doc, node, "HistoricalFixings");
        for (const auto& [fixingDate, fixingValue] : historicalFixings_) {
            XMLUtils::addChild(doc, histFixings, "Fixing", to_string(fixingValue), "fixingDate", to_string(fixingDate));
        }
    }
    return node;
}

void CPILegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = XMLUtils::getChildValue(node, "Index", true);
    startDate_ = XMLUtils::getChildValue(node, "StartDate", false);
    indices_.insert(index_);
    baseCPI_ = XMLUtils::getChildValueAsDouble(node, "BaseCPI", false, QuantLib::Null<QuantLib::Real>());
    observationLag_ = XMLUtils::getChildValue(node, "ObservationLag", false, "");
    // for backwards compatibility only
    if (auto c = XMLUtils::getChildNode(node, "Interpolated")) {
        QL_REQUIRE(XMLUtils::getChildNode(node, "Interpolation") == nullptr,
                   "can not have both Interpolated and Interpolation node in CPILegData");
        interpolation_ = parseBool(XMLUtils::getNodeValue(c)) ? "Linear" : "Flat";
    } else {
        interpolation_ = XMLUtils::getChildValue(node, "Interpolation", false, "");
    }
    XMLNode* subNomNode = XMLUtils::getChildNode(node, "SubtractInflationNotional");
    if (subNomNode)
        subtractInflationNominal_ = XMLUtils::getChildValueAsBool(node, "SubtractInflationNotional", true);
    else
        subtractInflationNominal_ = false;

    XMLNode* subNomCpnsNode = XMLUtils::getChildNode(node, "SubtractInflationNotionalAllCoupons");
    if (subNomCpnsNode)
        subtractInflationNominalCoupons_ =
            XMLUtils::getChildValueAsBool(node, "SubtractInflationNotionalAllCoupons", true);
    else
        subtractInflationNominalCoupons_ = false;

    rates_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Rates", "Rate", "startDate", rateDates_, &parseReal,
                                                             true);
    caps_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Caps", "Cap", "startDate", capDates_, &parseReal);
    floors_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Floors", "Floor", "startDate", floorDates_, &parseReal);

    finalFlowCap_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "FinalFlowCap")) {
        string v = XMLUtils::getNodeValue(n);
        if (!v.empty())
            finalFlowCap_ = parseReal(XMLUtils::getNodeValue(n));
    }

    finalFlowFloor_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(node, "FinalFlowFloor")) {
        string v = XMLUtils::getNodeValue(n);
        if (!v.empty())
            finalFlowFloor_ = parseReal(XMLUtils::getNodeValue(n));
    }

    if (XMLUtils::getChildNode(node, "NakedOption"))
        nakedOption_ = XMLUtils::getChildValueAsBool(node, "NakedOption", false);
    else
        nakedOption_ = false;
}

XMLNode* CPILegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    if (baseCPI_ != Null<Real>()) {
        XMLUtils::addChild(doc, node, "BaseCPI", baseCPI_);
    }
    XMLUtils::addChild(doc, node, "StartDate", startDate_);
    if (!observationLag_.empty()) {
        XMLUtils::addChild(doc, node, "ObservationLag", observationLag_);
    }
    if (!interpolation_.empty()) {
       XMLUtils::addChild(doc, node, "Interpolation", interpolation_);
    }
    XMLUtils::addChild(doc, node, "SubtractInflationNotional", subtractInflationNominal_);
    XMLUtils::addChild(doc, node, "SubtractInflationNotionalAllCoupons", subtractInflationNominalCoupons_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    if (finalFlowCap_ != Null<Real>())
        XMLUtils::addChild(doc, node, "FinalFlowCap", finalFlowCap_);
    if (finalFlowFloor_ != Null<Real>())
        XMLUtils::addChild(doc, node, "FinalFlowFloor", finalFlowFloor_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    return node;
}

void YoYLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = XMLUtils::getChildValue(node, "Index", true);
    indices_.insert(index_);
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays", true);
    observationLag_ = XMLUtils::getChildValue(node, "ObservationLag", false, "");
    gearings_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Gearings", "Gearing", "startDate", gearingDates_,
                                                                &parseReal);
    spreads_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Spreads", "Spread", "startDate", spreadDates_,
                                                               &parseReal);
    caps_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Caps", "Cap", "startDate", capDates_, &parseReal);
    floors_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Floors", "Floor", "startDate", floorDates_, &parseReal);
    if (XMLUtils::getChildNode(node, "NakedOption"))
        nakedOption_ = XMLUtils::getChildValueAsBool(node, "NakedOption", false);
    else
        nakedOption_ = false;
    if (XMLUtils::getChildNode(node, "AddInflationNotional"))
        addInflationNotional_ = XMLUtils::getChildValueAsBool(node, "AddInflationNotional", false);
    else
        addInflationNotional_ = false;
    if (XMLUtils::getChildNode(node, "IrregularYoY"))
        irregularYoY_ = XMLUtils::getChildValueAsBool(node, "IrregularYoY", false);
    else
        irregularYoY_ = false;
}

XMLNode* YoYLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", index_);
    if (!observationLag_.empty()) {
        XMLUtils::addChild(doc, node, "ObservationLag", observationLag_);
    }
    XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                gearingDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate", spreadDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    XMLUtils::addChild(doc, node, "AddInflationNotional", addInflationNotional_);
    XMLUtils::addChild(doc, node, "IrregularYoY", irregularYoY_);
    return node;
}

XMLNode* CMSLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", swapIndex_);
    XMLUtils::addChild(doc, node, "IsInArrears", isInArrears_);
    if (fixingDays_ != Null<Size>())
        XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                gearingDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate", spreadDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    return node;
}

void CMSLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    swapIndex_ = XMLUtils::getChildValue(node, "Index", true);
    indices_.insert(swapIndex_);
    // These are all optional
    spreads_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Spreads", "Spread", "startDate", spreadDates_,
                                                               &parseReal);
    XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears");
    if (arrNode)
        isInArrears_ = XMLUtils::getChildValueAsBool(node, "IsInArrears", true);
    else
        isInArrears_ = false; // default to fixing-in-advance
    if (auto n = XMLUtils::getChildNode(node, "FixingDays"))
        fixingDays_ = parseInteger(XMLUtils::getNodeValue(n));
    else
        fixingDays_ = Null<Size>();
    caps_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Caps", "Cap", "startDate", capDates_, &parseReal);
    floors_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Floors", "Floor", "startDate", floorDates_, &parseReal);
    gearings_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Gearings", "Gearing", "startDate", gearingDates_,
                                                                &parseReal);
    if (XMLUtils::getChildNode(node, "NakedOption"))
        nakedOption_ = XMLUtils::getChildValueAsBool(node, "NakedOption", false);
    else
        nakedOption_ = false;
}

XMLNode* CMBLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", genericBond_);
    XMLUtils::addChild(doc, node, "IsInArrears", isInArrears_);
    XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                gearingDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate", spreadDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    XMLUtils::addChild(doc, node, "CreditRisk", hasCreditRisk_);
    return node;
}

void CMBLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    genericBond_ = XMLUtils::getChildValue(node, "Index", true);
    //indices_.insert(swapIndex_);
    // These are all optional
    spreads_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Spreads", "Spread", "startDate", spreadDates_,
                                                               &parseReal);
    XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears");
    if (arrNode)
        isInArrears_ = XMLUtils::getChildValueAsBool(node, "IsInArrears", true);
    else
        isInArrears_ = false; // default to fixing-in-advance
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays", true);
    caps_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Caps", "Cap", "startDate", capDates_, &parseReal);
    floors_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Floors", "Floor", "startDate", floorDates_, &parseReal);
    gearings_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Gearings", "Gearing", "startDate", gearingDates_,
                                                                &parseReal);
    if (XMLUtils::getChildNode(node, "NakedOption"))
        nakedOption_ = XMLUtils::getChildValueAsBool(node, "NakedOption", false);
    else
        nakedOption_ = false;

    if (XMLUtils::getChildNode(node, "CreditRisk"))
        hasCreditRisk_ = XMLUtils::getChildValueAsBool(node, "CreditRisk", false);
    else
        hasCreditRisk_ = true;
}

XMLNode* DigitalCMSLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::appendNode(node, underlying_->toXML(doc));

    if (callStrikes_.size() > 0) {
        XMLUtils::addChild(doc, node, "CallPosition", to_string(callPosition_));
        XMLUtils::addChild(doc, node, "IsCallATMIncluded", isCallATMIncluded_);
        XMLUtils::addChildren(doc, node, "CallStrikes", "Strike", callStrikes_);
        XMLUtils::addChildren(doc, node, "CallPayoffs", "Payoff", callPayoffs_);
    }

    if (putStrikes_.size() > 0) {
        XMLUtils::addChild(doc, node, "PutPosition", to_string(putPosition_));
        XMLUtils::addChild(doc, node, "IsPutATMIncluded", isPutATMIncluded_);
        XMLUtils::addChildren(doc, node, "PutStrikes", "Strike", putStrikes_);
        XMLUtils::addChildren(doc, node, "PutPayoffs", "Payoff", putPayoffs_);
    }

    return node;
}

void DigitalCMSLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());

    XMLNode* underlyingNode = XMLUtils::getChildNode(node, "CMSLegData");
    underlying_ = QuantLib::ext::make_shared<CMSLegData>();
    underlying_->fromXML(underlyingNode);
    indices_ = underlying_->indices();

    callStrikes_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "CallStrikes", "Strike", "startDate",
                                                                   callStrikeDates_, &parseReal);
    if (callStrikes_.size() > 0) {
        string cp = XMLUtils::getChildValue(node, "CallPosition", true);
        callPosition_ = parsePositionType(cp);
        isCallATMIncluded_ = XMLUtils::getChildValueAsBool(node, "IsCallATMIncluded", true);
        callPayoffs_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "CallPayoffs", "Payoff", "startDate",
                                                                       callPayoffDates_, &parseReal);
    }

    putStrikes_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "PutStrikes", "Strike", "startDate",
                                                                  putStrikeDates_, &parseReal);
    if (putStrikes_.size() > 0) {
        string pp = XMLUtils::getChildValue(node, "PutPosition", true);
        putPosition_ = parsePositionType(pp);
        isPutATMIncluded_ = XMLUtils::getChildValueAsBool(node, "IsPutATMIncluded", true);
        putPayoffs_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "PutPayoffs", "Payoff", "startDate",
                                                                      putPayoffDates_, &parseReal);
    }
}

XMLNode* CMSSpreadLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index1", swapIndex1_);
    XMLUtils::addChild(doc, node, "Index2", swapIndex2_);
    XMLUtils::addChild(doc, node, "IsInArrears", isInArrears_);
    if (fixingDays_ != Null<Size>())
        XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate", spreadDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                gearingDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    return node;
}

void CMSSpreadLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    swapIndex1_ = XMLUtils::getChildValue(node, "Index1", true);
    swapIndex2_ = XMLUtils::getChildValue(node, "Index2", true);
    indices_.insert(swapIndex1_);
    indices_.insert(swapIndex2_);
    // These are all optional
    spreads_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Spreads", "Spread", "startDate", spreadDates_,
                                                               &parseReal);
    XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears");
    if (arrNode)
        isInArrears_ = XMLUtils::getChildValueAsBool(node, "IsInArrears", true);
    else
        isInArrears_ = false; // default to fixing-in-advance
    if (auto n = XMLUtils::getChildNode(node, "FixingDays"))
        fixingDays_ = parseInteger(XMLUtils::getNodeValue(n));
    else
        fixingDays_ = Null<Size>();
    caps_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Caps", "Cap", "startDate", capDates_, &parseReal);
    floors_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Floors", "Floor", "startDate", floorDates_, &parseReal);
    gearings_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Gearings", "Gearing", "startDate", gearingDates_,
                                                                &parseReal);
    if (XMLUtils::getChildNode(node, "NakedOption"))
        nakedOption_ = XMLUtils::getChildValueAsBool(node, "NakedOption", false);
    else
        nakedOption_ = false;
}

XMLNode* DigitalCMSSpreadLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::appendNode(node, underlying_->toXML(doc));

    if (callStrikes_.size() > 0) {
        XMLUtils::addChild(doc, node, "CallPosition", to_string(callPosition_));
        XMLUtils::addChild(doc, node, "IsCallATMIncluded", isCallATMIncluded_);
        XMLUtils::addChildren(doc, node, "CallStrikes", "Strike", callStrikes_);
        XMLUtils::addChildren(doc, node, "CallPayoffs", "Payoff", callPayoffs_);
    }

    if (putStrikes_.size() > 0) {
        XMLUtils::addChild(doc, node, "PutPosition", to_string(putPosition_));
        XMLUtils::addChild(doc, node, "IsPutATMIncluded", isPutATMIncluded_);
        XMLUtils::addChildren(doc, node, "PutStrikes", "Strike", putStrikes_);
        XMLUtils::addChildren(doc, node, "PutPayoffs", "Payoff", putPayoffs_);
    }

    return node;
}

void DigitalCMSSpreadLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());

    XMLNode* underlyingNode = XMLUtils::getChildNode(node, "CMSSpreadLegData");
    underlying_ = QuantLib::ext::make_shared<CMSSpreadLegData>();
    underlying_->fromXML(underlyingNode);
    indices_ = underlying_->indices();

    callStrikes_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "CallStrikes", "Strike", "startDate",
                                                                   callStrikeDates_, &parseReal);
    if (callStrikes_.size() > 0) {
        string cp = XMLUtils::getChildValue(node, "CallPosition", true);
        callPosition_ = parsePositionType(cp);
        isCallATMIncluded_ = XMLUtils::getChildValueAsBool(node, "IsCallATMIncluded", true);
        callPayoffs_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "CallPayoffs", "Payoff", "startDate",
                                                                       callPayoffDates_, &parseReal);
    }

    putStrikes_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "PutStrikes", "Strike", "startDate",
                                                                  putStrikeDates_, &parseReal);
    if (putStrikes_.size() > 0) {
        string pp = XMLUtils::getChildValue(node, "PutPosition", true);
        putPosition_ = parsePositionType(pp);
        isPutATMIncluded_ = XMLUtils::getChildValueAsBool(node, "IsPutATMIncluded", true);
        putPayoffs_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "PutPayoffs", "Payoff", "startDate",
                                                                      putPayoffDates_, &parseReal);
    }
}

void EquityLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    returnType_ = parseEquityReturnType(XMLUtils::getChildValue(node, "ReturnType"));
    if (returnType_ == EquityReturnType::Total && XMLUtils::getChildNode(node, "DividendFactor"))
        dividendFactor_ = XMLUtils::getChildValueAsDouble(node, "DividendFactor", true);
    else
        dividendFactor_ = 1.0;
    XMLNode* utmp = XMLUtils::getChildNode(node, "Underlying");
    if (!utmp)
        utmp = XMLUtils::getChildNode(node, "Name");
    equityUnderlying_.fromXML(utmp);
    indices_.insert("EQ-" + eqName());
    if (XMLUtils::getChildNode(node, "InitialPrice"))
        initialPrice_ = XMLUtils::getChildValueAsDouble(node, "InitialPrice");
    else
        initialPrice_ = Null<Real>();
    initialPriceCurrency_ = XMLUtils::getChildValue(node, "InitialPriceCurrency", false);
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays");
    XMLNode* tmp = XMLUtils::getChildNode(node, "ValuationSchedule");
    if (tmp)
        valuationSchedule_.fromXML(tmp);
    if (XMLUtils::getChildNode(node, "NotionalReset"))
        notionalReset_ = XMLUtils::getChildValueAsBool(node, "NotionalReset");
    else
        notionalReset_ = true;

    XMLNode* fxt = XMLUtils::getChildNode(node, "FXTerms");
    if (fxt) {
        eqCurrency_ = XMLUtils::getChildValue(fxt, "EquityCurrency", false);
        fxIndex_ = XMLUtils::getChildValue(fxt, "FXIndex", true);
        if (XMLUtils::getChildNode(fxt, "FXIndexFixingDays")) {
            WLOG("EquityLegData::fromXML, node FXIndexFixingDays has been deprecated, fixing days are "
                 "taken from conventions.");
        }
        if (XMLUtils::getChildNode(fxt, "FXIndexCalendar")) {
            WLOG("EquityLegData::fromXML, node FXIndexCalendar has been deprecated, fixing calendar is "
                 "taken from conventions.");
        }
        indices_.insert(fxIndex_);
    }

    if (XMLNode* qty = XMLUtils::getChildNode(node, "Quantity"))
        quantity_ = parseReal(XMLUtils::getNodeValue(qty));
    else
        quantity_ = Null<Real>();
}

XMLNode* EquityLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    if (quantity_ != Null<Real>())
        XMLUtils::addChild(doc, node, "Quantity", quantity_);

    XMLUtils::addChild(doc, node, "ReturnType", to_string(returnType_));
    if (returnType_ == EquityReturnType::Total)
        XMLUtils::addChild(doc, node, "DividendFactor", dividendFactor_);

    XMLUtils::appendNode(node, equityUnderlying_.toXML(doc));
    if (initialPrice_ != Null<Real>())
        XMLUtils::addChild(doc, node, "InitialPrice", initialPrice_);
    if (!initialPriceCurrency_.empty())
        XMLUtils::addChild(doc, node, "InitialPriceCurrency", initialPriceCurrency_);
    XMLUtils::addChild(doc, node, "NotionalReset", notionalReset_);

    if (valuationSchedule_.hasData()) {
        XMLNode* schedNode = valuationSchedule_.toXML(doc);
        XMLUtils::setNodeName(doc, schedNode, "ValuationSchedule");
        XMLUtils::appendNode(node, schedNode);
    } else {
        XMLUtils::addChild(doc, node, "FixingDays", static_cast<Integer>(fixingDays_));
    }

    if (fxIndex_ != "") {
        XMLNode* fxNode = doc.allocNode("FXTerms");
        XMLUtils::addChild(doc, fxNode, "EquityCurrency", eqCurrency_);
        XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex_);
        XMLUtils::appendNode(node, fxNode);
    }
    return node;
}

void AmortizationData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "AmortizationData");
    type_ = XMLUtils::getChildValue(node, "Type");
    value_ = XMLUtils::getChildValueAsDouble(node, "Value", false, Null<Real>());
    startDate_ = XMLUtils::getChildValue(node, "StartDate", false);
    endDate_ = XMLUtils::getChildValue(node, "EndDate", false);
    frequency_ = XMLUtils::getChildValue(node, "Frequency", false);
    underflow_ = XMLUtils::getChildValueAsBool(node, "Underflow", false, false);
    initialized_ = true;
    validate();
}

XMLNode* AmortizationData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("AmortizationData");
    XMLUtils::addChild(doc, node, "Type", type_);
    if (value_ != Null<Real>())
        XMLUtils::addChild(doc, node, "Value", value_);
    if (!startDate_.empty())
        XMLUtils::addChild(doc, node, "StartDate", startDate_);
    if (!endDate_.empty())
        XMLUtils::addChild(doc, node, "EndDate", endDate_);
    if (!frequency_.empty())
        XMLUtils::addChild(doc, node, "Frequency", frequency_);
    XMLUtils::addChild(doc, node, "Underflow", underflow_);
    return node;
}

void AmortizationData::validate() const {
    QL_REQUIRE(type_ == "LinearToMaturity" || value_ != Null<Real>(), "AmortizationData requires Value");
    QL_REQUIRE(type_ == "LinearToMaturity" || value_ != Null<Real>(), "AmortizationData requires Underflow");
}

LegData::LegData(const QuantLib::ext::shared_ptr<LegAdditionalData>& concreteLegData, bool isPayer, const string& currency,
                 const ScheduleData& scheduleData, const string& dayCounter, const std::vector<double>& notionals,
                 const std::vector<string>& notionalDates, const string& paymentConvention,
                 const bool notionalInitialExchange, const bool notionalFinalExchange,
                 const bool notionalAmortizingExchange, const bool isNotResetXCCY, const string& foreignCurrency,
                 const double foreignAmount, const string& fxIndex,
                 const std::vector<AmortizationData>& amortizationData, const string& paymentLag,
                 const string& notionalPaymentLag, const string& paymentCalendar, const vector<string>& paymentDates,
                 const std::vector<Indexing>& indexing, const bool indexingFromAssetLeg,
                 const string& lastPeriodDayCounter)
    : concreteLegData_(concreteLegData), isPayer_(isPayer), currency_(currency), schedule_(scheduleData),
      dayCounter_(dayCounter), notionals_(notionals), notionalDates_(notionalDates),
      paymentConvention_(paymentConvention), notionalInitialExchange_(notionalInitialExchange),
      notionalFinalExchange_(notionalFinalExchange), notionalAmortizingExchange_(notionalAmortizingExchange),
      isNotResetXCCY_(isNotResetXCCY), foreignCurrency_(foreignCurrency), foreignAmount_(foreignAmount),
      fxIndex_(fxIndex), amortizationData_(amortizationData), paymentLag_(paymentLag),
      notionalPaymentLag_(notionalPaymentLag), paymentCalendar_(paymentCalendar), paymentDates_(paymentDates),
      indexing_(indexing), indexingFromAssetLeg_(indexingFromAssetLeg), lastPeriodDayCounter_(lastPeriodDayCounter) {

    indices_ = concreteLegData_->indices();

    if (!fxIndex_.empty())
        indices_.insert(fxIndex_);

    for (auto const& i : indexing)
        if (i.hasData())
            indices_.insert(i.index());
}

void LegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "LegData");
    string legType = XMLUtils::getChildValue(node, "LegType", true);
    isPayer_ = XMLUtils::getChildValueAsBool(node, "Payer");
    currency_ = XMLUtils::getChildValue(node, "Currency", false);
    dayCounter_ = XMLUtils::getChildValue(node, "DayCounter"); // optional
    paymentConvention_ = XMLUtils::getChildValue(node, "PaymentConvention");
    paymentLag_ = XMLUtils::getChildValue(node, "PaymentLag");
    notionalPaymentLag_ = XMLUtils::getChildValue(node, "NotionalPaymentLag");
    paymentCalendar_ = XMLUtils::getChildValue(node, "PaymentCalendar", false);
    // if not given, default of getChildValueAsBool is true, which fits our needs here
    notionals_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Notionals", "Notional", "startDate",
                                                                 notionalDates_, &parseReal);
    isNotResetXCCY_ = true;
    notionalInitialExchange_ = false;
    notionalFinalExchange_ = false;
    notionalAmortizingExchange_ = false;
    if (auto tmp = XMLUtils::getChildNode(node, "Notionals")) {
        XMLNode* fxResetNode = XMLUtils::getChildNode(tmp, "FXReset");
        if (fxResetNode) {
            isNotResetXCCY_ = false;
            foreignCurrency_ = XMLUtils::getChildValue(fxResetNode, "ForeignCurrency", true);
            foreignAmount_ = XMLUtils::getChildValueAsDouble(fxResetNode, "ForeignAmount", true);
            fxIndex_ = XMLUtils::getChildValue(fxResetNode, "FXIndex", true);
            indices_.insert(fxIndex_);
            if (XMLUtils::getChildNode(node, "FixingDays")) {
                WLOG("LegData::fromXML, node FixingDays has been deprecated, fixing days are "
                     "taken from conventions.");
            }
            if (XMLUtils::getChildNode(node, "FixingCalendar")) {
                WLOG("LegData::fromXML, node FixingCalendar has been deprecated, fixing calendar is "
                     "taken from conventions.");
            }
            // TODO add schedule
        }
        XMLNode* exchangeNode = XMLUtils::getChildNode(tmp, "Exchanges");
        if (exchangeNode) {

            notionalInitialExchange_ = XMLUtils::getChildValueAsBool(exchangeNode, "NotionalInitialExchange");
            notionalFinalExchange_ = XMLUtils::getChildValueAsBool(exchangeNode, "NotionalFinalExchange");
            if (XMLUtils::getChildNode(exchangeNode, "NotionalAmortizingExchange"))
                notionalAmortizingExchange_ = XMLUtils::getChildValueAsBool(exchangeNode, "NotionalAmortizingExchange");
        }
    }

    XMLNode* amortizationParentNode = XMLUtils::getChildNode(node, "Amortizations");
    if (amortizationParentNode) {
        auto adNodes = XMLUtils::getChildrenNodes(amortizationParentNode, "AmortizationData");
        for (auto const& a : adNodes) {
            amortizationData_.push_back(AmortizationData());
            amortizationData_.back().fromXML(a);
        }
    }

    if (auto tmp = XMLUtils::getChildNode(node, "ScheduleData"))
        schedule_.fromXML(tmp);

    paymentDates_ = XMLUtils::getChildrenValues(node, "PaymentDates", "PaymentDate", false);
    if (!paymentDates_.empty()) {
        WLOG("Usage of PaymentDates is deprecated, use PaymentSchedule instead.");
    }

    strictNotionalDates_ = XMLUtils::getChildValueAsBool(node, "StrictNotionalDates", false, false);

    if (auto tmp = XMLUtils::getChildNode(node, "PaymentSchedule")) {
        paymentSchedule_.fromXML(tmp);
        QL_REQUIRE(paymentDates_.empty(), "Both PaymentDates and PaymentSchedule is given. Remove one of them. "
                                          "PaymentDates is deprecated, so preferably use PaymentSchedule.");
    }

    if (auto tmp = XMLUtils::getChildNode(node, "Indexings")) {
        if (auto n = XMLUtils::getChildNode(tmp, "FromAssetLeg")) {
            indexingFromAssetLeg_ = parseBool(XMLUtils::getNodeValue(n));
        } else {
            indexingFromAssetLeg_ = false;
        }
        auto indexings = XMLUtils::getChildrenNodes(tmp, "Indexing");
        for (auto const& i : indexings) {
            indexing_.push_back(Indexing());
            indexing_.back().fromXML(i);
        }
    }

    lastPeriodDayCounter_ = XMLUtils::getChildValue(node, "LastPeriodDayCounter", false);

    concreteLegData_ = initialiseConcreteLegData(legType);
    concreteLegData_->fromXML(XMLUtils::getChildNode(node, concreteLegData_->legNodeName()));

    indices_.insert(concreteLegData_->indices().begin(), concreteLegData_->indices().end());
}

QuantLib::ext::shared_ptr<LegAdditionalData> LegData::initialiseConcreteLegData(const string& legType) {
    auto legData = LegDataFactory::instance().build(legType);
    QL_REQUIRE(legData, "Leg type " << legType << " has not been registered with the leg data factory.");
    return legData;
}

XMLNode* LegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("LegData");
    QL_REQUIRE(node, "Failed to create LegData node");
    XMLUtils::addChild(doc, node, "LegType", legType());
    XMLUtils::addChild(doc, node, "Payer", isPayer_);
    XMLUtils::addChild(doc, node, "Currency", currency_);
    if (paymentConvention_ != "")
        XMLUtils::addChild(doc, node, "PaymentConvention", paymentConvention_);
    if (!paymentLag_.empty())
        XMLUtils::addChild(doc, node, "PaymentLag", paymentLag_);
    if (!notionalPaymentLag_.empty())
        XMLUtils::addChild(doc, node, "NotionalPaymentLag", notionalPaymentLag_);
    if (!paymentCalendar_.empty())
        XMLUtils::addChild(doc, node, "PaymentCalendar", paymentCalendar_);
    if (dayCounter_ != "")
        XMLUtils::addChild(doc, node, "DayCounter", dayCounter_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Notionals", "Notional", notionals_, "startDate",
                                                notionalDates_);
    XMLNode* notionalsNodePtr = XMLUtils::getChildNode(node, "Notionals");

    if (!isNotResetXCCY_) {
        XMLNode* resetNode = doc.allocNode("FXReset");
        XMLUtils::addChild(doc, resetNode, "ForeignCurrency", foreignCurrency_);
        XMLUtils::addChild(doc, resetNode, "ForeignAmount", foreignAmount_);
        XMLUtils::addChild(doc, resetNode, "FXIndex", fxIndex_);
        XMLUtils::appendNode(notionalsNodePtr, resetNode);
    }

    XMLNode* exchangeNode = doc.allocNode("Exchanges");
    XMLUtils::addChild(doc, exchangeNode, "NotionalInitialExchange", notionalInitialExchange_);
    XMLUtils::addChild(doc, exchangeNode, "NotionalFinalExchange", notionalFinalExchange_);
    XMLUtils::addChild(doc, exchangeNode, "NotionalAmortizingExchange", notionalAmortizingExchange_);
    XMLUtils::appendNode(notionalsNodePtr, exchangeNode);

    XMLUtils::appendNode(node, schedule_.toXML(doc));

    if (!paymentDates_.empty())
        XMLUtils::addChildren(doc, node, "PaymentDates", "PaymentDate", paymentDates_);

    if (!amortizationData_.empty()) {
        XMLNode* amortisationsParentNode = doc.allocNode("Amortizations");
        for (auto& amort : amortizationData_) {
            if (amort.initialized()) {
                XMLUtils::appendNode(amortisationsParentNode, amort.toXML(doc));
            }
        }
        XMLUtils::appendNode(node, amortisationsParentNode);
    }

    if (strictNotionalDates_) {
        XMLUtils::addChild(doc, node, "StrictNotionalDates", strictNotionalDates_);
    }

    if (paymentSchedule_.hasData()) {
        auto tmp = paymentSchedule_.toXML(doc);
        XMLUtils::setNodeName(doc, tmp, "PaymentSchedule");
        XMLUtils::appendNode(node, tmp);
    }

    if (!indexing_.empty() || indexingFromAssetLeg_) {
        XMLNode* indexingsNode = doc.allocNode("Indexings");
        if (indexingFromAssetLeg_)
            XMLUtils::addChild(doc, indexingsNode, "FromAssetLeg", indexingFromAssetLeg_);
        for (auto& i : indexing_) {
            if (i.hasData())
                XMLUtils::appendNode(indexingsNode, i.toXML(doc));
        }
        XMLUtils::appendNode(node, indexingsNode);
    }

    if (!lastPeriodDayCounter_.empty())
        XMLUtils::addChild(doc, node, "LastPeriodDayCounter", lastPeriodDayCounter_);

    XMLUtils::appendNode(node, concreteLegData_->toXML(doc));
    return node;
}

// Functions
Leg makeSimpleLeg(const LegData& data) {
    QuantLib::ext::shared_ptr<CashflowData> cashflowData = QuantLib::ext::dynamic_pointer_cast<CashflowData>(data.concreteLegData());
    QL_REQUIRE(cashflowData, "Wrong LegType, expected CashFlow, got " << data.legType());

    const vector<double>& amounts = cashflowData->amounts();
    const vector<string>& dates = cashflowData->dates();
    QL_REQUIRE(amounts.size() == dates.size(), "Amounts / Date size mismatch in makeSimpleLeg."
                                                   << "Amounts:" << amounts.size() << ", Dates:" << dates.size());
    Leg leg;
    for (Size i = 0; i < dates.size(); i++) {
        Date d = parseDate(dates[i]);
        if (!data.paymentCalendar().empty() && !data.paymentConvention().empty())
            d = parseCalendar(data.paymentCalendar()).adjust(d, parseBusinessDayConvention(data.paymentConvention()));
        leg.push_back(QuantLib::ext::make_shared<SimpleCashFlow>(amounts[i], d));
    }
    return leg;
}

Leg makeFixedLeg(const LegData& data, const QuantLib::Date& openEndDateReplacement) {

    QuantLib::ext::shared_ptr<FixedLegData> fixedLegData = QuantLib::ext::dynamic_pointer_cast<FixedLegData>(data.concreteLegData());
    QL_REQUIRE(fixedLegData, "Wrong LegType, expected Fixed, got " << data.legType());

    // build schedules

    Schedule schedule;
    Schedule paymentSchedule;
    ScheduleBuilder scheduleBuilder;
    scheduleBuilder.add(schedule, data.schedule());
    scheduleBuilder.add(paymentSchedule, data.paymentSchedule());
    scheduleBuilder.makeSchedules(openEndDateReplacement);

    // Get explicit payment dates, if given

    vector<Date> paymentDates;

    if (!paymentSchedule.empty()) {
        paymentDates = paymentSchedule.dates();
    } else if (!data.paymentDates().empty()) {
        BusinessDayConvention paymentDatesConvention =
            data.paymentConvention().empty() ? Unadjusted : parseBusinessDayConvention(data.paymentConvention());
        Calendar paymentDatesCalendar =
            data.paymentCalendar().empty() ? NullCalendar() : parseCalendar(data.paymentCalendar());
        paymentDates = parseVectorOfValues<Date>(data.paymentDates(), &parseDate);
        for (Size i = 0; i < paymentDates.size(); i++)
            paymentDates[i] = paymentDatesCalendar.adjust(paymentDates[i], paymentDatesConvention);
    }

    // set payment calendar

    Calendar paymentCalendar;
    if (!data.paymentCalendar().empty())
        paymentCalendar = parseCalendar(data.paymentCalendar());
    else if (!paymentSchedule.calendar().empty())
        paymentCalendar = paymentSchedule.calendar();
    else if (!schedule.calendar().empty())
        paymentCalendar = schedule.calendar();

    // set day counter and bdc

    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());

    // build standard schedules (for non-strict notional dates)

    vector<double> rates = buildScheduledVector(fixedLegData->rates(), fixedLegData->rateDates(), schedule);
    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);

    // parse payment lag

    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());

    // apply amortization

    applyAmortization(notionals, data, schedule, true, rates);

    // build leg

    if (!data.strictNotionalDates()) {

        // no strict notional dates

        Leg leg = FixedRateLeg(schedule)
                      .withNotionals(notionals)
                      .withCouponRates(rates, dc)
                      .withPaymentAdjustment(bdc)
                      .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                      .withPaymentCalendar(paymentCalendar)
                      .withLastPeriodDayCounter(data.lastPeriodDayCounter().empty()
                                                    ? DayCounter()
                                                    : parseDayCounter(data.lastPeriodDayCounter()))
                      .withPaymentDates(paymentDates);
        return leg;

    } else {

        // strict notional dates

        std::vector<Date> notionalDatesAsDates;
        std::vector<Date> rateDatesAsDates;

        for (auto const& d : data.notionalDates()) {
            if (!d.empty())
                notionalDatesAsDates.push_back(parseDate(d));
        }

        for (auto const& d : fixedLegData->rateDates()) {
            if (!d.empty())
                rateDatesAsDates.push_back(parseDate(d));
        }

        return makeNonStandardFixedLeg(schedule.dates(), paymentDates, data.notionals(), notionalDatesAsDates,
                                       fixedLegData->rates(), rateDatesAsDates, data.strictNotionalDates(), dc,
                                       paymentCalendar, bdc, boost::apply_visitor(PaymentLagPeriod(), paymentLag));
    }
}

Leg makeZCFixedLeg(const LegData& data, const QuantLib::Date& openEndDateReplacement) {
    QuantLib::ext::shared_ptr<ZeroCouponFixedLegData> zcFixedLegData =
        QuantLib::ext::dynamic_pointer_cast<ZeroCouponFixedLegData>(data.concreteLegData());
    QL_REQUIRE(zcFixedLegData, "Wrong LegType, expected Zero Coupon Fixed, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);

    Calendar paymentCalendar;
    if (data.paymentCalendar().empty())
        paymentCalendar = schedule.calendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());

    BusinessDayConvention payConvention = parseBusinessDayConvention(data.paymentConvention());
    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());
    Natural paymentLagDays = boost::apply_visitor(PaymentLagInteger(), paymentLag);

    DayCounter dc = parseDayCounter(data.dayCounter());

    Size numNotionals = data.notionals().size();
    Size numRates = zcFixedLegData->rates().size();
    Size numDates = schedule.size();

    QL_REQUIRE(numDates >= 2, "Incorrect number of schedule dates entered, expected at least 2, got " << numDates);
    QL_REQUIRE(numNotionals >= 1,
               "Incorrect number of notional values entered, expected at least1, got " << numNotionals);
    QL_REQUIRE(numRates >= 1, "Incorrect number of rate values entered, expected at least 1, got " << numRates);

    vector<Date> dates = schedule.dates();

    vector<double> rates = buildScheduledVector(zcFixedLegData->rates(), zcFixedLegData->rateDates(), schedule);
    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);

    Compounding comp = parseCompounding(zcFixedLegData->compounding());
    QL_REQUIRE(comp == QuantLib::Compounded || comp == QuantLib::Simple,
               "Compounding method " << zcFixedLegData->compounding() << " not supported");

    Leg leg;
    vector<Date> cpnDates;
    cpnDates.push_back(dates.front());

    for (Size i = 0; i < numDates - 1; i++) {

        double currentNotional = i < notionals.size() ? notionals[i] : notionals.back();
        double currentRate = i < rates.size() ? rates[i] : rates.back();
        cpnDates.push_back(dates[i + 1]);
        Date paymentDate = paymentCalendar.advance(dates[i + 1], paymentLagDays, Days, payConvention);
        leg.push_back(QuantLib::ext::make_shared<ZeroFixedCoupon>(paymentDate, currentNotional, currentRate, dc, cpnDates, comp,
                                                          zcFixedLegData->subtractNotional()));
    }
    return leg;
}

Leg makeIborLeg(const LegData& data, const QuantLib::ext::shared_ptr<IborIndex>& index,
                const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer,
                const QuantLib::Date& openEndDateReplacement) {

    QuantLib::ext::shared_ptr<FloatingLegData> floatData = QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << data.legType());

    // build schedules

    Schedule schedule;
    Schedule fixingSchedule;
    Schedule resetSchedule;
    Schedule paymentSchedule;
    ScheduleBuilder scheduleBuilder;
    scheduleBuilder.add(schedule, data.schedule());
    scheduleBuilder.add(fixingSchedule, floatData->fixingSchedule());
    scheduleBuilder.add(resetSchedule, floatData->resetSchedule());
    scheduleBuilder.add(paymentSchedule, data.paymentSchedule());
    scheduleBuilder.makeSchedules(openEndDateReplacement);

    // Get explicit payment dates, if given

    vector<Date> paymentDates;

    if (!paymentSchedule.empty()) {
        paymentDates = paymentSchedule.dates();
    } else if (!data.paymentDates().empty()) {
        BusinessDayConvention paymentDatesConvention =
            data.paymentConvention().empty() ? Unadjusted : parseBusinessDayConvention(data.paymentConvention());
        Calendar paymentDatesCalendar =
            data.paymentCalendar().empty() ? NullCalendar() : parseCalendar(data.paymentCalendar());
        paymentDates = parseVectorOfValues<Date>(data.paymentDates(), &parseDate);
        for (Size i = 0; i < paymentDates.size(); i++)
            paymentDates[i] = paymentDatesCalendar.adjust(paymentDates[i], paymentDatesConvention);
    }

    // set payment calendar

    Calendar paymentCalendar;
    if (!data.paymentCalendar().empty())
        paymentCalendar = parseCalendar(data.paymentCalendar());
    else if (!paymentSchedule.calendar().empty())
        paymentCalendar = paymentSchedule.calendar();
    else if (!schedule.calendar().empty())
        paymentCalendar = schedule.calendar();

    // set day counter and bdc

    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());

    // flag whether caps / floors are present

    bool hasCapsFloors = floatData->caps().size() > 0 || floatData->floors().size() > 0;

    // build standard schedules (for non-strict notional dates)

    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);
    vector<double> spreads =
        buildScheduledVectorNormalised(floatData->spreads(), floatData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        buildScheduledVectorNormalised(floatData->gearings(), floatData->gearingDates(), schedule, 1.0);

    // set fixing days and in arrears flag

    Size fixingDays = floatData->fixingDays() == Null<Size>() ? index->fixingDays() : floatData->fixingDays();
    bool isInArrears = floatData->isInArrears() ? *floatData->isInArrears() : false;

    // apply amortization

    applyAmortization(notionals, data, schedule, true);

    // handle float annuity, which is not done in applyAmortization, for this we can only have one block

    if (!data.amortizationData().empty()) {
        AmortizationType amortizationType = parseAmortizationType(data.amortizationData().front().type());
        if (amortizationType == AmortizationType::Annuity) {
            LOG("Build floating annuity notional schedule");
            QL_REQUIRE(data.amortizationData().size() == 1,
                       "Can have one AmortizationData block only for floating leg annuities");
            QL_REQUIRE(!hasCapsFloors, "Caps/Floors not supported in floating annuity coupons");
            QL_REQUIRE(floatData->gearings().size() == 0, "Gearings not supported in floating annuity coupons");
            DayCounter dc = index->dayCounter();
            Date startDate = data.amortizationData().front().startDate().empty()
                                 ? Date::minDate()
                                 : parseDate(data.amortizationData().front().startDate());
            double annuity = data.amortizationData().front().value();
            bool underflow = data.amortizationData().front().underflow();
            vector<QuantLib::ext::shared_ptr<Coupon>> coupons;
            for (Size i = 0; i < schedule.size() - 1; i++) {
                Date paymentDate = paymentCalendar.adjust(schedule[i + 1], bdc);
                if (schedule[i] < startDate || i == 0) {
                    QuantLib::ext::shared_ptr<FloatingRateCoupon> coupon;
                    if (!floatData->hasSubPeriods()) {
                        coupon = QuantLib::ext::make_shared<IborCoupon>(paymentDate, notionals[i], schedule[i], schedule[i + 1],
                                                                fixingDays, index, gearings[i], spreads[i], Date(),
                                                                Date(), dc, isInArrears);
                        coupon->setPricer(QuantLib::ext::make_shared<BlackIborCouponPricer>());
                    } else {
                        coupon = QuantLib::ext::make_shared<QuantExt::SubPeriodsCoupon1>(
                            paymentDate, notionals[i], schedule[i], schedule[i + 1], index,
                            floatData->isAveraged() ? QuantExt::SubPeriodsCoupon1::Averaging
                                                    : QuantExt::SubPeriodsCoupon1::Compounding,
                            index->businessDayConvention(), spreads[i], dc, floatData->includeSpread(), gearings[i]);
                        coupon->setPricer(QuantLib::ext::make_shared<QuantExt::SubPeriodsCouponPricer1>());
                    }
                    coupons.push_back(coupon);
                    LOG("FloatingAnnuityCoupon: " << i << " " << coupon->nominal() << " " << coupon->amount());
                } else {
                    QL_REQUIRE(coupons.size() > 0,
                               "FloatingAnnuityCoupon needs at least one predecessor, e.g. a plain IborCoupon");
                    LOG("FloatingAnnuityCoupon, previous nominal/coupon: " << i << " " << coupons.back()->nominal()
                                                                           << " " << coupons.back()->amount());
                    QuantLib::ext::shared_ptr<QuantExt::FloatingAnnuityCoupon> coupon =
                        QuantLib::ext::make_shared<QuantExt::FloatingAnnuityCoupon>(
                            annuity, underflow, coupons.back(), paymentDate, schedule[i], schedule[i + 1], fixingDays,
                            index, gearings[i], spreads[i], Date(), Date(), dc, isInArrears);
                    coupons.push_back(coupon);
                    LOG("FloatingAnnuityCoupon: " << i << " " << coupon->nominal() << " " << coupon->amount());
                }
            }
            Leg leg;
            for (Size i = 0; i < coupons.size(); i++)
                leg.push_back(coupons[i]);
            LOG("Floating annuity notional schedule done");
            return leg;
        }
    }

    // handle sub periods leg

    if (floatData->hasSubPeriods()) {
        QL_REQUIRE(floatData->caps().empty() && floatData->floors().empty(),
                   "SubPeriodsLegs does not support caps or floors");
        QL_REQUIRE(!isInArrears, "SubPeriodLegs do not support in arrears fixings");
        Leg leg = QuantExt::SubPeriodsLeg1(schedule, index)
                      .withNotionals(notionals)
                      .withPaymentDayCounter(dc)
                      .withPaymentAdjustment(bdc)
                      .withGearings(gearings)
                      .withSpreads(spreads)
                      .withType(floatData->isAveraged() ? QuantExt::SubPeriodsCoupon1::Averaging
                                                        : QuantExt::SubPeriodsCoupon1::Compounding)
                      .includeSpread(floatData->includeSpread());
        QuantExt::setCouponPricer(leg, QuantLib::ext::make_shared<QuantExt::SubPeriodsCouponPricer1>());
        return leg;
    }

    // parse payment lag

    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());

    // handle ibor leg

    Leg tmpLeg;
    bool isNonStandard;

    if (!data.strictNotionalDates() && fixingSchedule.empty() && resetSchedule.empty()) {

        // no strict notional dates, no fixing or reset schedule

        IborLeg iborLeg = IborLeg(schedule, index)
                              .withNotionals(notionals)
                              .withSpreads(spreads)
                              .withPaymentCalendar(paymentCalendar)
                              .withPaymentDayCounter(dc)
                              .withPaymentAdjustment(bdc)
                              .withFixingDays(fixingDays)
                              .inArrears(isInArrears)
                              .withGearings(gearings)
                              .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                              .withPaymentDates(paymentDates);
        if (floatData->caps().size() > 0)
            iborLeg.withCaps(buildScheduledVector(floatData->caps(), floatData->capDates(), schedule));
        if (floatData->floors().size() > 0)
            iborLeg.withFloors(buildScheduledVector(floatData->floors(), floatData->floorDates(), schedule));

        tmpLeg = iborLeg;
        isNonStandard = false;

    } else {

        // strict notional dates, fixing or reset schedule present

        QL_REQUIRE(!hasCapsFloors, "Ibor leg with strict notional or reset dates, explicit fixing or reset schedule "
                                   "does not support cap / floors");

        std::vector<Date> notionalDatesAsDates;
        std::vector<Date> spreadDatesAsDates;
        std::vector<Date> gearingDatesAsDates;

        for (auto const& d : data.notionalDates()) {
            if (!d.empty())
                notionalDatesAsDates.push_back(parseDate(d));
        }

        for (auto const& d : floatData->spreadDates()) {
            if (!d.empty())
                spreadDatesAsDates.push_back(parseDate(d));
        }

        for (auto const& d : floatData->gearingDates()) {
            if (!d.empty())
                gearingDatesAsDates.push_back(parseDate(d));
        }

        tmpLeg = makeNonStandardIborLeg(index, schedule.dates(), paymentDates, fixingSchedule.dates(),
                                        resetSchedule.dates(), fixingDays, data.notionals(), notionalDatesAsDates,
                                        floatData->spreads(), spreadDatesAsDates, floatData->gearings(),
                                        gearingDatesAsDates, data.strictNotionalDates(), dc, paymentCalendar, bdc,
                                        boost::apply_visitor(PaymentLagPeriod(), paymentLag), isInArrears);

        isNonStandard = true;
    }

    if (attachPricer && (hasCapsFloors || isInArrears || isNonStandard)) {
        auto builder = engineFactory->builder("CapFlooredIborLeg");
        QL_REQUIRE(builder, "No builder found for CapFlooredIborLeg");
        auto cappedFlooredIborBuilder = QuantLib::ext::dynamic_pointer_cast<CapFlooredIborLegEngineBuilder>(builder);
        auto couponPricer = cappedFlooredIborBuilder->engine(IndexNameTranslator::instance().oreName(index->name()));
        QuantLib::setCouponPricer(tmpLeg, couponPricer);
    }

    // build naked option leg if required

    if (floatData->nakedOption()) {
        tmpLeg = StrippedCappedFlooredCouponLeg(tmpLeg);
    }

    // return the leg

    return tmpLeg;
}

Leg makeOISLeg(const LegData& data, const QuantLib::ext::shared_ptr<OvernightIndex>& index,
               const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer,
               const QuantLib::Date& openEndDateReplacement) {
    QuantLib::ext::shared_ptr<FloatingLegData> floatData = QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << data.legType());

    auto tmp = data.schedule();

    /* For schedules with 1D tenor, this ensures that the index calendar supersedes the calendar provided
     in the trade XML and using "following" rolling conventions to avoid differing calendars and subsequent
     "degenerate schedule" errors in the building of the overnight coupon value date schedules.
     Generally, "1D" is an unusual tenor to use (and often just an error in the input data), but we want to
     make sure that this edge case works technically. */
    for (auto& r : tmp.modifyRules()) {
        if (r.tenor() == "1D") {
            r.modifyCalendar() = to_string(index->fixingCalendar());
            r.modifyConvention() = "F";
            r.modifyTermConvention() = "F";
        }
    }

    Schedule schedule = makeSchedule(tmp, openEndDateReplacement);
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());

    // Get explicit payment dates which in most cases should be empty
    vector<Date> paymentDates;
    if (!data.paymentDates().empty()) {
        BusinessDayConvention paymentDatesConvention =
            data.paymentConvention().empty() ? Unadjusted : parseBusinessDayConvention(data.paymentConvention());
        Calendar paymentDatesCalendar =
            data.paymentCalendar().empty() ? NullCalendar() : parseCalendar(data.paymentCalendar());
        paymentDates = parseVectorOfValues<Date>(data.paymentDates(), &parseDate);
        for (Size i = 0; i < paymentDates.size(); i++)
            paymentDates[i] = paymentDatesCalendar.adjust(paymentDates[i], paymentDatesConvention);
    }

    // try to set the rate computation period based on the schedule tenor
    Period rateComputationPeriod = 0 * Days;
    if (!tmp.rules().empty() && !tmp.rules().front().tenor().empty())
        rateComputationPeriod = parsePeriod(tmp.rules().front().tenor());
    else if (!tmp.dates().empty() && !tmp.dates().front().tenor().empty())
        rateComputationPeriod = parsePeriod(tmp.dates().front().tenor());

    Calendar paymentCalendar;
    if (data.paymentCalendar().empty())
        paymentCalendar = index->fixingCalendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());

    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);
    vector<double> spreads =
        buildScheduledVectorNormalised(floatData->spreads(), floatData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        buildScheduledVectorNormalised(floatData->gearings(), floatData->gearingDates(), schedule, 1.0);
    bool isInArrears = floatData->isInArrears() ? *floatData->isInArrears() : true;

    applyAmortization(notionals, data, schedule, false);

    if (floatData->isAveraged()) {

        QuantLib::ext::shared_ptr<QuantExt::AverageONIndexedCouponPricer> couponPricer =
            QuantLib::ext::make_shared<QuantExt::AverageONIndexedCouponPricer>();

        QuantLib::ext::shared_ptr<QuantExt::CapFlooredAverageONIndexedCouponPricer> cfCouponPricer;
        if (attachPricer && (floatData->caps().size() > 0 || floatData->floors().size() > 0)) {
            auto builder = QuantLib::ext::dynamic_pointer_cast<CapFlooredAverageONIndexedCouponLegEngineBuilder>(
                engineFactory->builder("CapFlooredAverageONIndexedCouponLeg"));
            QL_REQUIRE(builder, "No builder found for CapFlooredAverageONIndexedCouponLeg");
            cfCouponPricer = QuantLib::ext::dynamic_pointer_cast<CapFlooredAverageONIndexedCouponPricer>(
                builder->engine(IndexNameTranslator::instance().oreName(index->name()), rateComputationPeriod));
            QL_REQUIRE(cfCouponPricer, "internal error, could not cast to CapFlooredAverageONIndexedCouponPricer");
        }

        QuantExt::AverageONLeg leg =
            QuantExt::AverageONLeg(schedule, index)
                .withNotionals(notionals)
                .withSpreads(spreads)
                .withPaymentCalendar(paymentCalendar)
                .withGearings(gearings)
                .withPaymentDayCounter(dc)
                .withPaymentAdjustment(bdc)
                .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                .withInArrears(isInArrears)
                .withLastRecentPeriod(floatData->lastRecentPeriod())
                .withLastRecentPeriodCalendar(floatData->lastRecentPeriodCalendar().empty()
                                                  ? Calendar()
                                                  : parseCalendar(floatData->lastRecentPeriodCalendar()))
                .withLookback(floatData->lookback())
                .withRateCutoff(floatData->rateCutoff() == Null<Size>() ? 0 : floatData->rateCutoff())
                .withFixingDays(floatData->fixingDays())
                .withCaps(buildScheduledVectorNormalised<Real>(floatData->caps(), floatData->capDates(), schedule,
                                                               Null<Real>()))
                .withFloors(buildScheduledVectorNormalised<Real>(floatData->floors(), floatData->capDates(), schedule,
                                                                 Null<Real>()))
                .withNakedOption(floatData->nakedOption())
                .includeSpreadInCapFloors(floatData->includeSpread())
                .withLocalCapFloor(floatData->localCapFloor())
                .withAverageONIndexedCouponPricer(couponPricer)
                .withCapFlooredAverageONIndexedCouponPricer(cfCouponPricer)
                .withTelescopicValueDates(floatData->telescopicValueDates())
                .withPaymentDates(paymentDates);
        return leg;

    } else {

        QuantLib::ext::shared_ptr<QuantExt::OvernightIndexedCouponPricer> couponPricer =
            QuantLib::ext::make_shared<QuantExt::OvernightIndexedCouponPricer>();

        QuantLib::ext::shared_ptr<QuantExt::CappedFlooredOvernightIndexedCouponPricer> cfCouponPricer;
        if (attachPricer && (floatData->caps().size() > 0 || floatData->floors().size() > 0)) {
            auto builder = QuantLib::ext::dynamic_pointer_cast<CapFlooredOvernightIndexedCouponLegEngineBuilder>(
                engineFactory->builder("CapFlooredOvernightIndexedCouponLeg"));
            QL_REQUIRE(builder, "No builder found for CapFlooredOvernightIndexedCouponLeg");
            cfCouponPricer = QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCouponPricer>(
                builder->engine(IndexNameTranslator::instance().oreName(index->name()), rateComputationPeriod));
            QL_REQUIRE(cfCouponPricer, "internal error, could not cast to CapFlooredAverageONIndexedCouponPricer");
        }

        Leg leg = QuantExt::OvernightLeg(schedule, index)
                      .withNotionals(notionals)
                      .withSpreads(spreads)
                      .withPaymentDayCounter(dc)
                      .withPaymentAdjustment(bdc)
                      .withPaymentCalendar(paymentCalendar)
                      .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                      .withGearings(gearings)
                      .withInArrears(isInArrears)
                      .withLastRecentPeriod(floatData->lastRecentPeriod())
                      .withLastRecentPeriodCalendar(floatData->lastRecentPeriodCalendar().empty()
                                                        ? Calendar()
                                                        : parseCalendar(floatData->lastRecentPeriodCalendar()))
                      .includeSpread(floatData->includeSpread())
                      .withLookback(floatData->lookback())
                      .withFixingDays(floatData->fixingDays())
                      .withRateCutoff(floatData->rateCutoff() == Null<Size>() ? 0 : floatData->rateCutoff())
                      .withCaps(buildScheduledVectorNormalised<Real>(floatData->caps(), floatData->capDates(), schedule,
                                                                     Null<Real>()))
                      .withFloors(buildScheduledVectorNormalised<Real>(floatData->floors(), floatData->capDates(),
                                                                       schedule, Null<Real>()))
                      .withNakedOption(floatData->nakedOption())
                      .withLocalCapFloor(floatData->localCapFloor())
                      .withOvernightIndexedCouponPricer(couponPricer)
                      .withCapFlooredOvernightIndexedCouponPricer(cfCouponPricer)
                      .withTelescopicValueDates(floatData->telescopicValueDates())
                      .withPaymentDates(paymentDates);

        // If the overnight index is BRL CDI, we need a special coupon pricer
        QuantLib::ext::shared_ptr<BRLCdi> brlCdiIndex = QuantLib::ext::dynamic_pointer_cast<BRLCdi>(index);
        if (brlCdiIndex)
            QuantExt::setCouponPricer(leg, QuantLib::ext::make_shared<BRLCdiCouponPricer>());

        return leg;
    }
}

Leg makeBMALeg(const LegData& data, const QuantLib::ext::shared_ptr<QuantExt::BMAIndexWrapper>& indexWrapper,
               const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const QuantLib::Date& openEndDateReplacement) {
    QuantLib::ext::shared_ptr<FloatingLegData> floatData = QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << data.legType());
    QuantLib::ext::shared_ptr<BMAIndex> index = indexWrapper->bma();

    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Calendar paymentCalendar;

    if (data.paymentCalendar().empty())
        paymentCalendar = schedule.calendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());

    vector<Real> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);
    vector<Real> spreads =
        buildScheduledVectorNormalised(floatData->spreads(), floatData->spreadDates(), schedule, 0.0);
    vector<Real> gearings =
        buildScheduledVectorNormalised(floatData->gearings(), floatData->gearingDates(), schedule, 1.0);
    vector<Real> caps =
        buildScheduledVectorNormalised(floatData->caps(), floatData->capDates(), schedule, (Real)Null<Real>());
    vector<Real> floors =
        buildScheduledVectorNormalised(floatData->floors(), floatData->floorDates(), schedule, (Real)Null<Real>());

    applyAmortization(notionals, data, schedule, false);

    Leg leg = AverageBMALeg(schedule, index)
                  .withNotionals(notionals)
                  .withSpreads(spreads)
                  .withPaymentDayCounter(dc)
                  .withPaymentCalendar(paymentCalendar)
                  .withPaymentAdjustment(bdc)
                  .withGearings(gearings);

    // try to set the rate computation period based on the schedule tenor

    Period rateComputationPeriod = 0 * Days;
    if (!data.schedule().rules().empty() && !data.schedule().rules().front().tenor().empty())
        rateComputationPeriod = parsePeriod(data.schedule().rules().front().tenor());
    else if (!data.schedule().dates().empty() && !data.schedule().dates().front().tenor().empty())
        rateComputationPeriod = parsePeriod(data.schedule().dates().front().tenor());

    // handle caps / floors

    if (floatData->caps().size() > 0 || floatData->floors().size() > 0) {

        QuantLib::ext::shared_ptr<QuantExt::CapFlooredAverageBMACouponPricer> cfCouponPricer;
        auto builder = QuantLib::ext::dynamic_pointer_cast<CapFlooredAverageBMACouponLegEngineBuilder>(
            engineFactory->builder("CapFlooredAverageBMACouponLeg"));
        QL_REQUIRE(builder, "No builder found for CapFlooredAverageBMACouponLeg");
        cfCouponPricer = QuantLib::ext::dynamic_pointer_cast<CapFlooredAverageBMACouponPricer>(
            builder->engine(IndexNameTranslator::instance().oreName(index->name()), rateComputationPeriod));
        QL_REQUIRE(cfCouponPricer, "internal error, could not cast to CapFlooredAverageBMACouponPricer");

        for (Size i = 0; i < leg.size(); ++i) {
            auto bmaCpn = QuantLib::ext::dynamic_pointer_cast<AverageBMACoupon>(leg[i]);
            QL_REQUIRE(bmaCpn, "makeBMALeg(): internal error, exepcted AverageBMACoupon. Contact dev.");
            if (caps[i] != Null<Real>() || floors[i] != Null<Real>()) {
                auto cpn = QuantLib::ext::make_shared<CappedFlooredAverageBMACoupon>(
                    bmaCpn, caps[i], floors[i], floatData->nakedOption(), floatData->includeSpread());
                cpn->setPricer(cfCouponPricer);
                leg[i] = cpn;
            }
        }
    }

    // return the leg

    return leg;
}

Leg makeNotionalLeg(const Leg& refLeg, const bool initNomFlow, const bool finalNomFlow, const bool amortNomFlow,
                    const Natural notionalPaymentLag,  const BusinessDayConvention paymentConvention,
                    const Calendar paymentCalendar, const bool excludeIndexing) {

    // Assumption - Cashflows on Input Leg are all coupons
    // This is the Leg to be populated
    Leg leg;

    // Initial Flow Amount
    if (initNomFlow) {
        auto coupon = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[0]);
        QL_REQUIRE(coupon, "makeNotionalLeg does not support non-coupon legs");
        double initFlowAmt = (excludeIndexing ? unpackIndexedCoupon(coupon) : coupon)->nominal();
        Date initDate = coupon->accrualStartDate();
        initDate = paymentCalendar.advance(initDate, notionalPaymentLag, Days, paymentConvention);
        if (initFlowAmt != 0)
            leg.push_back(QuantLib::ext::shared_ptr<CashFlow>(new SimpleCashFlow(-initFlowAmt, initDate)));
    }

    // Amortization Flows
    if (amortNomFlow) {
        for (Size i = 1; i < refLeg.size(); i++) {
            auto coupon = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[i]);
            QL_REQUIRE(coupon, "makeNotionalLeg does not support non-coupon legs");
            auto coupon2 = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[i - 1]);
            QL_REQUIRE(coupon, "makeNotionalLeg does not support non-coupon legs");
            Date flowDate = coupon->accrualStartDate();
            flowDate = paymentCalendar.advance(flowDate, notionalPaymentLag, Days, paymentConvention);
            Real initNom = (excludeIndexing ? unpackIndexedCoupon(coupon2) : coupon2)->nominal();
            Real newNom = (excludeIndexing ? unpackIndexedCoupon(coupon) : coupon)->nominal();
            Real flow = initNom - newNom;
            if (flow != 0)
                leg.push_back(QuantLib::ext::shared_ptr<CashFlow>(new SimpleCashFlow(flow, flowDate)));
        }
    }

    // Final Nominal Return at Maturity
    if (finalNomFlow) {
        auto coupon = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(refLeg.back());
        QL_REQUIRE(coupon, "makeNotionalLeg does not support non-coupon legs");
        double finalNomFlow = (excludeIndexing ? unpackIndexedCoupon(coupon) : coupon)->nominal();
        Date finalDate = coupon->accrualEndDate();
        finalDate = paymentCalendar.advance(finalDate, notionalPaymentLag, Days, paymentConvention);
        if (finalNomFlow != 0)
            leg.push_back(QuantLib::ext::shared_ptr<CashFlow>(new SimpleCashFlow(finalNomFlow, finalDate)));
    }

    return leg;
}

Leg makeCPILeg(const LegData& data, const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index,
               const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const QuantLib::Date& openEndDateReplacement) {

    QuantLib::ext::shared_ptr<CPILegData> cpiLegData = QuantLib::ext::dynamic_pointer_cast<CPILegData>(data.concreteLegData());
    QL_REQUIRE(cpiLegData, "Wrong LegType, expected CPI, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);
    DayCounter dc = parseDayCounter(data.dayCounter());
    Calendar paymentCalendar;

    if (data.paymentCalendar().empty())
        paymentCalendar = schedule.calendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());

    QuantLib::ext::shared_ptr<InflationSwapConvention> cpiSwapConvention = nullptr;

    auto inflationConventions = InstrumentConventions::instance().conventions()->get(
        cpiLegData->index() + "_INFLATIONSWAP", Convention::Type::InflationSwap);

    if (inflationConventions.first)
        cpiSwapConvention = QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(inflationConventions.second);

    Period observationLag;
    if (cpiLegData->observationLag().empty()) {
        QL_REQUIRE(cpiSwapConvention,
                   "observationLag is not specified in legData and couldn't find convention for "
                       << cpiLegData->index() << ". Please add field to trade xml or add convention");
        DLOG("Build CPI Leg and use observation lag from standard inflationswap convention");
        observationLag = cpiSwapConvention->observationLag();
    } else {
        observationLag = parsePeriod(cpiLegData->observationLag());
    }
    
    CPI::InterpolationType interpolationMethod;
    if (cpiLegData->interpolation().empty()) {
        QL_REQUIRE(cpiSwapConvention,
                   "Interpolation is not specified in legData and couldn't find convention for "
                       << cpiLegData->index() << ". Please add field to trade xml or add convention");
        DLOG("Build CPI Leg and use observation lag from standard inflationswap convention");
        interpolationMethod = cpiSwapConvention->interpolated() ? CPI::Linear : CPI::Flat;
    } else {
        interpolationMethod = parseObservationInterpolation(cpiLegData->interpolation());
    }

    vector<double> rates = buildScheduledVector(cpiLegData->rates(), cpiLegData->rateDates(), schedule);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    bool couponCap = cpiLegData->caps().size() > 0;
    bool couponFloor = cpiLegData->floors().size() > 0;
    bool couponCapFloor = cpiLegData->caps().size() > 0 || cpiLegData->floors().size() > 0;
    bool finalFlowCapFloor = cpiLegData->finalFlowCap() != Null<Real>() || cpiLegData->finalFlowFloor() != Null<Real>();

    applyAmortization(notionals, data, schedule, false);
    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());

    QuantExt::CPILeg cpiLeg =
        QuantExt::CPILeg(schedule, index,
                         engineFactory->market()->discountCurve(data.currency(),
                                                                engineFactory->configuration(MarketContext::pricing)),
                         cpiLegData->baseCPI(), observationLag)
            .withNotionals(notionals)
            .withPaymentDayCounter(dc)
            .withPaymentAdjustment(bdc)
            .withPaymentCalendar(paymentCalendar)
            .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
            .withFixedRates(rates)
            .withObservationInterpolation(interpolationMethod)
            .withSubtractInflationNominal(cpiLegData->subtractInflationNominal())
            .withSubtractInflationNominalAllCoupons(cpiLegData->subtractInflationNominalCoupons());

    // the cpi leg uses the first schedule date as the start date, which only makes sense if there are at least
    // two dates in the schedule, otherwise the only date in the schedule is the pay date of the cf and a separate
    // start date is expected; if both the separate start date and a schedule with more than one date is given
    const string& start = cpiLegData->startDate();
    if (schedule.size() < 2) {
        QL_REQUIRE(!start.empty(), "makeCPILeg(): only one schedule date, a 'StartDate' must be given.");
        cpiLeg.withStartDate(parseDate(start));
    } else if (!start.empty()) {
        DLOG("Schedule with more than 2 dates was provided. The first schedule date "

             << io::iso_date(schedule.dates().front()) << " is used as the start date. The 'StartDate' of " << start
             << " is not used.");
    }
    if (couponCap)
        cpiLeg.withCaps(buildScheduledVector(cpiLegData->caps(), cpiLegData->capDates(), schedule));

    if (couponFloor)
        cpiLeg.withFloors(buildScheduledVector(cpiLegData->floors(), cpiLegData->floorDates(), schedule));

    if (cpiLegData->finalFlowCap() != Null<Real>())
        cpiLeg.withFinalFlowCap(cpiLegData->finalFlowCap());

    if (cpiLegData->finalFlowFloor() != Null<Real>())
        cpiLeg.withFinalFlowFloor(cpiLegData->finalFlowFloor());

    Leg leg = cpiLeg.operator Leg();
    Size n = leg.size();
    QL_REQUIRE(n > 0, "Empty CPI Leg");

    if (couponCapFloor || finalFlowCapFloor) {
        
        string indexName = cpiLegData->index();
        
        // get a coupon pricer for the leg
        QuantLib::ext::shared_ptr<EngineBuilder> cpBuilder = engineFactory->builder("CappedFlooredCpiLegCoupons");
        QL_REQUIRE(cpBuilder, "No builder found for CappedFlooredCpiLegCoupons");
        QuantLib::ext::shared_ptr<CapFlooredCpiLegCouponEngineBuilder> cappedFlooredCpiCouponBuilder =
            QuantLib::ext::dynamic_pointer_cast<CapFlooredCpiLegCouponEngineBuilder>(cpBuilder);
        QuantLib::ext::shared_ptr<InflationCouponPricer> couponPricer = cappedFlooredCpiCouponBuilder->engine(indexName);

        // get a cash flow pricer for the leg
        QuantLib::ext::shared_ptr<EngineBuilder> cfBuilder = engineFactory->builder("CappedFlooredCpiLegCashFlows");
        QL_REQUIRE(cfBuilder, "No builder found for CappedFlooredCpiLegCashFLows");
        QuantLib::ext::shared_ptr<CapFlooredCpiLegCashFlowEngineBuilder> cappedFlooredCpiCashFlowBuilder =
            QuantLib::ext::dynamic_pointer_cast<CapFlooredCpiLegCashFlowEngineBuilder>(cfBuilder);
        QuantLib::ext::shared_ptr<InflationCashFlowPricer> cashFlowPricer = cappedFlooredCpiCashFlowBuilder->engine(indexName);

        // set coupon pricer for the leg
        for (Size i = 0; i < leg.size(); i++) {
            // nothing to do for the plain CPI Coupon, because the pricer is already set when the leg builder is called
            // nothing to do for the plain CPI CashFlow either, because it does not require a pricer

            QuantLib::ext::shared_ptr<CappedFlooredCPICoupon> cfCpiCoupon =
                QuantLib::ext::dynamic_pointer_cast<CappedFlooredCPICoupon>(leg[i]);
            if (cfCpiCoupon && couponCapFloor) {
                cfCpiCoupon->setPricer(couponPricer);
            }

            QuantLib::ext::shared_ptr<CappedFlooredCPICashFlow> cfCpiCashFlow =
                QuantLib::ext::dynamic_pointer_cast<CappedFlooredCPICashFlow>(leg[i]);
            if (cfCpiCashFlow && finalFlowCapFloor && i == (leg.size() - 1)) {
                cfCpiCashFlow->setPricer(cashFlowPricer);
            }
        }
    }

    // QuantLib CPILeg automatically adds a Notional Cashflow at maturity date on a CPI swap
    if (!data.notionalFinalExchange()) {
        leg.pop_back();
    }

    // build naked option leg if required and we have at least one cap/floor present in the coupon or the final flow
    if ((couponCapFloor || finalFlowCapFloor) && cpiLegData->nakedOption()) {
        leg = StrippedCappedFlooredCPICouponLeg(leg);
    }

    return leg;
}

Leg makeYoYLeg(const LegData& data, const QuantLib::ext::shared_ptr<InflationIndex>& index,
               const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const QuantLib::Date& openEndDateReplacement) {
    QuantLib::ext::shared_ptr<YoYLegData> yoyLegData = QuantLib::ext::dynamic_pointer_cast<YoYLegData>(data.concreteLegData());
    QL_REQUIRE(yoyLegData, "Wrong LegType, expected YoY, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Calendar paymentCalendar;

    QuantLib::ext::shared_ptr<InflationSwapConvention> cpiSwapConvention = nullptr;

    auto inflationConventions = InstrumentConventions::instance().conventions()->get(
        yoyLegData->index() + "_INFLATIONSWAP", Convention::Type::InflationSwap);

    if (inflationConventions.first)
        cpiSwapConvention = QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(inflationConventions.second);

    Period observationLag;
    if (yoyLegData->observationLag().empty()) {
        QL_REQUIRE(cpiSwapConvention,
                   "observationLag is not specified in legData and couldn't find convention for "
                       << yoyLegData->index() << ". Please add field to trade xml or add convention");
        DLOG("Build CPI Leg and use observation lag from standard inflationswap convention");
        observationLag = cpiSwapConvention->observationLag();
    } else {
        observationLag = parsePeriod(yoyLegData->observationLag());
    }

    if (data.paymentCalendar().empty())
        paymentCalendar = schedule.calendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());

    vector<double> gearings =
        buildScheduledVectorNormalised(yoyLegData->gearings(), yoyLegData->gearingDates(), schedule, 1.0);
    vector<double> spreads =
        buildScheduledVectorNormalised(yoyLegData->spreads(), yoyLegData->spreadDates(), schedule, 0.0);
    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);

    bool irregularYoY = yoyLegData->irregularYoY();
    bool couponCap = yoyLegData->caps().size() > 0;
    bool couponFloor = yoyLegData->floors().size() > 0;
    bool couponCapFloor = yoyLegData->caps().size() > 0 || yoyLegData->floors().size() > 0;
    bool addInflationNotional = yoyLegData->addInflationNotional();

    applyAmortization(notionals, data, schedule, false);
    Leg leg;
    if (!irregularYoY) {
        auto yoyIndex = QuantLib::ext::dynamic_pointer_cast<YoYInflationIndex>(index);
        QL_REQUIRE(yoyIndex, "Need a YoY Inflation Index");
        QuantExt::yoyInflationLeg yoyLeg =
            QuantExt::yoyInflationLeg(schedule, paymentCalendar, yoyIndex, observationLag)
                .withNotionals(notionals)
                .withPaymentDayCounter(dc)
                .withPaymentAdjustment(bdc)
                .withFixingDays(yoyLegData->fixingDays())
                .withGearings(gearings)
                .withSpreads(spreads)
                .withInflationNotional(addInflationNotional)
                .withRateCurve(engineFactory->market()->discountCurve(
                    data.currency(), engineFactory->configuration(MarketContext::pricing)));

        if (couponCap)
            yoyLeg.withCaps(buildScheduledVector(yoyLegData->caps(), yoyLegData->capDates(), schedule));

        if (couponFloor)
            yoyLeg.withFloors(buildScheduledVector(yoyLegData->floors(), yoyLegData->floorDates(), schedule));

        leg = yoyLeg.operator Leg();

        if (couponCapFloor) {
            // get a coupon pricer for the leg
            QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("CapFlooredYYLeg");
            QL_REQUIRE(builder, "No builder found for CapFlooredYYLeg");
            QuantLib::ext::shared_ptr<CapFlooredYoYLegEngineBuilder> cappedFlooredYoYBuilder =
                QuantLib::ext::dynamic_pointer_cast<CapFlooredYoYLegEngineBuilder>(builder);
            string indexname = yoyLegData->index();
            QuantLib::ext::shared_ptr<InflationCouponPricer> couponPricer =
                cappedFlooredYoYBuilder->engine(IndexNameTranslator::instance().oreName(indexname));

            // set coupon pricer for the leg

            for (Size i = 0; i < leg.size(); i++) {
                QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredYoYInflationCoupon>(leg[i])->setPricer(
                    QuantLib::ext::dynamic_pointer_cast<QuantLib::YoYInflationCouponPricer>(couponPricer));
            }

            // build naked option leg if required
            if (yoyLegData->nakedOption()) {
                leg = StrippedCappedFlooredYoYInflationCouponLeg(leg);
                for (auto const& t : leg) {
                    auto s = QuantLib::ext::dynamic_pointer_cast<StrippedCappedFlooredYoYInflationCoupon>(t);
                }
            }
        }
    } else {
        QuantLib::CPI::InterpolationType interpolation = QuantLib::CPI::Flat;
        if (cpiSwapConvention && cpiSwapConvention->interpolated()) {
            interpolation = QuantLib::CPI::Linear;
        }
        auto zcIndex = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndex>(index);
        QL_REQUIRE(zcIndex, "Need a Zero Coupon Inflation Index");
        QuantExt::NonStandardYoYInflationLeg yoyLeg =
            QuantExt::NonStandardYoYInflationLeg(schedule, schedule.calendar(), zcIndex, observationLag)
                .withNotionals(notionals)
                .withPaymentDayCounter(dc)
                .withPaymentAdjustment(bdc)
                .withFixingDays(yoyLegData->fixingDays())
                .withGearings(gearings)
                .withSpreads(spreads)
                .withRateCurve(engineFactory->market()->discountCurve(
                    data.currency(), engineFactory->configuration(MarketContext::pricing)))
                .withInflationNotional(addInflationNotional)
                .withObservationInterpolation(interpolation);

        if (couponCap)
            yoyLeg.withCaps(buildScheduledVector(yoyLegData->caps(), yoyLegData->capDates(), schedule));

        if (couponFloor)
            yoyLeg.withFloors(buildScheduledVector(yoyLegData->floors(), yoyLegData->floorDates(), schedule));

        leg = yoyLeg.operator Leg();

        if (couponCapFloor) {
            // get a coupon pricer for the leg
            QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("CapFlooredNonStdYYLeg");
            QL_REQUIRE(builder, "No builder found for CapFlooredNonStdYYLeg");
            QuantLib::ext::shared_ptr<CapFlooredNonStandardYoYLegEngineBuilder> cappedFlooredYoYBuilder =
                QuantLib::ext::dynamic_pointer_cast<CapFlooredNonStandardYoYLegEngineBuilder>(builder);
            string indexname = zcIndex->name();
            QuantLib::ext::shared_ptr<InflationCouponPricer> couponPricer =
                cappedFlooredYoYBuilder->engine(IndexNameTranslator::instance().oreName(indexname));

            // set coupon pricer for the leg

            for (Size i = 0; i < leg.size(); i++) {
                QuantLib::ext::dynamic_pointer_cast<QuantExt::NonStandardCappedFlooredYoYInflationCoupon>(leg[i])->setPricer(
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::NonStandardYoYInflationCouponPricer>(couponPricer));
            }

            // build naked option leg if required
            if (yoyLegData->nakedOption()) {
                leg = StrippedCappedFlooredYoYInflationCouponLeg(leg);
                for (auto const& t : leg) {
                    auto s = QuantLib::ext::dynamic_pointer_cast<StrippedCappedFlooredYoYInflationCoupon>(t);
                }
            }
        }
    }
    return leg;
}

Leg makeCMSLeg(const LegData& data, const QuantLib::ext::shared_ptr<QuantLib::SwapIndex>& swapIndex,
               const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer,
               const QuantLib::Date& openEndDateReplacement) {
    QuantLib::ext::shared_ptr<CMSLegData> cmsData = QuantLib::ext::dynamic_pointer_cast<CMSLegData>(data.concreteLegData());
    QL_REQUIRE(cmsData, "Wrong LegType, expected CMS, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Calendar paymentCalendar;
    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());

    if (data.paymentCalendar().empty())
        paymentCalendar = schedule.calendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());

    vector<double> spreads =
        ore::data::buildScheduledVectorNormalised(cmsData->spreads(), cmsData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        ore::data::buildScheduledVectorNormalised(cmsData->gearings(), cmsData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);
    Size fixingDays = cmsData->fixingDays() == Null<Size>() ? swapIndex->fixingDays() : cmsData->fixingDays();

    applyAmortization(notionals, data, schedule, false);

    CmsLeg cmsLeg = CmsLeg(schedule, swapIndex)
                        .withNotionals(notionals)
                        .withSpreads(spreads)
                        .withGearings(gearings)
                        .withPaymentCalendar(paymentCalendar)
                        .withPaymentDayCounter(dc)
                        .withPaymentAdjustment(bdc)
                        .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                        .withFixingDays(fixingDays)
                        .inArrears(cmsData->isInArrears());

    if (cmsData->caps().size() > 0)
        cmsLeg.withCaps(buildScheduledVector(cmsData->caps(), cmsData->capDates(), schedule));

    if (cmsData->floors().size() > 0)
        cmsLeg.withFloors(buildScheduledVector(cmsData->floors(), cmsData->floorDates(), schedule));

    if (!attachPricer)
        return cmsLeg;

    // Get a coupon pricer for the leg
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("CMS");
    QL_REQUIRE(builder, "No builder found for CmsLeg");
    QuantLib::ext::shared_ptr<CmsCouponPricerBuilder> cmsSwapBuilder =
        QuantLib::ext::dynamic_pointer_cast<CmsCouponPricerBuilder>(builder);
    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> couponPricer =
        cmsSwapBuilder->engine(IndexNameTranslator::instance().oreName(swapIndex->iborIndex()->name()));

    // Loop over the coupons in the leg and set pricer
    Leg tmpLeg = cmsLeg;
    QuantLib::setCouponPricer(tmpLeg, couponPricer);

    // build naked option leg if required
    if (cmsData->nakedOption()) {
        tmpLeg = StrippedCappedFlooredCouponLeg(tmpLeg);
    }
    return tmpLeg;
}

Leg makeCMBLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer,
               const QuantLib::Date& openEndDateReplacement) {
    QuantLib::ext::shared_ptr<CMBLegData> cmbData = QuantLib::ext::dynamic_pointer_cast<CMBLegData>(data.concreteLegData());
    QL_REQUIRE(cmbData, "Wrong LegType, expected CMB, got " << data.legType());

    std::string bondIndexName = cmbData->genericBond();
    // Expected bondIndexName structure with at least two tokens, separated by "-", of the form FAMILY-TERM or
    // FAMILY-MUN, for example: US-CMT-5Y, US-TIPS-10Y, UK-GILT-5Y, DE-BUND-10Y
    std::vector<string> tokens;
    split(tokens, bondIndexName, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() >= 2,
               "Generic Bond Index with at least two tokens separated by - expected, found " << bondIndexName);
    std::string securityFamily = tokens[0];
    for (Size i = 1; i < tokens.size() - 1; ++i)
        securityFamily = securityFamily + "-" + tokens[i];
    string underlyingTerm = tokens.back();
    Period underlyingPeriod = parsePeriod(underlyingTerm);
    LOG("Generic bond id " << bondIndexName << " has family " << securityFamily << " and term " << underlyingPeriod);

    Schedule schedule = makeSchedule(data.schedule());
    Calendar calendar = schedule.calendar();
    int fixingDays = cmbData->fixingDays();
    BusinessDayConvention convention = schedule.businessDayConvention();
    bool creditRisk = cmbData->hasCreditRisk();

    // Get the generic bond reference data, notional 1, credit risk as specified in the leg data 
    BondData bondData(securityFamily, 1.0, creditRisk);
    bondData.populateFromBondReferenceData(engineFactory->referenceData());
    DLOG("Bond data for security id " << securityFamily << " loaded");
    QL_REQUIRE(bondData.coupons().size() == 1,
	       "multiple reference bond legs not covered by the CMB leg");
    QL_REQUIRE(bondData.coupons().front().schedule().rules().size() == 1,
	       "multiple bond schedule rules not covered by the CMB leg");
    QL_REQUIRE(bondData.coupons().front().schedule().dates().size() == 0,
	       "dates based bond schedules not covered by the CMB leg");

    // Get bond yield conventions
    auto ret = InstrumentConventions::instance().conventions()->get(securityFamily, Convention::Type::BondYield);
    QuantLib::ext::shared_ptr<BondYieldConvention> conv;
    if (ret.first)
        conv = QuantLib::ext::dynamic_pointer_cast<BondYieldConvention>(ret.second);
    else {
	conv = QuantLib::ext::make_shared<BondYieldConvention>();
        ALOG("BondYield conventions not found for security " << securityFamily << ", falling back on defaults:"
	     << " compounding=" << conv->compoundingName()
	     << ", priceType=" << conv->priceTypeName()
	     << ", accuracy=" << conv->accuracy() 
	     << ", maxEvaluations=" << conv->maxEvaluations() 
	     << ", guess=" << conv->guess());
    } 

    Schedule bondSchedule = makeSchedule(bondData.coupons().front().schedule());
    DayCounter bondDayCounter = parseDayCounter(bondData.coupons().front().dayCounter());
    Currency bondCurrency = parseCurrency(bondData.currency());
    Calendar bondCalendar = parseCalendar(bondData.calendar());
    Size bondSettlementDays = parseInteger(bondData.settlementDays());
    BusinessDayConvention bondConvention = bondSchedule.businessDayConvention();
    bool bondEndOfMonth = bondSchedule.endOfMonth();
    Frequency bondFrequency = bondSchedule.tenor().frequency();
    
    DayCounter dayCounter = parseDayCounter(data.dayCounter());

    // Create a ConstantMaturityBondIndex for each schedule start date
    DLOG("Create Constant Maturity Bond Indices for each period");
    std::vector<QuantLib::ext::shared_ptr<QuantExt::ConstantMaturityBondIndex>> bondIndices;
    for (Size i = 0; i < schedule.dates().size() - 1; ++i) {
        // Construct bond with start date = accrual start date and maturity = accrual start date + term
        // or start = accrual end if in arrears. Adjusted for fixing lag, ignoring bond settlement lags for now.
        Date refDate = cmbData->isInArrears() ? schedule[i+1] : schedule[i];
	Date start = calendar.advance(refDate, -fixingDays, Days, Preceding); 
	std::string startDate = to_string(start);
	std::string endDate = to_string(start + underlyingPeriod);
	bondData.populateFromBondReferenceData(engineFactory->referenceData(), startDate, endDate);
	Bond bondTrade(Envelope(), bondData);
	bondTrade.build(engineFactory);
	auto bond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(bondTrade.instrument()->qlInstrument());
	QuantLib::ext::shared_ptr<QuantExt::ConstantMaturityBondIndex> bondIndex
	    = QuantLib::ext::make_shared<QuantExt::ConstantMaturityBondIndex>(securityFamily, underlyingPeriod,
	        // from bond reference data
		bondSettlementDays, bondCurrency, bondCalendar, bondDayCounter, bondConvention, bondEndOfMonth,
		// underlying forward starting bond
		bond,
		// yield calculation parameters from conventions, except frequency which is from bond reference data
		conv->compounding(), bondFrequency, conv->accuracy(), conv->maxEvaluations(), conv->guess(), conv->priceType());
	bondIndices.push_back(bondIndex);
    }
    
    // Create a sequence of floating rate coupons linked to those indexes and concatenate them to a leg
    DLOG("Create CMB leg");
    vector<double> spreads = buildScheduledVectorNormalised(cmbData->spreads(), cmbData->spreadDates(), schedule, 0.0);
    vector<double> gearings = buildScheduledVectorNormalised(cmbData->gearings(), cmbData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);

    // FIXME: Move the following into CmbLeg in cmbcoupon.xpp
    QL_REQUIRE(bondIndices.size() == schedule.size() - 1,
	       "vector size mismatch between schedule (" << schedule.size() << ") "
	       << "and bond indices (" << bondIndices.size() << ")");
    Leg leg;
    for (Size i = 0; i < schedule.size() - 1; i++) {
        Date paymentDate = calendar.adjust(schedule[i + 1], convention);
	DLOG("Coupon " << i << ": "
	     << io::iso_date(paymentDate) << " "
	     << notionals[i] << " "
	     << io::iso_date(schedule[i]) << " "
	     << io::iso_date(schedule[i+1]) << " "
	     << cmbData->fixingDays() << " "
	     << gearings[i] << " "
	     << spreads[i] << " "
	     << dayCounter.name());
	QuantLib::ext::shared_ptr<CmbCoupon> coupon
	    = QuantLib::ext::make_shared<CmbCoupon>(paymentDate, notionals[i], schedule[i], schedule[i + 1],
					    cmbData->fixingDays(), bondIndices[i], gearings[i], spreads[i], Date(), Date(),
					    dayCounter, cmbData->isInArrears());	
	auto pricer = QuantLib::ext::make_shared<CmbCouponPricer>();
	coupon->setPricer(pricer);
	leg.push_back(coupon);
    }

    return leg;
}

Leg makeDigitalCMSLeg(const LegData& data, const QuantLib::ext::shared_ptr<QuantLib::SwapIndex>& swapIndex,
                      const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer,
                      const QuantLib::Date& openEndDateReplacement) {
    auto digitalCmsData = QuantLib::ext::dynamic_pointer_cast<DigitalCMSLegData>(data.concreteLegData());
    QL_REQUIRE(digitalCmsData, "Wrong LegType, expected DigitalCMS");

    auto cmsData = QuantLib::ext::dynamic_pointer_cast<CMSLegData>(digitalCmsData->underlying());
    QL_REQUIRE(cmsData, "Incomplete DigitalCms Leg, expected CMS data");

    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);

    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    vector<double> spreads =
        ore::data::buildScheduledVectorNormalised(cmsData->spreads(), cmsData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        ore::data::buildScheduledVectorNormalised(cmsData->gearings(), cmsData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);

    double eps = 1e-4;
    vector<double> callStrikes =
        ore::data::buildScheduledVector(digitalCmsData->callStrikes(), digitalCmsData->callStrikeDates(), schedule);

    for (Size i = 0; i < callStrikes.size(); i++) {
        if (std::fabs(callStrikes[i]) < eps / 2) {
            callStrikes[i] = eps / 2;
        }
    }

    vector<double> callPayoffs =
        ore::data::buildScheduledVector(digitalCmsData->callPayoffs(), digitalCmsData->callPayoffDates(), schedule);

    vector<double> putStrikes =
        ore::data::buildScheduledVector(digitalCmsData->putStrikes(), digitalCmsData->putStrikeDates(), schedule);
    vector<double> putPayoffs =
        ore::data::buildScheduledVector(digitalCmsData->putPayoffs(), digitalCmsData->putPayoffDates(), schedule);

    Size fixingDays = cmsData->fixingDays() == Null<Size>() ? swapIndex->fixingDays() : cmsData->fixingDays();

    applyAmortization(notionals, data, schedule, false);

    DigitalCmsLeg digitalCmsLeg = DigitalCmsLeg(schedule, swapIndex)
                                      .withNotionals(notionals)
                                      .withSpreads(spreads)
                                      .withGearings(gearings)
                                      // .withPaymentCalendar(paymentCalendar)
                                      .withPaymentDayCounter(dc)
                                      .withPaymentAdjustment(bdc)
                                      .withFixingDays(fixingDays)
                                      .inArrears(cmsData->isInArrears())
                                      .withCallStrikes(callStrikes)
                                      .withLongCallOption(digitalCmsData->callPosition())
                                      .withCallATM(digitalCmsData->isCallATMIncluded())
                                      .withCallPayoffs(callPayoffs)
                                      .withPutStrikes(putStrikes)
                                      .withLongPutOption(digitalCmsData->putPosition())
                                      .withPutATM(digitalCmsData->isPutATMIncluded())
                                      .withPutPayoffs(putPayoffs)
                                      .withReplication(QuantLib::ext::make_shared<DigitalReplication>())
                                      .withNakedOption(cmsData->nakedOption());

    if (cmsData->caps().size() > 0 || cmsData->floors().size() > 0)
        QL_FAIL("caps/floors not supported in DigitalCMSOptions");

    if (!attachPricer)
        return digitalCmsLeg;

    // Get a coupon pricer for the leg
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("CMS");
    QL_REQUIRE(builder, "No CMS builder found for CmsLeg");
    QuantLib::ext::shared_ptr<CmsCouponPricerBuilder> cmsBuilder = QuantLib::ext::dynamic_pointer_cast<CmsCouponPricerBuilder>(builder);
    auto cmsPricer = QuantLib::ext::dynamic_pointer_cast<CmsCouponPricer>(
        cmsBuilder->engine(IndexNameTranslator::instance().oreName(swapIndex->iborIndex()->name())));
    QL_REQUIRE(cmsPricer, "Expected CMS Pricer");

    // Loop over the coupons in the leg and set pricer
    Leg tmpLeg = digitalCmsLeg;
    QuantLib::setCouponPricer(tmpLeg, cmsPricer);

    return tmpLeg;
}

Leg makeCMSSpreadLeg(const LegData& data, const QuantLib::ext::shared_ptr<QuantLib::SwapSpreadIndex>& swapSpreadIndex,
                     const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer,
                     const QuantLib::Date& openEndDateReplacement) {
    QuantLib::ext::shared_ptr<CMSSpreadLegData> cmsSpreadData =
        QuantLib::ext::dynamic_pointer_cast<CMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(cmsSpreadData, "Wrong LegType, expected CMSSpread, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Calendar paymentCalendar;
    if (data.paymentCalendar().empty())
        paymentCalendar = schedule.calendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());

    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());

    vector<double> spreads = ore::data::buildScheduledVectorNormalised(cmsSpreadData->spreads(),
                                                                       cmsSpreadData->spreadDates(), schedule, 0.0);
    vector<double> gearings = ore::data::buildScheduledVectorNormalised(cmsSpreadData->gearings(),
                                                                        cmsSpreadData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);
    Size fixingDays =
        cmsSpreadData->fixingDays() == Null<Size>() ? swapSpreadIndex->fixingDays() : cmsSpreadData->fixingDays();

    applyAmortization(notionals, data, schedule, false);

    CmsSpreadLeg cmsSpreadLeg = CmsSpreadLeg(schedule, swapSpreadIndex)
                                    .withNotionals(notionals)
                                    .withSpreads(spreads)
                                    .withGearings(gearings)
                                    .withPaymentCalendar(paymentCalendar)
                                    .withPaymentDayCounter(dc)
                                    .withPaymentAdjustment(bdc)
                                    .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                                    .withFixingDays(fixingDays)
                                    .inArrears(cmsSpreadData->isInArrears());

    if (cmsSpreadData->caps().size() > 0)
        cmsSpreadLeg.withCaps(buildScheduledVector(cmsSpreadData->caps(), cmsSpreadData->capDates(), schedule));

    if (cmsSpreadData->floors().size() > 0)
        cmsSpreadLeg.withFloors(buildScheduledVector(cmsSpreadData->floors(), cmsSpreadData->floorDates(), schedule));

    if (!attachPricer)
        return cmsSpreadLeg;

    // Get a coupon pricer for the leg
    auto builder1 = engineFactory->builder("CMS");
    QL_REQUIRE(builder1, "No CMS builder found for CmsSpreadLeg");
    auto cmsBuilder = QuantLib::ext::dynamic_pointer_cast<CmsCouponPricerBuilder>(builder1);
    auto cmsPricer = QuantLib::ext::dynamic_pointer_cast<CmsCouponPricer>(cmsBuilder->engine(
        IndexNameTranslator::instance().oreName(swapSpreadIndex->swapIndex1()->iborIndex()->name())));
    QL_REQUIRE(cmsPricer, "Expected CMS Pricer");
    auto builder2 = engineFactory->builder("CMSSpread");
    QL_REQUIRE(builder2, "No CMS Spread builder found for CmsSpreadLeg");
    auto cmsSpreadBuilder = QuantLib::ext::dynamic_pointer_cast<CmsSpreadCouponPricerBuilder>(builder2);
    auto cmsSpreadPricer = cmsSpreadBuilder->engine(swapSpreadIndex->currency(), cmsSpreadData->swapIndex1(),
                                                    cmsSpreadData->swapIndex2(), cmsPricer);
    QL_REQUIRE(cmsSpreadPricer, "Expected CMS Spread Pricer");

    // Loop over the coupons in the leg and set pricer
    Leg tmpLeg = cmsSpreadLeg;
    QuantLib::setCouponPricer(tmpLeg, cmsSpreadPricer);

    // build naked option leg if required
    if (cmsSpreadData->nakedOption()) {
        tmpLeg = StrippedCappedFlooredCouponLeg(tmpLeg);
    }
    return tmpLeg;
}

Leg makeDigitalCMSSpreadLeg(const LegData& data, const QuantLib::ext::shared_ptr<QuantLib::SwapSpreadIndex>& swapSpreadIndex,
                            const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                            const QuantLib::Date& openEndDateReplacement) {
    auto digitalCmsSpreadData = QuantLib::ext::dynamic_pointer_cast<DigitalCMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(digitalCmsSpreadData, "Wrong LegType, expected DigitalCMSSpread");

    auto cmsSpreadData = QuantLib::ext::dynamic_pointer_cast<CMSSpreadLegData>(digitalCmsSpreadData->underlying());
    QL_REQUIRE(cmsSpreadData, "Incomplete DigitalCmsSpread Leg, expected CMSSpread data");

    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());

    Calendar paymentCalendar;
    if (data.paymentCalendar().empty())
        paymentCalendar = schedule.calendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());

    vector<double> spreads = ore::data::buildScheduledVectorNormalised(cmsSpreadData->spreads(),
                                                                       cmsSpreadData->spreadDates(), schedule, 0.0);
    vector<double> gearings = ore::data::buildScheduledVectorNormalised(cmsSpreadData->gearings(),
                                                                        cmsSpreadData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);

    double eps = 1e-4;
    vector<double> callStrikes = ore::data::buildScheduledVector(digitalCmsSpreadData->callStrikes(),
                                                                 digitalCmsSpreadData->callStrikeDates(), schedule);

    for (Size i = 0; i < callStrikes.size(); i++) {
        if (std::fabs(callStrikes[i]) < eps / 2) {
            callStrikes[i] = eps / 2;
        }
    }

    vector<double> callPayoffs = ore::data::buildScheduledVector(digitalCmsSpreadData->callPayoffs(),
                                                                 digitalCmsSpreadData->callPayoffDates(), schedule);

    vector<double> putStrikes = ore::data::buildScheduledVector(digitalCmsSpreadData->putStrikes(),
                                                                digitalCmsSpreadData->putStrikeDates(), schedule);
    vector<double> putPayoffs = ore::data::buildScheduledVector(digitalCmsSpreadData->putPayoffs(),
                                                                digitalCmsSpreadData->putPayoffDates(), schedule);

    Size fixingDays =
        cmsSpreadData->fixingDays() == Null<Size>() ? swapSpreadIndex->fixingDays() : cmsSpreadData->fixingDays();

    applyAmortization(notionals, data, schedule, false);

    DigitalCmsSpreadLeg digitalCmsSpreadLeg = DigitalCmsSpreadLeg(schedule, swapSpreadIndex)
                                                  .withNotionals(notionals)
                                                  .withSpreads(spreads)
                                                  .withGearings(gearings)
                                                  .withPaymentDayCounter(dc)
                                                  .withPaymentCalendar(paymentCalendar)
                                                  .withPaymentAdjustment(bdc)
                                                  .withFixingDays(fixingDays)
                                                  .inArrears(cmsSpreadData->isInArrears())
                                                  .withCallStrikes(callStrikes)
                                                  .withLongCallOption(digitalCmsSpreadData->callPosition())
                                                  .withCallATM(digitalCmsSpreadData->isCallATMIncluded())
                                                  .withCallPayoffs(callPayoffs)
                                                  .withPutStrikes(putStrikes)
                                                  .withLongPutOption(digitalCmsSpreadData->putPosition())
                                                  .withPutATM(digitalCmsSpreadData->isPutATMIncluded())
                                                  .withPutPayoffs(putPayoffs)
                                                  .withReplication(QuantLib::ext::make_shared<DigitalReplication>())
                                                  .withNakedOption(cmsSpreadData->nakedOption());

    if (cmsSpreadData->caps().size() > 0 || cmsSpreadData->floors().size() > 0)
        QL_FAIL("caps/floors not supported in DigitalCMSSpreadOptions");

    // Get a coupon pricer for the leg
    auto builder1 = engineFactory->builder("CMS");
    QL_REQUIRE(builder1, "No CMS builder found for CmsSpreadLeg");
    auto cmsBuilder = QuantLib::ext::dynamic_pointer_cast<CmsCouponPricerBuilder>(builder1);
    auto cmsPricer = QuantLib::ext::dynamic_pointer_cast<CmsCouponPricer>(cmsBuilder->engine(
        IndexNameTranslator::instance().oreName(swapSpreadIndex->swapIndex1()->iborIndex()->name())));
    QL_REQUIRE(cmsPricer, "Expected CMS Pricer");
    auto builder2 = engineFactory->builder("CMSSpread");
    QL_REQUIRE(builder2, "No CMS Spread builder found for CmsSpreadLeg");
    auto cmsSpreadBuilder = QuantLib::ext::dynamic_pointer_cast<CmsSpreadCouponPricerBuilder>(builder2);
    auto cmsSpreadPricer = cmsSpreadBuilder->engine(swapSpreadIndex->currency(), cmsSpreadData->swapIndex1(),
                                                    cmsSpreadData->swapIndex2(), cmsPricer);
    QL_REQUIRE(cmsSpreadPricer, "Expected CMS Spread Pricer");

    // Loop over the coupons in the leg and set pricer
    Leg tmpLeg = digitalCmsSpreadLeg;
    QuantLib::setCouponPricer(tmpLeg, cmsSpreadPricer);

    return tmpLeg;
}

Leg makeEquityLeg(const LegData& data, const QuantLib::ext::shared_ptr<EquityIndex2>& equityCurve,
                  const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex, const QuantLib::Date& openEndDateReplacement) {
    QuantLib::ext::shared_ptr<EquityLegData> eqLegData = QuantLib::ext::dynamic_pointer_cast<EquityLegData>(data.concreteLegData());
    QL_REQUIRE(eqLegData, "Wrong LegType, expected Equity, got " << data.legType());

    DayCounter dc;
    if (data.dayCounter().empty())
        dc = Actual365Fixed();
    else
        dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());

    Real dividendFactor = eqLegData->dividendFactor();
    Real initialPrice = eqLegData->initialPrice();
    bool initialPriceIsInTargetCcy = false;

    if (!eqLegData->initialPriceCurrency().empty()) {
        // parse currencies to handle minor currencies
        Currency initialPriceCurrency = parseCurrencyWithMinors(eqLegData->initialPriceCurrency());
        Currency dataCurrency = parseCurrencyWithMinors(data.currency());
        Currency eqCurrency;
        // set equity currency
        if (!eqLegData->eqCurrency().empty())
            eqCurrency = parseCurrencyWithMinors(eqLegData->eqCurrency());
        else if (!equityCurve->currency().empty())
            eqCurrency = equityCurve->currency();
        else
            TLOG("Cannot find currency for equity " << equityCurve->name());

        // initial price currency must match leg or equity currency
        QL_REQUIRE(initialPriceCurrency == dataCurrency || initialPriceCurrency == eqCurrency || eqCurrency.empty(),
                   "initial price ccy (" << initialPriceCurrency << ") must match either leg ccy (" << dataCurrency
                                         << ") or equity ccy (if given, got '" << eqCurrency << "')");
        initialPriceIsInTargetCcy = initialPriceCurrency == dataCurrency;
        // adjust for minor currency
        initialPrice = convertMinorToMajorCurrency(eqLegData->initialPriceCurrency(), initialPrice);
    }
    bool notionalReset = eqLegData->notionalReset();
    Natural fixingDays = eqLegData->fixingDays();
    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());

    ScheduleBuilder scheduleBuilder;

    ScheduleData scheduleData = data.schedule();
    Schedule schedule;
    scheduleBuilder.add(schedule, scheduleData);

    ScheduleData valuationData = eqLegData->valuationSchedule();
    Schedule valuationSchedule;
    if (valuationData.hasData())
        scheduleBuilder.add(valuationSchedule, valuationData);

    scheduleBuilder.makeSchedules(openEndDateReplacement);

    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);

    Calendar paymentCalendar;
    if (data.paymentCalendar().empty())
        paymentCalendar = schedule.calendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());

    applyAmortization(notionals, data, schedule, false);
    Leg leg = EquityLeg(schedule, equityCurve, fxIndex)
                  .withNotionals(notionals)
                  .withQuantity(eqLegData->quantity())
                  .withPaymentDayCounter(dc)
                  .withPaymentAdjustment(bdc)
                  .withPaymentCalendar(paymentCalendar)
                  .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                  .withReturnType(eqLegData->returnType())
                  .withDividendFactor(dividendFactor)
                  .withInitialPrice(initialPrice)
                  .withInitialPriceIsInTargetCcy(initialPriceIsInTargetCcy)
                  .withNotionalReset(notionalReset)
                  .withFixingDays(fixingDays)
                  .withValuationSchedule(valuationSchedule);

    QL_REQUIRE(leg.size() > 0, "Empty Equity Leg");

    return leg;
}

Real currentNotional(const Leg& leg) {
    Date today = Settings::instance().evaluationDate();
    // assume the leg is sorted
    // We just take the first coupon::nominal we find, otherwise return 0
    for (auto cf : leg) {
        if (cf->date() > today) {
            QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(cf);
            if (coupon)
                return coupon->nominal();
        }
    }
    return 0;
}

Real originalNotional(const Leg& leg) {
    // assume the leg is sorted
    // We just take the first coupon::nominal we find, otherwise return 0
    if (leg.size() > 0) {
        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(leg.front());
        if (coupon)
            return coupon->nominal();
    }
    return 0;
}

vector<double> buildAmortizationScheduleFixedAmount(const vector<double>& notionals, const Schedule& schedule,
                                                    const AmortizationData& data) {
    DLOG("Build fixed amortization notional schedule");
    vector<double> nominals = normaliseToSchedule(notionals, schedule, (Real)Null<Real>());
    Date startDate = data.startDate().empty() ? Date::minDate() : parseDate(data.startDate());
    Date endDate = data.endDate().empty() ? Date::maxDate() : parseDate(data.endDate());
    bool underflow = data.underflow();
    Period amortPeriod = data.frequency().empty() ? 0 * Days : parsePeriod(data.frequency());
    double amort = data.value();
    Date lastAmortDate = Date::minDate();
    for (Size i = 0; i < schedule.size() - 1; i++) {
        if (i > 0 && (lastAmortDate == Date::minDate() || schedule[i] > lastAmortDate + amortPeriod - 4 * Days) &&
            schedule[i] >= startDate && schedule[i] < endDate) { // FIXME: tolerance
            nominals[i] = nominals[i - 1] - amort;
            lastAmortDate = schedule[i];
        } else if (i > 0 && lastAmortDate > Date::minDate()) {
            nominals[i] = nominals[i - 1];
        }
        if (amort > nominals[i] && underflow == false)
            amort = std::max(nominals[i], 0.0);
    }
    DLOG("Fixed amortization notional schedule done");
    return nominals;
}

vector<double> buildAmortizationScheduleRelativeToInitialNotional(const vector<double>& notionals,
                                                                  const Schedule& schedule,
                                                                  const AmortizationData& data) {
    DLOG("Build notional schedule with amortizations expressed as percentages of initial notional");
    vector<double> nominals = normaliseToSchedule(notionals, schedule, (Real)Null<Real>());
    Date startDate = data.startDate().empty() ? Date::minDate() : parseDate(data.startDate());
    Date endDate = data.endDate().empty() ? Date::maxDate() : parseDate(data.endDate());
    bool underflow = data.underflow();
    Period amortPeriod = data.frequency().empty() ? 0 * Days : parsePeriod(data.frequency());
    double amort = data.value() * nominals.front();
    Date lastAmortDate = Date::minDate();
    for (Size i = 0; i < schedule.size() - 1; i++) {
        if (i > 0 && (lastAmortDate == Date::minDate() || schedule[i] > lastAmortDate + amortPeriod - 4 * Days) &&
            schedule[i] >= startDate && schedule[i] < endDate) { // FIXME: tolerance
            nominals[i] = nominals[i - 1] - amort;
            lastAmortDate = schedule[i];
        } else if (i > 0 && lastAmortDate > Date::minDate())
            nominals[i] = nominals[i - 1];
        if (amort > nominals[i] && underflow == false) {
            amort = std::max(nominals[i], 0.0);
        }
    }
    DLOG("Fixed amortization notional schedule done");
    return nominals;
}

vector<double> buildAmortizationScheduleRelativeToPreviousNotional(const vector<double>& notionals,
                                                                   const Schedule& schedule,
                                                                   const AmortizationData& data) {
    DLOG("Build notional schedule with amortizations expressed as percentages of previous notionals");
    vector<double> nominals = normaliseToSchedule(notionals, schedule, (Real)Null<Real>());
    Date startDate = data.startDate().empty() ? Date::minDate() : parseDate(data.startDate());
    Date endDate = data.endDate().empty() ? Date::maxDate() : parseDate(data.endDate());
    Period amortPeriod = data.frequency().empty() ? 0 * Days : parsePeriod(data.frequency());
    double fraction = data.value();
    QL_REQUIRE(fraction < 1.0 || close_enough(fraction, 1.0),
               "amortization fraction " << fraction << " expected to be <= 1");
    Date lastAmortDate = Date::minDate();
    for (Size i = 0; i < schedule.size() - 1; i++) {
        if (i > 0 && (lastAmortDate == Date::minDate() || schedule[i] > lastAmortDate + amortPeriod - 4 * Days) &&
            schedule[i] >= startDate && schedule[i] < endDate) { // FIXME: tolerance
            nominals[i] = nominals[i - 1] * (1.0 - fraction);
            lastAmortDate = schedule[i];
        } else if (i > 0 && lastAmortDate > Date::minDate())
            nominals[i] = nominals[i - 1];
    }
    DLOG("Fixed amortization notional schedule done");
    return nominals;
}

vector<double> buildAmortizationScheduleFixedAnnuity(const vector<double>& notionals, const vector<double>& rates,
                                                     const Schedule& schedule, const AmortizationData& data,
                                                     const DayCounter& dc) {
    DLOG("Build fixed annuity notional schedule");
    vector<double> nominals = normaliseToSchedule(notionals, schedule, (Real)Null<Real>());
    Date startDate = data.startDate().empty() ? Date::minDate() : parseDate(data.startDate());
    Date endDate = data.endDate().empty() ? Date::maxDate() : parseDate(data.endDate());
    bool underflow = data.underflow();
    double annuity = data.value();
    Real amort = 0.0;
    Date lastAmortDate = Date::minDate();
    for (Size i = 0; i < schedule.size() - 1; i++) {
        if (i > 0 && schedule[i] >= startDate && schedule[i] < endDate) {
            nominals[i] = nominals[i - 1] - amort;
            lastAmortDate = schedule[i];
        } else if (i > 0 && lastAmortDate > Date::minDate())
            nominals[i] = nominals[i - 1];
        Real dcf = dc.yearFraction(schedule[i], schedule[i + 1]);
        Real rate = i < rates.size() ? rates[i] : rates.back();
        amort = annuity - rate * nominals[i] * dcf;
        if (amort > nominals[i] && underflow == false) {
            amort = std::max(nominals[i], 0.0);
        }
    }
    DLOG("Fixed Annuity notional schedule done");
    return nominals;
}

vector<double> buildAmortizationScheduleLinearToMaturity(const vector<double>& notionals, const Schedule& schedule,
                                                         const AmortizationData& data) {
    DLOG("Build linear-to-maturity notional schedule");
    vector<double> nominals = normaliseToSchedule(notionals, schedule, (Real)Null<Real>());
    Date startDate = data.startDate().empty() ? Date::minDate() : parseDate(data.startDate());
    Date endDate = data.endDate().empty() ? Date::maxDate() : parseDate(data.endDate());
    Period amortPeriod = data.frequency().empty() ? 0 * Days : parsePeriod(data.frequency());
    Date lastAmortDate = Date::minDate();
    Real periodAmortization = Null<Real>();
    Real accumulatedAmortization = 0.0;
    for (Size i = 0; i < schedule.size() - 1; ++i) {
        if (schedule[i] >= startDate && periodAmortization == Null<Real>()) {
            periodAmortization = nominals[i] / static_cast<Real>(schedule.size() - i);
        }
        if (i > 0 && schedule[i] >= startDate && schedule[i] < endDate) {
            accumulatedAmortization += periodAmortization;
        }
        if (i > 0 && (lastAmortDate == Date::minDate() || schedule[i] > lastAmortDate + amortPeriod - 4 * Days) &&
            schedule[i] >= startDate && schedule[i] < endDate) {
            nominals[i] = nominals[i - 1] - accumulatedAmortization;
            accumulatedAmortization = 0.0;
            lastAmortDate = schedule[i];
        } else if (i > 0 && lastAmortDate > Date::minDate()) {
            nominals[i] = nominals[i - 1];
        }
    }
    DLOG("Linear-to-maturity notional schedule done");
    return nominals;
}

void applyAmortization(std::vector<Real>& notionals, const LegData& data, const Schedule& schedule,
                       const bool annuityAllowed, const std::vector<Real>& rates) {
    Date lastEndDate = Date::minDate();
    for (Size i = 0; i < data.amortizationData().size(); ++i) {
        const AmortizationData& amort = data.amortizationData()[i];
        if (!amort.initialized())
            continue;
        QL_REQUIRE(i == 0 || !amort.startDate().empty(),
                   "All AmortizationData blocks except the first require a StartDate");
        Date startDate = amort.startDate().empty() ? Date::minDate() : parseDate(amort.startDate());
        QL_REQUIRE(startDate >= lastEndDate, "Amortization start date ("
                                                 << startDate << ") is earlier than last end date (" << lastEndDate
                                                 << ")");
        lastEndDate = amort.endDate().empty() ? Date::minDate() : parseDate(amort.endDate());
        AmortizationType amortizationType = parseAmortizationType(amort.type());
        if (amortizationType == AmortizationType::FixedAmount)
            notionals = buildAmortizationScheduleFixedAmount(notionals, schedule, amort);
        else if (amortizationType == AmortizationType::RelativeToInitialNotional)
            notionals = buildAmortizationScheduleRelativeToInitialNotional(notionals, schedule, amort);
        else if (amortizationType == AmortizationType::RelativeToPreviousNotional)
            notionals = buildAmortizationScheduleRelativeToPreviousNotional(notionals, schedule, amort);
        else if (amortizationType == AmortizationType::Annuity) {
            QL_REQUIRE(annuityAllowed, "Amortization type Annuity not allowed for leg type " << data.legType());
            if (!rates.empty())
                notionals = buildAmortizationScheduleFixedAnnuity(notionals, rates, schedule, amort,
                                                                  parseDayCounter(data.dayCounter()));
        } else if (amortizationType == AmortizationType::LinearToMaturity) {
            notionals = buildAmortizationScheduleLinearToMaturity(notionals, schedule, amort);
        } else {
            QL_FAIL("AmortizationType " << amort.type() << " not supported");
        }
        // check that for a floating leg we only have one amortization block, if the type is annuity
        // we recognise a floating leg by an empty (fixed) rates vector
        if (rates.empty() && amortizationType == AmortizationType::Annuity) {
            QL_REQUIRE(data.amortizationData().size() == 1,
                       "Floating Leg supports only one amortisation block of type Annuity");
        }
    }
}

void applyIndexing(Leg& leg, const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                   RequiredFixings& requiredFixings, const QuantLib::Date& openEndDateReplacement,
                   const bool useXbsCurves) {
    for (auto const& indexing : data.indexing()) {
        if (indexing.hasData()) {
            DLOG("apply indexing (index='" << indexing.index() << "') to leg of type " << data.legType());
            QL_REQUIRE(engineFactory, "applyIndexing: engineFactory required");

            // we allow indexing by equity, commodity and FX indices (technically any QuantLib::Index
            // will work, so the list of index types can be extended here if required)
            QuantLib::ext::shared_ptr<Index> index;
            string config = engineFactory->configuration(MarketContext::pricing);
            if (boost::starts_with(indexing.index(), "EQ-")) {
                string eqName = indexing.index().substr(3);
                index = *engineFactory->market()->equityCurve(eqName, config);
            } else if (boost::starts_with(indexing.index(), "FX-")) {
                auto tmp = parseFxIndex(indexing.index());
                Currency ccy1 = tmp->targetCurrency();
                Currency ccy2 = tmp->sourceCurrency();
                QL_REQUIRE(ccy1.code() == data.currency() || ccy2.code() == data.currency(),
                           "applyIndexing: fx index '" << indexing.index() << "' ccys do not match leg ccy ("
                                                       << data.currency() << ")");
                std::string domestic = data.currency();
                std::string foreign = ccy1.code() == domestic ? ccy2.code() : ccy1.code();
                index = buildFxIndex(indexing.index(), domestic, foreign, engineFactory->market(),
                                     engineFactory->configuration(MarketContext::pricing), useXbsCurves);

            } else if (boost::starts_with(indexing.index(), "COMM-")) {
                auto tmp = parseCommodityIndex(indexing.index());
                index = parseCommodityIndex(indexing.index(), true,
                                            engineFactory->market()->commodityPriceCurve(tmp->underlyingName(), config),
                                            tmp->fixingCalendar());
            } else if (boost::starts_with(indexing.index(), "BOND-")) {
                // if we build a bond index, we add the required fixings for the bond underlying
                QuantLib::ext::shared_ptr<BondIndex> bi = parseBondIndex(indexing.index());
                QL_REQUIRE(!(QuantLib::ext::dynamic_pointer_cast<BondFuturesIndex>(bi)),
                           "BondFuture Legs are not yet supported");
                BondData bondData(bi->securityName(), 1.0);
                BondIndexBuilder bondIndexBuilder(bondData, indexing.indexIsDirty(), indexing.indexIsRelative(),
                                       parseCalendar(indexing.fixingCalendar()),
                                       indexing.indexIsConditionalOnSurvival(), engineFactory);
                index = bondIndexBuilder.bondIndex();
                bondIndexBuilder.addRequiredFixings(requiredFixings, leg);
            } else {
                QL_FAIL("invalid index '" << indexing.index()
                                          << "' in indexing data, expected EQ-, FX-, COMM-, BOND- index");
            }

            QL_REQUIRE(index, "applyIndexing(): index is null, this is unexpected");

            // apply the indexing
            IndexedCouponLeg indLeg(leg, indexing.quantity(), index);
            indLeg.withInitialFixing(indexing.initialFixing());
            // we set the initial notional fixing only if we have an initial exchange, otherwise this is applied to the
            // first notional payment appearing in the leg
            if (data.notionalInitialExchange())
                indLeg.withInitialNotionalFixing(indexing.initialNotionalFixing());
            indLeg.withFixingDays(indexing.fixingDays());
            indLeg.inArrearsFixing(indexing.inArrearsFixing());
            if (indexing.valuationSchedule().hasData())
                indLeg.withValuationSchedule(makeSchedule(indexing.valuationSchedule(), openEndDateReplacement));
            if (!indexing.fixingCalendar().empty())
                indLeg.withFixingCalendar(parseCalendar(indexing.fixingCalendar()));
            if (!indexing.fixingConvention().empty())
                indLeg.withFixingConvention(parseBusinessDayConvention(indexing.fixingConvention()));
            leg = indLeg;
        }
    }
}

Leg joinLegs(const std::vector<Leg>& legs) {
    Leg masterLeg;
    Size lastLeg = Null<Size>();
    for (Size i = 0; i < legs.size(); ++i) {
        // skip empty legs
        if (legs[i].empty())
            continue;
        // check if the periods of adjacent legs are consistent
        if (lastLeg != Null<Size>()) {
            auto lcpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(legs[lastLeg].back());
            auto fcpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(legs[i].front());
            QL_REQUIRE(lcpn, "joinLegs: expected coupon as last cashflow in leg #" << lastLeg);
            QL_REQUIRE(fcpn, "joinLegs: expected coupon as first cashflow in leg #" << i);
            QL_REQUIRE(lcpn->accrualEndDate() == fcpn->accrualStartDate(),
                       "joinLegs: accrual end date of last coupon in leg #"
                           << lastLeg << " (" << lcpn->accrualEndDate()
                           << ") is not equal to accrual start date of first coupon in leg #" << i << " ("
                           << fcpn->accrualStartDate() << ")");
            lastLeg = i;
        }
        // copy legs together
        masterLeg.insert(masterLeg.end(), legs[i].begin(), legs[i].end());
    }
    return masterLeg;
}

Leg buildNotionalLeg(const LegData& data, const Leg& leg, RequiredFixings& requiredFixings,
                     const QuantLib::ext::shared_ptr<Market>& market, const std::string& configuration) {

    if (!data.isNotResetXCCY()) {
        // If we have an FX resetting leg, add the notional amount at the start and end of each coupon period.
        DLOG("Building Resetting XCCY Notional leg");
        Real foreignNotional = data.foreignAmount();

        QL_REQUIRE(!data.fxIndex().empty(), "buildNotionalLeg(): need fx index for fx resetting leg");
        auto fxIndex =
            buildFxIndex(data.fxIndex(), data.currency(), data.foreignCurrency(), market, configuration, true);

        PaymentLag notionalPayLag = parsePaymentLag(data.notionalPaymentLag());
        Natural payLagInteger = boost::apply_visitor(PaymentLagInteger(), notionalPayLag);
        const Calendar& payCalendar = parseCalendar(data.paymentCalendar());
        const BusinessDayConvention& payConvention = parseBusinessDayConvention(data.paymentConvention());

        Leg resettingLeg;
        for (Size j = 0; j < leg.size(); j++) {

            QuantLib::ext::shared_ptr<Coupon> c = QuantLib::ext::dynamic_pointer_cast<Coupon>(leg[j]);
            QL_REQUIRE(c, "Expected each cashflow in FX resetting leg to be of type Coupon");

            const Date& initFlowDate = payCalendar.advance(c->accrualStartDate(), payLagInteger, Days, payConvention);
            const Date& finalFlowDate = payCalendar.advance(c->accrualEndDate(), payLagInteger, Days, payConvention);

            // Build a pair of notional flows, one at the start and one at the end of the accrual period.
            // They both have the same FX fixing date => same amount in this leg's currency.
            QuantLib::ext::shared_ptr<CashFlow> outCf;
            QuantLib::ext::shared_ptr<CashFlow> inCf;
            Date fixingDate;
            if (j == 0) {

                // Two possibilities for first coupon:
                // 1. we have not been given a domestic notional so it is an FX linked coupon
                // 2. we have been given an explicit domestic notional so it is a simple cashflow
                if (data.notionals().size() == 0) {
                    fixingDate = fxIndex->fixingDate(c->accrualStartDate());
                    if (data.notionalInitialExchange()) {
                        outCf = QuantLib::ext::make_shared<FXLinkedCashFlow>(initFlowDate, fixingDate, -foreignNotional,
                                                                             fxIndex);
                    }
                    // if there is only one period we generate the cash flow at the period end
                    // only if there is a final notional exchange
                    if (leg.size() > 1 || data.notionalFinalExchange()) {
                        inCf = QuantLib::ext::make_shared<FXLinkedCashFlow>(finalFlowDate, fixingDate, foreignNotional,
                                                                            fxIndex);
                    }
                } else {
                    if (data.notionalInitialExchange()) {
                        outCf = QuantLib::ext::make_shared<SimpleCashFlow>(-c->nominal(), initFlowDate);
                    }
                    if (leg.size() > 1 || data.notionalFinalExchange()) {
                        inCf = QuantLib::ext::make_shared<SimpleCashFlow>(c->nominal(), finalFlowDate);
                    }
                }
            } else {
                fixingDate = fxIndex->fixingDate(c->accrualStartDate());
                outCf =
                    QuantLib::ext::make_shared<FXLinkedCashFlow>(initFlowDate, fixingDate, -foreignNotional, fxIndex);
                // we don't want a final one, unless there is notional exchange
                if (j < leg.size() - 1 || data.notionalFinalExchange()) {
                    inCf =
                        QuantLib::ext::make_shared<FXLinkedCashFlow>(finalFlowDate, fixingDate, foreignNotional, fxIndex);
                }
            }

            // Add the cashflows to the notional leg if they have been populated
            if (outCf) {
                resettingLeg.push_back(outCf);
                if (fixingDate != Date())
                    requiredFixings.addFixingDate(fixingDate, data.fxIndex(), outCf->date());
            }
            if (inCf) {
                resettingLeg.push_back(inCf);
                if (fixingDate != Date())
                    requiredFixings.addFixingDate(fixingDate, data.fxIndex(), inCf->date());
            }
        }

        if (data.notionalAmortizingExchange()) {
            QL_FAIL("Cannot have an amortizing notional with FX reset");
        }

        return resettingLeg;

    } else if ((data.notionalInitialExchange() || data.notionalFinalExchange() || data.notionalAmortizingExchange()) &&
               (data.legType() != "CPI")) {

        // check for notional exchanges on non FX reseting trades

        PaymentLag notionalPayLag = parsePaymentLag(data.notionalPaymentLag());
        Natural notionalPayLagInteger = boost::apply_visitor(PaymentLagInteger(), notionalPayLag);

        return makeNotionalLeg(leg, data.notionalInitialExchange(), data.notionalFinalExchange(),
                               data.notionalAmortizingExchange(), notionalPayLagInteger,
                               parseBusinessDayConvention(data.paymentConvention()),
                               parseCalendar(data.paymentCalendar()), true);
    } else {
        return Leg();
    }
}

namespace {
std::string getCmbLegSecurity(const std::string& genericBond) {
    return genericBond.substr(0, genericBond.find_last_of('-'));
}

QuantLib::ext::shared_ptr<BondReferenceDatum> getCmbLegRefData(const CMBLegData& cmbData,
                                                       const QuantLib::ext::shared_ptr<ReferenceDataManager>& refData) {
    QL_REQUIRE(refData, "getCmbLegCreditQualifierMapping(): reference data is null");
    std::string security = getCmbLegSecurity(cmbData.genericBond());
    if (refData->hasData(ore::data::BondReferenceDatum::TYPE, security)) {
        auto bondRefData = QuantLib::ext::dynamic_pointer_cast<ore::data::BondReferenceDatum>(
            refData->getData(ore::data::BondReferenceDatum::TYPE, security));
        QL_REQUIRE(bondRefData != nullptr, "getCmbLegRefData(): internal error, could not cast to BondReferenceDatum");
        return bondRefData;
    }
    return nullptr;
}
} // namespace

std::string getCmbLegCreditRiskCurrency(const CMBLegData& ld, const QuantLib::ext::shared_ptr<ReferenceDataManager>& refData) {
    if (auto bondRefData = getCmbLegRefData(ld, refData)) {
        std::string security = getCmbLegSecurity(ld.genericBond());
        BondData bd(security, 1.0);
        bd.populateFromBondReferenceData(bondRefData);
        return bd.currency();
    }
    return std::string();
}

std::pair<std::string, SimmCreditQualifierMapping>
getCmbLegCreditQualifierMapping(const CMBLegData& ld, const QuantLib::ext::shared_ptr<ReferenceDataManager>& refData,
                                const std::string& tradeId, const std::string& tradeType) {
    string source;
    ore::data::SimmCreditQualifierMapping target;
    std::string security = getCmbLegSecurity(ld.genericBond());
    if (auto bondRefData = getCmbLegRefData(ld, refData)) {
        source = ore::data::securitySpecificCreditCurveName(security, bondRefData->bondData().creditCurveId);
        target.targetQualifier = security;
        target.creditGroup = bondRefData->bondData().creditGroup;
    }
    if (source.empty() || target.targetQualifier.empty()) {
        ore::data::StructuredTradeErrorMessage(tradeId, tradeType, "getCmbLegCreditQualifierMapping()",
                                                    "Could not set mapping for CMB Leg security '" +
                                                        security +
                                                   "'. Check security name and reference data.")
            .log();
    }
    return std::make_pair(source, target);
}

} // namespace data
} // namespace ore
