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
#include <ored/portfolio/builders/capflooredcpileg.hpp>
#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ored/portfolio/builders/capflooredovernightindexedcouponleg.hpp>
#include <ored/portfolio/builders/capflooredyoyleg.hpp>
#include <ored/portfolio/builders/cms.hpp>
#include <ored/portfolio/builders/cmsspread.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/make_shared.hpp>
#include <ql/cashflow.hpp>
#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/cpicouponpricer.hpp>
#include <ql/cashflows/digitalcoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/errors.hpp>
#include <ql/experimental/coupons/cmsspreadcoupon.hpp>
#include <ql/experimental/coupons/digitalcmsspreadcoupon.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/version.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/brlcdicouponpricer.hpp>
#include <qle/cashflows/couponpricer.hpp>
#include <qle/cashflows/cpicoupon.hpp>
#include <qle/cashflows/cpicouponpricer.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/floatingannuitycoupon.hpp>
#include <qle/cashflows/indexedcoupon.hpp>

#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/strippedcapflooredcpicoupon.hpp>
#include <qle/cashflows/strippedcapflooredyoyinflationcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/cashflows/subperiodscouponpricer.hpp>
#include <qle/indexes/bmaindexwrapper.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

LegDataRegister<CashflowData> CashflowData::reg_("Cashflow");

void CashflowData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    amounts_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Cashflow", "Amount", "date", dates_, &parseReal, false);
}

XMLNode* CashflowData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Cashflow", "Amount", amounts_, "date", dates_);
    return node;
}

LegDataRegister<FixedLegData> FixedLegData::reg_("Fixed");

void FixedLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    rates_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Rates", "Rate", "startDate", rateDates_, parseReal,
                                                             true);
}

XMLNode* FixedLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    return node;
}

LegDataRegister<ZeroCouponFixedLegData> ZeroCouponFixedLegData::reg_("ZeroCouponFixed");

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

XMLNode* ZeroCouponFixedLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    XMLUtils::addChild(doc, node, "Compounding", compounding_);
    XMLUtils::addChild(doc, node, "SubtractNotional", subtractNotional_);
    return node;
}

LegDataRegister<FloatingLegData> FloatingLegData::reg_("Floating");

void FloatingLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = internalIndexName(XMLUtils::getChildValue(node, "Index", true));
    indices_.insert(index_);
    // These are all optional
    spreads_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Spreads", "Spread", "startDate", spreadDates_,
                                                               &parseReal);
    isInArrears_ = isAveraged_ = hasSubPeriods_ = includeSpread_ = false;
    if (XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears"))
        isInArrears_ = parseBool(XMLUtils::getNodeValue(arrNode));
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
}

XMLNode* FloatingLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChild(doc, node, "IsInArrears", isInArrears_);
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
    return node;
}

LegDataRegister<CPILegData> CPILegData::reg_("CPI");

void CPILegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = XMLUtils::getChildValue(node, "Index", true);
    startDate_ = XMLUtils::getChildValue(node, "StartDate", false);
    indices_.insert(index_);
    baseCPI_ = XMLUtils::getChildValueAsDouble(node, "BaseCPI", true);
    observationLag_ = XMLUtils::getChildValue(node, "ObservationLag", true);
    // for backwards compatibility only
    if (auto c = XMLUtils::getChildNode(node, "Interpolated")) {
        QL_REQUIRE(XMLUtils::getChildNode(node, "Interpolation") == nullptr,
                   "can not have both Interpolated and Interpolation node in CPILegData");
        interpolation_ = parseBool(XMLUtils::getNodeValue(c)) ? "Linear" : "Flat";
    } else {
        interpolation_ = XMLUtils::getChildValue(node, "Interpolation", true);
    }
    XMLNode* subNomNode = XMLUtils::getChildNode(node, "SubtractInflationNotional");
    if (subNomNode)
        subtractInflationNominal_ = XMLUtils::getChildValueAsBool(node, "SubtractInflationNotional", true);
    else
        subtractInflationNominal_ = false;
    rates_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Rates", "Rate", "startDate", rateDates_, &parseReal,
                                                             true);
    caps_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Caps", "Cap", "startDate", capDates_, &parseReal);
    floors_ =
        XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Floors", "Floor", "startDate", floorDates_, &parseReal);
    if (XMLUtils::getChildNode(node, "NakedOption"))
        nakedOption_ = XMLUtils::getChildValueAsBool(node, "NakedOption", false);
    else
        nakedOption_ = false;
}

XMLNode* CPILegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    XMLUtils::addChild(doc, node, "BaseCPI", baseCPI_);
    XMLUtils::addChild(doc, node, "StartDate", startDate_);
    XMLUtils::addChild(doc, node, "ObservationLag", observationLag_);
    XMLUtils::addChild(doc, node, "Interpolation", interpolation_);
    XMLUtils::addChild(doc, node, "SubtractInflationNotional", subtractInflationNominal_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    return node;
}

LegDataRegister<YoYLegData> YoYLegData::reg_("YY");

void YoYLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = XMLUtils::getChildValue(node, "Index", true);
    indices_.insert(index_);
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays", true);
    observationLag_ = XMLUtils::getChildValue(node, "ObservationLag", true);
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
}

XMLNode* YoYLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChild(doc, node, "ObservationLag", observationLag_);
    XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                gearingDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate", spreadDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    return node;
}

XMLNode* CMSLegData::toXML(XMLDocument& doc) {
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

LegDataRegister<CMSLegData> CMSLegData::reg_("CMS");

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

XMLNode* CMSSpreadLegData::toXML(XMLDocument& doc) {
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

LegDataRegister<CMSSpreadLegData> CMSSpreadLegData::reg_("CMSSpread");

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

XMLNode* DigitalCMSSpreadLegData::toXML(XMLDocument& doc) {
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

LegDataRegister<DigitalCMSSpreadLegData> DigitalCMSSpreadLegData::reg_("DigitalCMSSpread");

void DigitalCMSSpreadLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());

    XMLNode* underlyingNode = XMLUtils::getChildNode(node, "CMSSpreadLegData");
    underlying_ = boost::make_shared<CMSSpreadLegData>();
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

LegDataRegister<EquityLegData> EquityLegData::reg_("Equity");

void EquityLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    returnType_ = XMLUtils::getChildValue(node, "ReturnType");
    if (returnType_ == "Total" && XMLUtils::getChildNode(node, "DividendFactor"))
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
        eqCurrency_ = XMLUtils::getChildValue(fxt, "EquityCurrency", true);
        fxIndex_ = XMLUtils::getChildValue(fxt, "FXIndex", true);
        fxIndexFixingDays_ = XMLUtils::getChildValueAsInt(fxt, "FXIndexFixingDays");
        fxIndexCalendar_ = XMLUtils::getChildValue(fxt, "FXIndexCalendar");
        indices_.insert(fxIndex_);
    }

    if (XMLNode* qty = XMLUtils::getChildNode(node, "Quantity")) {
        quantity_ = parseReal(XMLUtils::getNodeValue(qty));
    } else {
        quantity_ = Null<Real>();
    }
}

XMLNode* EquityLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    if (quantity_ != Null<Real>()) {
        XMLUtils::addChild(doc, node, "Quantity", quantity_);
    }
    XMLUtils::addChild(doc, node, "ReturnType", returnType_);
    if (returnType_ == "Total") {
        XMLUtils::addChild(doc, node, "DividendFactor", dividendFactor_);
    }
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
        if (fxIndexFixingDays_)
            XMLUtils::addChild(doc, fxNode, "FXIndexFixingDays", static_cast<Integer>(fxIndexFixingDays_));
        if (fxIndexCalendar_ != "")
            XMLUtils::addChild(doc, fxNode, "FXIndexCalendar", fxIndexCalendar_);
        XMLUtils::appendNode(node, fxNode);
    }
    return node;
}

void AmortizationData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "AmortizationData");
    type_ = XMLUtils::getChildValue(node, "Type");
    value_ = XMLUtils::getChildValueAsDouble(node, "Value");
    startDate_ = XMLUtils::getChildValue(node, "StartDate");
    endDate_ = XMLUtils::getChildValue(node, "EndDate", false);     // optional
    frequency_ = XMLUtils::getChildValue(node, "Frequency", false); // optional
    underflow_ = XMLUtils::getChildValueAsBool(node, "Underflow");
    initialized_ = true;
}

XMLNode* AmortizationData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("AmortizationData");
    XMLUtils::addChild(doc, node, "Type", type_);
    XMLUtils::addChild(doc, node, "Value", value_);
    XMLUtils::addChild(doc, node, "StartDate", startDate_);
    if (endDate_ != "")
        XMLUtils::addChild(doc, node, "EndDate", endDate_);
    if (frequency_ != "")
        XMLUtils::addChild(doc, node, "Frequency", frequency_);
    XMLUtils::addChild(doc, node, "Underflow", underflow_);
    return node;
}

LegData::LegData(const boost::shared_ptr<LegAdditionalData>& concreteLegData, bool isPayer, const string& currency,
                 const ScheduleData& scheduleData, const string& dayCounter, const std::vector<double>& notionals,
                 const std::vector<string>& notionalDates, const string& paymentConvention,
                 const bool notionalInitialExchange, const bool notionalFinalExchange,
                 const bool notionalAmortizingExchange, const bool isNotResetXCCY, const string& foreignCurrency,
                 const double foreignAmount, const string& fxIndex, int fixingDays, const string& fixingCalendar,
                 const std::vector<AmortizationData>& amortizationData, const int paymentLag,
                 const string& paymentCalendar, const vector<string>& paymentDates,
                 const std::vector<Indexing>& indexing, const bool indexingFromAssetLeg)
    : concreteLegData_(concreteLegData), isPayer_(isPayer), currency_(currency), schedule_(scheduleData),
      dayCounter_(dayCounter), notionals_(notionals), notionalDates_(notionalDates),
      paymentConvention_(paymentConvention), notionalInitialExchange_(notionalInitialExchange),
      notionalFinalExchange_(notionalFinalExchange), notionalAmortizingExchange_(notionalAmortizingExchange),
      isNotResetXCCY_(isNotResetXCCY), foreignCurrency_(foreignCurrency), foreignAmount_(foreignAmount),
      fxIndex_(fxIndex), fixingDays_(fixingDays), fixingCalendar_(fixingCalendar), amortizationData_(amortizationData),
      paymentLag_(paymentLag), paymentCalendar_(paymentCalendar), paymentDates_(paymentDates), indexing_(indexing),
      indexingFromAssetLeg_(indexingFromAssetLeg) {

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
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    dayCounter_ = XMLUtils::getChildValue(node, "DayCounter"); // optional
    paymentConvention_ = XMLUtils::getChildValue(node, "PaymentConvention");
    paymentLag_ = XMLUtils::getChildValueAsInt(node, "PaymentLag");
    paymentCalendar_ = XMLUtils::getChildValue(node, "PaymentCalendar", false);
    // if not given, default of getChildValueAsBool is true, which fits our needs here
    notionals_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Notionals", "Notional", "startDate",
                                                                 notionalDates_, &parseReal);
    XMLNode* tmp = XMLUtils::getChildNode(node, "Notionals");
    isNotResetXCCY_ = true;
    notionalInitialExchange_ = false;
    notionalFinalExchange_ = false;
    notionalAmortizingExchange_ = false;
    if (tmp) {
        XMLNode* fxResetNode = XMLUtils::getChildNode(tmp, "FXReset");
        if (fxResetNode) {
            isNotResetXCCY_ = false;
            foreignCurrency_ = XMLUtils::getChildValue(fxResetNode, "ForeignCurrency", true);
            foreignAmount_ = XMLUtils::getChildValueAsDouble(fxResetNode, "ForeignAmount", true);
            fxIndex_ = XMLUtils::getChildValue(fxResetNode, "FXIndex", true);
            indices_.insert(fxIndex_);
            fixingDays_ = XMLUtils::getChildValueAsInt(fxResetNode, "FixingDays");
            fixingCalendar_ = XMLUtils::getChildValue(fxResetNode, "FixingCalendar"); // may be empty string
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

    tmp = XMLUtils::getChildNode(node, "ScheduleData");
    if (tmp)
        schedule_.fromXML(tmp);

    paymentDates_ = XMLUtils::getChildrenValues(node, "PaymentDates", "PaymentDate", false);

    tmp = XMLUtils::getChildNode(node, "Indexings");
    if (tmp) {
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

    concreteLegData_ = initialiseConcreteLegData(legType);
    concreteLegData_->fromXML(XMLUtils::getChildNode(node, concreteLegData_->legNodeName()));

    indices_.insert(concreteLegData_->indices().begin(), concreteLegData_->indices().end());
}

boost::shared_ptr<LegAdditionalData> LegData::initialiseConcreteLegData(const string& legType) {
    auto legData = LegDataFactory::instance().build(legType);
    QL_REQUIRE(legData, "Leg type " << legType << " has not been registered with the leg data factory.");
    return legData;
}

XMLNode* LegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("LegData");
    QL_REQUIRE(node, "Failed to create LegData node");
    XMLUtils::addChild(doc, node, "LegType", legType());
    XMLUtils::addChild(doc, node, "Payer", isPayer_);
    XMLUtils::addChild(doc, node, "Currency", currency_);
    if (paymentConvention_ != "")
        XMLUtils::addChild(doc, node, "PaymentConvention", paymentConvention_);
    if (paymentLag_ != 0)
        XMLUtils::addChild(doc, node, "PaymentLag", paymentLag_);
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
        XMLUtils::addChild(doc, resetNode, "FixingDays", fixingDays_);
        XMLUtils::addChild(doc, resetNode, "FixingCalendar", fixingCalendar_);
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

    XMLUtils::appendNode(node, concreteLegData_->toXML(doc));
    return node;
}

// Functions
Leg makeSimpleLeg(const LegData& data) {
    boost::shared_ptr<CashflowData> cashflowData = boost::dynamic_pointer_cast<CashflowData>(data.concreteLegData());
    QL_REQUIRE(cashflowData, "Wrong LegType, expected CashFlow, got " << data.legType());

    const vector<double>& amounts = cashflowData->amounts();
    const vector<string>& dates = cashflowData->dates();
    QL_REQUIRE(amounts.size() == dates.size(), "Amounts / Date size mismatch in makeSimpleLeg."
                                                   << "Amounts:" << amounts.size() << ", Dates:" << dates.size());
    Leg leg;
    for (Size i = 0; i < dates.size(); i++) {
        Date d = parseDate(dates[i]);
        leg.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(amounts[i], d)));
    }
    return leg;
}

Leg makeFixedLeg(const LegData& data) {
    boost::shared_ptr<FixedLegData> fixedLegData = boost::dynamic_pointer_cast<FixedLegData>(data.concreteLegData());
    QL_REQUIRE(fixedLegData, "Wrong LegType, expected Fixed, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Calendar paymentCalendar = schedule.calendar();
    vector<double> rates = buildScheduledVector(fixedLegData->rates(), fixedLegData->rateDates(), schedule);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    Natural paymentLag = data.paymentLag();
    applyAmortization(notionals, data, schedule, true, rates);
    Leg leg = FixedRateLeg(schedule)
                  .withNotionals(notionals)
                  .withCouponRates(rates, dc)
                  .withPaymentAdjustment(bdc)
                  .withPaymentLag(paymentLag)
                  .withPaymentCalendar(paymentCalendar);
    return leg;
}

Leg makeZCFixedLeg(const LegData& data) {
    boost::shared_ptr<ZeroCouponFixedLegData> zcFixedLegData =
        boost::dynamic_pointer_cast<ZeroCouponFixedLegData>(data.concreteLegData());
    QL_REQUIRE(zcFixedLegData, "Wrong LegType, expected Zero Coupon Fixed, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    // check we have a single notional and two dates in the schedule
    vector<double> notionals = data.notionals();
    int numNotionals = notionals.size();
    int numRates = zcFixedLegData->rates().size();
    int numDates = schedule.size();
    QL_REQUIRE(numDates >= 2, "Incorrect number of schedule dates entered, expected at least 2, got " << numDates);
    QL_REQUIRE(numNotionals == 1, "Incorrect number of notional values entered, expected 1, got " << numNotionals);
    QL_REQUIRE(numRates == 1, "Incorrect number of rate values entered, expected 1, got " << numRates);

    // coupon
    Rate fixedRate = zcFixedLegData->rates()[0];
    Real fixedAmount = notionals[0];
    vector<Date> dates = schedule.dates();

    Compounding comp = parseCompounding(zcFixedLegData->compounding());
    QL_REQUIRE(comp == QuantLib::Compounded || comp == QuantLib::Simple,
               "Compounding method " << zcFixedLegData->compounding() << " not supported");

    // we loop over the dates in the schedule, computing the compound factor.
    // For the Compounded rule:
    // (1+r)^dcf_0 *  (1+r)^dcf_1 * ... = (1+r)^(dcf_0 + dcf_1 + ...)
    // So we compute the sum of all DayCountFractions in the loop.
    // For the Simple rule:
    // (1 + r * dcf_0) * (1 + r * dcf_1)...
    double totalDCF = 0;
    double compoundFactor = 1;
    for (Size i = 0; i < dates.size() - 1; i++) {
        double dcf = dc.yearFraction(dates[i], dates[i + 1]);
        if (comp == QuantLib::Simple)
            compoundFactor *= (1 + fixedRate * dcf);
        else
            totalDCF += dcf;
    }
    if (comp == QuantLib::Compounded)
        compoundFactor = pow(1.0 + fixedRate, totalDCF);
    if (zcFixedLegData->subtractNotional())
        fixedAmount *= (compoundFactor - 1);
    else
        fixedAmount *= compoundFactor;
    Date maturity = schedule.endDate();
    Date fixedPayDate = schedule.calendar().adjust(maturity, bdc);

    Leg leg;
    leg.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(fixedAmount, fixedPayDate)));

    return leg;
}

Leg makeIborLeg(const LegData& data, const boost::shared_ptr<IborIndex>& index,
                const boost::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer) {
    boost::shared_ptr<FloatingLegData> floatData = boost::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());

    bool hasCapsFloors = floatData->caps().size() > 0 || floatData->floors().size() > 0;
    vector<double> notionals = buildScheduledVectorNormalised(data.notionals(), data.notionalDates(), schedule, 0.0);
    vector<double> spreads =
        buildScheduledVectorNormalised(floatData->spreads(), floatData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        buildScheduledVectorNormalised(floatData->gearings(), floatData->gearingDates(), schedule, 1.0);
    Size fixingDays = floatData->fixingDays() == Null<Size>() ? index->fixingDays() : floatData->fixingDays();

    applyAmortization(notionals, data, schedule, true);
    // handle float annuity, which is not done in applyAmortization, for this we can only have one block
    if (!data.amortizationData().empty()) {
        AmortizationType amortizationType = parseAmortizationType(data.amortizationData().front().type());
        if (amortizationType == AmortizationType::Annuity) {
            LOG("Build floating annuity notional schedule");
            QL_REQUIRE(!hasCapsFloors, "Caps/Floors not supported in floating annuity coupons");
            QL_REQUIRE(floatData->gearings().size() == 0, "Gearings not supported in floating annuity coupons");
            DayCounter dc = index->dayCounter();
            Date startDate = parseDate(data.amortizationData().front().startDate());
            double annuity = data.amortizationData().front().value();
            bool underflow = data.amortizationData().front().underflow();
            vector<boost::shared_ptr<Coupon>> coupons;
            for (Size i = 0; i < schedule.size() - 1; i++) {
                Date paymentDate = schedule.calendar().adjust(schedule[i + 1], bdc);
                if (schedule[i] < startDate || i == 0) {
                    boost::shared_ptr<FloatingRateCoupon> coupon;
                    if (!floatData->hasSubPeriods()) {
                        coupon = boost::make_shared<IborCoupon>(paymentDate, notionals[i], schedule[i], schedule[i + 1],
                                                                fixingDays, index, gearings[i], spreads[i], Date(),
                                                                Date(), dc, floatData->isInArrears());
                        coupon->setPricer(boost::make_shared<BlackIborCouponPricer>());
                    } else {
                        coupon = boost::make_shared<SubPeriodsCoupon>(
                            paymentDate, notionals[i], schedule[i], schedule[i + 1], index,
                            floatData->isAveraged() ? SubPeriodsCoupon::Averaging : SubPeriodsCoupon::Compounding,
                            index->businessDayConvention(), spreads[i], dc, floatData->includeSpread(), gearings[i]);
                        coupon->setPricer(boost::make_shared<SubPeriodsCouponPricer>());
                    }
                    coupons.push_back(coupon);
                    LOG("FloatingAnnuityCoupon: " << i << " " << coupon->nominal() << " " << coupon->amount());
                } else {
                    QL_REQUIRE(coupons.size() > 0,
                               "FloatingAnnuityCoupon needs at least one predecessor, e.g. a plain IborCoupon");
                    LOG("FloatingAnnuityCoupon, previous nominal/coupon: " << i << " " << coupons.back()->nominal()
                                                                           << " " << coupons.back()->amount());
                    boost::shared_ptr<QuantExt::FloatingAnnuityCoupon> coupon =
                        boost::make_shared<QuantExt::FloatingAnnuityCoupon>(
                            annuity, underflow, coupons.back(), paymentDate, schedule[i], schedule[i + 1], fixingDays,
                            index, gearings[i], spreads[i], Date(), Date(), dc, floatData->isInArrears());
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

    if (floatData->hasSubPeriods()) {
        QL_REQUIRE(floatData->caps().empty() && floatData->floors().empty(),
                   "SubPeriodsLegs does not support caps or floors");
        QL_REQUIRE(!floatData->isInArrears(), "SubPeriodLegs do not support in aarears fixings");
        Leg leg = SubPeriodsLeg(schedule, index)
                      .withNotionals(notionals)
                      .withPaymentDayCounter(dc)
                      .withPaymentAdjustment(bdc)
                      .withGearings(gearings)
                      .withSpreads(spreads)
                      .withType(floatData->isAveraged() ? SubPeriodsCoupon::Averaging : SubPeriodsCoupon::Compounding)
                      .includeSpread(floatData->includeSpread());
        QuantExt::setCouponPricer(leg, boost::make_shared<SubPeriodsCouponPricer>());
        return leg;
    }

    IborLeg iborLeg = IborLeg(schedule, index)
                          .withNotionals(notionals)
                          .withSpreads(spreads)
                          .withPaymentDayCounter(dc)
                          .withPaymentAdjustment(bdc)
                          .withFixingDays(fixingDays)
                          .inArrears(floatData->isInArrears())
                          .withGearings(gearings)
                          .withPaymentLag(data.paymentLag());

    // If no caps or floors or in arrears fixing, return the leg
    if (!hasCapsFloors && !floatData->isInArrears())
        return iborLeg;

    // If there are caps or floors or in arrears fixing, add them and set pricer
    if (floatData->caps().size() > 0)
        iborLeg.withCaps(buildScheduledVector(floatData->caps(), floatData->capDates(), schedule));

    if (floatData->floors().size() > 0)
        iborLeg.withFloors(buildScheduledVector(floatData->floors(), floatData->floorDates(), schedule));

    if (!attachPricer)
        return iborLeg;

    // Get a coupon pricer for the leg
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("CapFlooredIborLeg");
    QL_REQUIRE(builder, "No builder found for CapFlooredIborLeg");
    boost::shared_ptr<CapFlooredIborLegEngineBuilder> cappedFlooredIborBuilder =
        boost::dynamic_pointer_cast<CapFlooredIborLegEngineBuilder>(builder);
    boost::shared_ptr<FloatingRateCouponPricer> couponPricer = cappedFlooredIborBuilder->engine(index->currency());

    // Loop over the coupons in the leg and set pricer
    Leg tmpLeg = iborLeg;
    QuantLib::setCouponPricer(tmpLeg, couponPricer);

    // build naked option leg if required
    if (floatData->nakedOption()) {
        tmpLeg = StrippedCappedFlooredCouponLeg(tmpLeg);
        // fix for missing registration in ql 1.13
        for (auto const& t : tmpLeg) {
            auto s = boost::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(t);
            if (s != nullptr)
                s->registerWith(s->underlying());
        }
    }
    return tmpLeg;
}

Leg makeOISLeg(const LegData& data, const boost::shared_ptr<OvernightIndex>& index,
               const boost::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer) {
    boost::shared_ptr<FloatingLegData> floatData = boost::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Natural paymentLag = data.paymentLag();
    Calendar paymentCalendar = index->fixingCalendar();
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    vector<double> spreads =
        buildScheduledVectorNormalised(floatData->spreads(), floatData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        buildScheduledVectorNormalised(floatData->gearings(), floatData->gearingDates(), schedule, 1.0);

    applyAmortization(notionals, data, schedule, false);

    if (floatData->isAveraged()) {

        if (floatData->caps().size() > 0 || floatData->floors().size() > 0)
            QL_FAIL("Caps and floors are not supported for OIS averaged legs");

        boost::shared_ptr<QuantExt::AverageONIndexedCouponPricer> couponPricer =
            boost::make_shared<QuantExt::AverageONIndexedCouponPricer>();
        QuantExt::AverageONLeg leg =
            QuantExt::AverageONLeg(schedule, index)
                .withNotionals(notionals)
                .withSpreads(spreads)
                .withGearings(gearings)
                .withPaymentDayCounter(dc)
                .withPaymentAdjustment(bdc)
                .withPaymentLag(paymentLag)
                .withLookback(floatData->lookback())
                .withRateCutoff(floatData->rateCutoff() == Null<Size>() ? 0 : floatData->rateCutoff())
                .withFixingDays(floatData->fixingDays())
                .withAverageONIndexedCouponPricer(couponPricer);

        return leg;

    } else {

        Leg leg = QuantExt::OvernightLeg(schedule, index)
                      .withNotionals(notionals)
                      .withSpreads(spreads)
                      .withPaymentDayCounter(dc)
                      .withPaymentAdjustment(bdc)
                      .withPaymentCalendar(paymentCalendar)
                      .withPaymentLag(paymentLag)
                      .withGearings(gearings)
                      .includeSpread(floatData->includeSpread())
                      .withLookback(floatData->lookback())
                      .withFixingDays(floatData->fixingDays())
                      .withRateCutoff(floatData->rateCutoff() == Null<Size>() ? 0 : floatData->rateCutoff())
                      .withCaps(buildScheduledVectorNormalised<Real>(floatData->caps(), floatData->capDates(), schedule,
                                                                     Null<Real>()))
                      .withFloors(buildScheduledVectorNormalised<Real>(floatData->floors(), floatData->capDates(),
                                                                       schedule, Null<Real>()))
                      .withNakedOption(floatData->nakedOption());

        // If the overnight index is BRL CDI, we need a special coupon pricer
        boost::shared_ptr<BRLCdi> brlCdiIndex = boost::dynamic_pointer_cast<BRLCdi>(index);
        if (brlCdiIndex)
            QuantExt::setCouponPricer(leg, boost::make_shared<BRLCdiCouponPricer>());

        // if we have caps / floors, we need a pricer for that
        if (attachPricer && (floatData->caps().size() > 0 || floatData->floors().size() > 0)) {
            boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("CapFlooredOvernightIndexedCouponLeg");
            QL_REQUIRE(builder, "No builder found for CapFlooredOvernightIndexedCouponLeg");
            auto pricerBuilder = boost::dynamic_pointer_cast<CapFlooredOvernightIndexedCouponLegEngineBuilder>(builder);
            boost::shared_ptr<FloatingRateCouponPricer> pricer = pricerBuilder->engine(index->currency());
            QuantExt::setCouponPricer(leg, pricer);
        }

        return leg;
    }
}

Leg makeBMALeg(const LegData& data, const boost::shared_ptr<QuantExt::BMAIndexWrapper>& indexWrapper) {
    boost::shared_ptr<FloatingLegData> floatData = boost::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << data.legType());
    boost::shared_ptr<BMAIndex> index = indexWrapper->bma();

    if (floatData->caps().size() > 0 || floatData->floors().size() > 0)
        QL_FAIL("Caps and floors are not supported for BMA legs");

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());

    vector<Real> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    vector<Real> spreads = buildScheduledVector(floatData->spreads(), floatData->spreadDates(), schedule);
    vector<Real> gearings = buildScheduledVector(floatData->gearings(), floatData->gearingDates(), schedule);

    applyAmortization(notionals, data, schedule, false);

    AverageBMALeg leg = AverageBMALeg(schedule, index)
                            .withNotionals(notionals)
                            .withSpreads(spreads)
                            .withPaymentDayCounter(dc)
                            .withPaymentAdjustment(bdc)
                            .withGearings(gearings);

    return leg;
}

Leg makeNotionalLeg(const Leg& refLeg, const bool initNomFlow, const bool finalNomFlow, const bool amortNomFlow,
                    const BusinessDayConvention paymentConvention, const Calendar paymentCalendar) {

    // Assumption - Cashflows on Input Leg are all coupons
    // This is the Leg to be populated
    Leg leg;

    // Initial Flow Amount
    if (initNomFlow) {
        auto coupon = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[0]);
        QL_REQUIRE(coupon, "makeNotionalLeg does not support non-coupon legs");
        double initFlowAmt = coupon->nominal();
        Date initDate = coupon->accrualStartDate();
        initDate = paymentCalendar.adjust(initDate, paymentConvention);
        if (initFlowAmt != 0)
            leg.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(-initFlowAmt, initDate)));
    }

    // Amortization Flows
    if (amortNomFlow) {
        for (Size i = 1; i < refLeg.size(); i++) {
            auto coupon = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[i]);
            QL_REQUIRE(coupon, "makeNotionalLeg does not support non-coupon legs");
            auto coupon2 = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[i - 1]);
            QL_REQUIRE(coupon, "makeNotionalLeg does not support non-coupon legs");
            Date flowDate = coupon->accrualStartDate();
            flowDate = paymentCalendar.adjust(flowDate, paymentConvention);
            Real initNom = coupon2->nominal();
            Real newNom = coupon->nominal();
            Real flow = initNom - newNom;
            if (flow != 0)
                leg.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(flow, flowDate)));
        }
    }

    // Final Nominal Return at Maturity
    if (finalNomFlow) {
        // boost::shared_ptr<QuantLib::Coupon> coupon = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg.back());
        // if (coupon) {
        //     double finalNomFlow = coupon->nominal();
        //     Date finalDate = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg.back())->date();
        //     if (finalNomFlow != 0)
        //         leg.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(finalNomFlow, finalDate)));
        // } else {
        //     ALOG("The reference leg's last cash flow is not a coupon, we cannot create a final exchange flow");
        // }
        auto coupon = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg.back());
        QL_REQUIRE(coupon, "makeNotionalLeg does not support non-coupon legs");
        double finalNomFlow = coupon->nominal();
        Date finalDate = coupon->accrualEndDate();
        finalDate = paymentCalendar.adjust(finalDate, paymentConvention);
        if (finalNomFlow != 0)
            leg.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(finalNomFlow, finalDate)));
    }

    return leg;
}

Leg makeCPILeg(const LegData& data, const boost::shared_ptr<ZeroInflationIndex>& index,
               const boost::shared_ptr<EngineFactory>& engineFactory) {
    boost::shared_ptr<CPILegData> cpiLegData = boost::dynamic_pointer_cast<CPILegData>(data.concreteLegData());
    QL_REQUIRE(cpiLegData, "Wrong LegType, expected CPI, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Period observationLag = parsePeriod(cpiLegData->observationLag());
    CPI::InterpolationType interpolationMethod = parseObservationInterpolation(cpiLegData->interpolation());
    vector<double> rates = buildScheduledVector(cpiLegData->rates(), cpiLegData->rateDates(), schedule);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);

    applyAmortization(notionals, data, schedule, false);

    QuantExt::CPILeg cpiLeg = QuantExt::CPILeg(schedule, index, cpiLegData->baseCPI(), observationLag)
                                  .withNotionals(notionals)
                                  .withPaymentDayCounter(dc)
                                  .withPaymentAdjustment(bdc)
                                  .withPaymentCalendar(schedule.calendar())
                                  .withFixedRates(rates)
                                  .withObservationInterpolation(interpolationMethod)
                                  .withSubtractInflationNominal(cpiLegData->subtractInflationNominal());

    // the cpi leg uses the first schedule date as the start date, which only makes sense if there are at least
    // two dates in the schedule, otherwise the only date in the schedule is the pay date of the cf and a a separate
    // start date is expected; if both the separate start date and a schedule with more than one date is given
    if (schedule.size() < 2) {
        QL_REQUIRE(!cpiLegData->startDate().empty(),
                   "makeCPILeg(): if only one schedule date is given, a StartDate must be given in addition");
        cpiLeg.withStartDate(parseDate(cpiLegData->startDate()));
    } else {
        QL_REQUIRE(cpiLegData->startDate().empty() || parseDate(cpiLegData->startDate()) == schedule.dates().front(),
                   "makeCPILeg(): first schedule date ("
                       << schedule.dates().front() << ") must be identical to start date ("
                       << parseDate(cpiLegData->startDate())
                       << "), the start date can be omitted for schedules containing more than one date");
    }

    bool couponCap = cpiLegData->caps().size() > 0;
    bool couponFloor = cpiLegData->floors().size() > 0;
    bool couponCapFloor = cpiLegData->caps().size() > 0 || cpiLegData->floors().size() > 0;

    if (couponCap)
        cpiLeg.withCaps(buildScheduledVector(cpiLegData->caps(), cpiLegData->capDates(), schedule));

    if (couponFloor)
        cpiLeg.withFloors(buildScheduledVector(cpiLegData->floors(), cpiLegData->floorDates(), schedule));

    Leg leg = cpiLeg.operator Leg();
    Size n = leg.size();
    QL_REQUIRE(n > 0, "Empty CPI Leg");

    if (couponCapFloor) {

        string indexName = index->name();
        // remove blanks (FIXME)
        indexName.replace(indexName.find(" ", 0), 1, "");

        // get a coupon pricer for the leg
        boost::shared_ptr<EngineBuilder> cpBuilder = engineFactory->builder("CappedFlooredCpiLegCoupons");
        QL_REQUIRE(cpBuilder, "No builder found for CappedFlooredCpiLegCoupons");
        boost::shared_ptr<CapFlooredCpiLegCouponEngineBuilder> cappedFlooredCpiCouponBuilder =
            boost::dynamic_pointer_cast<CapFlooredCpiLegCouponEngineBuilder>(cpBuilder);
        boost::shared_ptr<InflationCouponPricer> couponPricer = cappedFlooredCpiCouponBuilder->engine(indexName);

        // get a cash flow pricer for the leg
        boost::shared_ptr<EngineBuilder> cfBuilder = engineFactory->builder("CappedFlooredCpiLegCashFlows");
        QL_REQUIRE(cfBuilder, "No builder found for CappedFlooredCpiLegCashFLows");
        boost::shared_ptr<CapFlooredCpiLegCashFlowEngineBuilder> cappedFlooredCpiCashFlowBuilder =
            boost::dynamic_pointer_cast<CapFlooredCpiLegCashFlowEngineBuilder>(cfBuilder);
        boost::shared_ptr<InflationCashFlowPricer> cashFlowPricer = cappedFlooredCpiCashFlowBuilder->engine(indexName);

        // set coupon pricer for the leg
        for (Size i = 0; i < leg.size(); i++) {
            // nothing to do for the plain CPI Coupon, because the pricer is already set when the leg bilder is called
            // nothing to do for the plain CPI CashFlow either, because it does not require a pricer

            boost::shared_ptr<CappedFlooredCPICoupon> cfCpiCoupon =
                boost::dynamic_pointer_cast<CappedFlooredCPICoupon>(leg[i]);
            if (cfCpiCoupon) {
                cfCpiCoupon->setPricer(couponPricer);
            } else {
                boost::shared_ptr<CappedFlooredCPICashFlow> cfCpiCashFlow =
                    boost::dynamic_pointer_cast<CappedFlooredCPICashFlow>(leg[i]);
                if (cfCpiCashFlow) {
                    cfCpiCashFlow->setPricer(cashFlowPricer);
                }
            }
        }
    }

    // QuantLib CPILeg automatically adds a Notional Cashflow at maturity date on a CPI swap
    // If Notional Exchange set to false, remove the final cashflow.
    if (!data.notionalFinalExchange()) {
        // QL_REQUIRE(n > 1, "Cannot have Notional Final Exchange with just a single cashflow");
        // boost::shared_ptr<CPICashFlow> cpicf = boost::dynamic_pointer_cast<CPICashFlow>(leg[n - 1]);
        // We do not need this check that the last coupon payment date matches the final CPI cash flow date, this is
        // identical by construction, see QuantLib::CPILeg or QuantExt::CPILeg.
        // Moreover, we may have no coupons and just a single fow at the end so that leg[n - 2] causes a problem.
        // boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(leg[n - 2]);
        // if (cpicf && (cpicf->date() == coupon->date()))
        //   leg.pop_back();
        leg.pop_back();
    }

    // build naked option leg if required
    if (couponCapFloor && cpiLegData->nakedOption()) {
        leg = StrippedCappedFlooredCPICouponLeg(leg);
        // fix for missing registration in ql 1.13
        for (auto const& t : leg) {
            auto s = boost::dynamic_pointer_cast<StrippedCappedFlooredCPICoupon>(t);
            if (s != nullptr)
                s->registerWith(s->underlying());
        }
    }

    return leg;
}

Leg makeYoYLeg(const LegData& data, const boost::shared_ptr<YoYInflationIndex>& index,
               const boost::shared_ptr<EngineFactory>& engineFactory) {
    boost::shared_ptr<YoYLegData> yoyLegData = boost::dynamic_pointer_cast<YoYLegData>(data.concreteLegData());
    QL_REQUIRE(yoyLegData, "Wrong LegType, expected YoY, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Period observationLag = parsePeriod(yoyLegData->observationLag());
    vector<double> gearings =
        buildScheduledVectorNormalised(yoyLegData->gearings(), yoyLegData->gearingDates(), schedule, 1.0);
    vector<double> spreads =
        buildScheduledVectorNormalised(yoyLegData->spreads(), yoyLegData->spreadDates(), schedule, 0.0);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);

    bool couponCap = yoyLegData->caps().size() > 0;
    bool couponFloor = yoyLegData->floors().size() > 0;
    bool couponCapFloor = yoyLegData->caps().size() > 0 || yoyLegData->floors().size() > 0;

    applyAmortization(notionals, data, schedule, false);

    yoyInflationLeg yoyLeg = yoyInflationLeg(schedule, schedule.calendar(), index, observationLag)
                                 .withNotionals(notionals)
                                 .withPaymentDayCounter(dc)
                                 .withPaymentAdjustment(bdc)
                                 .withFixingDays(yoyLegData->fixingDays())
                                 .withGearings(gearings)
                                 .withSpreads(spreads);

    if (couponCap)
        yoyLeg.withCaps(buildScheduledVector(yoyLegData->caps(), yoyLegData->capDates(), schedule));

    if (couponFloor)
        yoyLeg.withFloors(buildScheduledVector(yoyLegData->floors(), yoyLegData->floorDates(), schedule));

    if (couponCapFloor) {
        // get a coupon pricer for the leg
        boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("CapFlooredYYLeg");
        QL_REQUIRE(builder, "No builder found for CapFlooredYYLeg");
        boost::shared_ptr<CapFlooredYoYLegEngineBuilder> cappedFlooredYoYBuilder =
            boost::dynamic_pointer_cast<CapFlooredYoYLegEngineBuilder>(builder);
        string indexname = index->name();
        boost::shared_ptr<InflationCouponPricer> couponPricer =
            cappedFlooredYoYBuilder->engine(indexname.replace(indexname.find(" ", 0), 1, ""));

        // set coupon pricer for the leg
        Leg leg = yoyLeg.operator Leg();
        for (Size i = 0; i < leg.size(); i++) {
            boost::dynamic_pointer_cast<CappedFlooredYoYInflationCoupon>(leg[i])->setPricer(
                boost::dynamic_pointer_cast<QuantLib::YoYInflationCouponPricer>(couponPricer));
        }

        // build naked option leg if required
        if (yoyLegData->nakedOption()) {
            leg = StrippedCappedFlooredYoYInflationCouponLeg(leg);
            for (auto const& t : leg) {
                auto s = boost::dynamic_pointer_cast<StrippedCappedFlooredYoYInflationCoupon>(t);
            }
        }

        return leg;
    } else {
        return yoyLeg;
    }
}

Leg makeCMSLeg(const LegData& data, const boost::shared_ptr<QuantLib::SwapIndex>& swapIndex,
               const boost::shared_ptr<EngineFactory>& engineFactory, const vector<double>& caps,
               const vector<double>& floors, const bool attachPricer) {
    boost::shared_ptr<CMSLegData> cmsData = boost::dynamic_pointer_cast<CMSLegData>(data.concreteLegData());
    QL_REQUIRE(cmsData, "Wrong LegType, expected CMS, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    bool couponCapFloor = cmsData->caps().size() > 0 || cmsData->floors().size() > 0;
    bool nakedCapFloor = caps.size() > 0 || floors.size() > 0;

    vector<double> spreads =
        ore::data::buildScheduledVectorNormalised(cmsData->spreads(), cmsData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        ore::data::buildScheduledVectorNormalised(cmsData->gearings(), cmsData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    Size fixingDays = cmsData->fixingDays() == Null<Size>() ? swapIndex->fixingDays() : cmsData->fixingDays();

    applyAmortization(notionals, data, schedule, false);

    CmsLeg cmsLeg = CmsLeg(schedule, swapIndex)
                        .withNotionals(notionals)
                        .withSpreads(spreads)
                        .withGearings(gearings)
                        .withPaymentDayCounter(dc)
                        .withPaymentAdjustment(bdc)
                        .withFixingDays(fixingDays)
                        .inArrears(cmsData->isInArrears());

    if (nakedCapFloor) {
        vector<string> capFloorDates;

        if (caps.size() > 0)
            cmsLeg.withCaps(buildScheduledVector(caps, capFloorDates, schedule));

        if (floors.size() > 0)
            cmsLeg.withFloors(buildScheduledVector(floors, capFloorDates, schedule));
    } else if (couponCapFloor) {
        if (cmsData->caps().size() > 0)
            cmsLeg.withCaps(buildScheduledVector(cmsData->caps(), cmsData->capDates(), schedule));

        if (cmsData->floors().size() > 0)
            cmsLeg.withFloors(buildScheduledVector(cmsData->floors(), cmsData->floorDates(), schedule));
    }

    if (!attachPricer)
        return cmsLeg;

    // Get a coupon pricer for the leg
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("CMS");
    QL_REQUIRE(builder, "No builder found for CmsLeg");
    boost::shared_ptr<CmsCouponPricerBuilder> cmsSwapBuilder =
        boost::dynamic_pointer_cast<CmsCouponPricerBuilder>(builder);
    boost::shared_ptr<FloatingRateCouponPricer> couponPricer = cmsSwapBuilder->engine(swapIndex->currency());

    // Loop over the coupons in the leg and set pricer
    Leg tmpLeg = cmsLeg;
    QuantLib::setCouponPricer(tmpLeg, couponPricer);

    // build naked option leg if required
    if (cmsData->nakedOption()) {
        tmpLeg = StrippedCappedFlooredCouponLeg(tmpLeg);
        // fix for missing registration in ql 1.13
        for (auto const& t : tmpLeg) {
            auto s = boost::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(t);
            if (s != nullptr)
                s->registerWith(s->underlying());
        }
    }
    return tmpLeg;
}

Leg makeCMSSpreadLeg(const LegData& data, const boost::shared_ptr<QuantLib::SwapSpreadIndex>& swapSpreadIndex,
                     const boost::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer) {
#if QL_HEX_VERSION < 0x011300f0
    WLOG("The CMS Spread implementation in older QL versions has issues (found "
         << QL_VERSION << "), consider upgrading to at least 1.13")
#endif
    boost::shared_ptr<CMSSpreadLegData> cmsSpreadData =
        boost::dynamic_pointer_cast<CMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(cmsSpreadData, "Wrong LegType, expected CMSSpread, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    vector<double> spreads = ore::data::buildScheduledVectorNormalised(cmsSpreadData->spreads(),
                                                                       cmsSpreadData->spreadDates(), schedule, 0.0);
    vector<double> gearings = ore::data::buildScheduledVectorNormalised(cmsSpreadData->gearings(),
                                                                        cmsSpreadData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    Size fixingDays =
        cmsSpreadData->fixingDays() == Null<Size>() ? swapSpreadIndex->fixingDays() : cmsSpreadData->fixingDays();

    applyAmortization(notionals, data, schedule, false);

    CmsSpreadLeg cmsSpreadLeg = CmsSpreadLeg(schedule, swapSpreadIndex)
                                    .withNotionals(notionals)
                                    .withSpreads(spreads)
                                    .withGearings(gearings)
                                    .withPaymentDayCounter(dc)
                                    .withPaymentAdjustment(bdc)
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
    auto cmsBuilder = boost::dynamic_pointer_cast<CmsCouponPricerBuilder>(builder1);
    auto cmsPricer = boost::dynamic_pointer_cast<CmsCouponPricer>(cmsBuilder->engine(swapSpreadIndex->currency()));
    QL_REQUIRE(cmsPricer, "Expected CMS Pricer");
    auto builder2 = engineFactory->builder("CMSSpread");
    QL_REQUIRE(builder2, "No CMS Spread builder found for CmsSpreadLeg");
    auto cmsSpreadBuilder = boost::dynamic_pointer_cast<CmsSpreadCouponPricerBuilder>(builder2);
    auto cmsSpreadPricer = cmsSpreadBuilder->engine(swapSpreadIndex->currency(), cmsSpreadData->swapIndex1(),
                                                    cmsSpreadData->swapIndex2(), cmsPricer);
    QL_REQUIRE(cmsSpreadPricer, "Expected CMS Spread Pricer");

    // Loop over the coupons in the leg and set pricer
    Leg tmpLeg = cmsSpreadLeg;
    QuantLib::setCouponPricer(tmpLeg, cmsSpreadPricer);

    // build naked option leg if required
    if (cmsSpreadData->nakedOption()) {
        tmpLeg = StrippedCappedFlooredCouponLeg(tmpLeg);
        // fix for missing registration in ql 1.13
        for (auto const& t : tmpLeg) {
            auto s = boost::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(t);
            if (s != nullptr)
                s->registerWith(s->underlying());
        }
    }
    return tmpLeg;
}

Leg makeDigitalCMSSpreadLeg(const LegData& data, const boost::shared_ptr<QuantLib::SwapSpreadIndex>& swapSpreadIndex,
                            const boost::shared_ptr<EngineFactory>& engineFactory) {
#if QL_HEX_VERSION < 0x011300f0
    WLOG("The CMS Spread implementation in older QL versions has issues (found "
         << QL_VERSION << "), consider upgrading to at least 1.13")
#endif
    auto digitalCmsSpreadData = boost::dynamic_pointer_cast<DigitalCMSSpreadLegData>(data.concreteLegData());
    QL_REQUIRE(digitalCmsSpreadData, "Wrong LegType, expected DigitalCMSSpread");

    auto cmsSpreadData = boost::dynamic_pointer_cast<CMSSpreadLegData>(digitalCmsSpreadData->underlying());
    QL_REQUIRE(cmsSpreadData, "Incomplete DigitalCmsSpread Leg, expected CMSSpread data");

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    vector<double> spreads = ore::data::buildScheduledVectorNormalised(cmsSpreadData->spreads(),
                                                                       cmsSpreadData->spreadDates(), schedule, 0.0);
    vector<double> gearings = ore::data::buildScheduledVectorNormalised(cmsSpreadData->gearings(),
                                                                        cmsSpreadData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);

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
                                                  .withReplication(boost::make_shared<DigitalReplication>())
                                                  .withNakedOption(cmsSpreadData->nakedOption());

    if (cmsSpreadData->caps().size() > 0 || cmsSpreadData->floors().size() > 0)
        QL_FAIL("caps/floors not supported in DigitalCMSSpreadOptions");

    // Get a coupon pricer for the leg
    auto builder1 = engineFactory->builder("CMS");
    QL_REQUIRE(builder1, "No CMS builder found for CmsSpreadLeg");
    auto cmsBuilder = boost::dynamic_pointer_cast<CmsCouponPricerBuilder>(builder1);
    auto cmsPricer = boost::dynamic_pointer_cast<CmsCouponPricer>(cmsBuilder->engine(swapSpreadIndex->currency()));
    QL_REQUIRE(cmsPricer, "Expected CMS Pricer");
    auto builder2 = engineFactory->builder("CMSSpread");
    QL_REQUIRE(builder2, "No CMS Spread builder found for CmsSpreadLeg");
    auto cmsSpreadBuilder = boost::dynamic_pointer_cast<CmsSpreadCouponPricerBuilder>(builder2);
    auto cmsSpreadPricer = cmsSpreadBuilder->engine(swapSpreadIndex->currency(), cmsSpreadData->swapIndex1(),
                                                    cmsSpreadData->swapIndex2(), cmsPricer);
    QL_REQUIRE(cmsSpreadPricer, "Expected CMS Spread Pricer");

    // Loop over the coupons in the leg and set pricer
    Leg tmpLeg = digitalCmsSpreadLeg;
    QuantLib::setCouponPricer(tmpLeg, cmsSpreadPricer);

    return tmpLeg;
}

Leg makeEquityLeg(const LegData& data, const boost::shared_ptr<EquityIndex>& equityCurve,
                  const boost::shared_ptr<QuantExt::FxIndex>& fxIndex) {
    boost::shared_ptr<EquityLegData> eqLegData = boost::dynamic_pointer_cast<EquityLegData>(data.concreteLegData());
    QL_REQUIRE(eqLegData, "Wrong LegType, expected Equity, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    bool isTotalReturn = eqLegData->returnType() == "Total";
    Real dividendFactor = eqLegData->dividendFactor();
    Real initialPrice = eqLegData->initialPrice();
    bool initialPriceIsInTargetCcy = false;
    if (!eqLegData->initialPriceCurrency().empty()) {
        QL_REQUIRE(eqLegData->initialPriceCurrency() == data.currency() ||
                       eqLegData->initialPriceCurrency() == eqLegData->eqCurrency() || eqLegData->eqCurrency().empty(),
                   "initial price ccy (" << eqLegData->initialPriceCurrency() << ") must match either leg ccy ("
                                         << data.currency() << ") or equity ccy (if given, got '"
                                         << eqLegData->eqCurrency() << "')");
        initialPriceIsInTargetCcy = eqLegData->initialPriceCurrency() == data.currency();
    }
    bool notionalReset = eqLegData->notionalReset();
    Natural fixingDays = eqLegData->fixingDays();
    ScheduleData valuationData = eqLegData->valuationSchedule();
    Schedule valuationSchedule;
    if (valuationData.hasData())
        valuationSchedule = makeSchedule(valuationData);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);

    applyAmortization(notionals, data, schedule, false);

    Leg leg = EquityLeg(schedule, equityCurve, fxIndex)
                  .withNotionals(notionals)
                  .withQuantity(eqLegData->quantity())
                  .withPaymentDayCounter(dc)
                  .withPaymentAdjustment(bdc)
                  .withTotalReturn(isTotalReturn)
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
        if (cf->date() >= today) {
            boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<QuantLib::Coupon>(cf);
            if (coupon)
                return coupon->nominal();
        }
    }
    return 0;
}

vector<double> buildAmortizationScheduleFixedAmount(const vector<double>& notionals, const Schedule& schedule,
                                                    const AmortizationData& data) {
    LOG("Build fixed amortization notional schedule");
    vector<double> nominals = normaliseToSchedule(notionals, schedule, (Real)Null<Real>());
    Date startDate = parseDate(data.startDate());
    Date endDate = data.endDate() == "" ? Date::maxDate() : parseDate(data.endDate());
    bool underflow = data.underflow();
    Period amortPeriod = parsePeriod(data.frequency());
    double amort = data.value();
    Date lastAmortDate = Date::minDate();
    for (Size i = 0; i < schedule.size() - 1; i++) {
        if (i > 0 && schedule[i] > lastAmortDate + amortPeriod - 4 * Days && schedule[i] >= startDate &&
            schedule[i] < endDate) { // FIXME: tolerance
            nominals[i] = nominals[i - 1] - amort;
            lastAmortDate = schedule[i];
        } else if (i > 0 && schedule[i] >= endDate) {
            nominals[i] = nominals[i - 1];
        }
        if (amort > nominals[i] && underflow == false)
            amort = std::max(nominals[i], 0.0);
    }
    LOG("Fixed amortization notional schedule done");
    return nominals;
}

vector<double> buildAmortizationScheduleRelativeToInitialNotional(const vector<double>& notionals,
                                                                  const Schedule& schedule,
                                                                  const AmortizationData& data) {
    LOG("Build notional schedule with amortizations expressed as percentages of initial notional");
    vector<double> nominals = normaliseToSchedule(notionals, schedule, (Real)Null<Real>());
    Date startDate = parseDate(data.startDate());
    Date endDate = data.endDate() == "" ? Date::maxDate() : parseDate(data.endDate());
    bool underflow = data.underflow();
    Period amortPeriod = parsePeriod(data.frequency());
    double fraction = data.value(); // first difference to "FixedAmount"
    QL_REQUIRE(fraction >= 0.0 && fraction <= 1.0,
               "amortization fraction " << fraction << " out of range"); // second difference to "FixedAmount"
    double amort = fraction * nominals.front();                          // third difference to "FixedAmount"
    Date lastAmortDate = Date::minDate();
    for (Size i = 0; i < schedule.size() - 1; i++) {
        if (i > 0 && schedule[i] > lastAmortDate + amortPeriod - 4 * Days && schedule[i] >= startDate &&
            schedule[i] < endDate) { // FIXME: tolerance
            nominals[i] = nominals[i - 1] - amort;
            lastAmortDate = schedule[i];
        } else if (i > 0 && schedule[i] >= endDate)
            nominals[i] = nominals[i - 1];
        if (amort > nominals[i] && underflow == false) {
            amort = std::max(nominals[i], 0.0);
        }
    }
    LOG("Fixed amortization notional schedule done");
    return nominals;
}

vector<double> buildAmortizationScheduleRelativeToPreviousNotional(const vector<double>& notionals,
                                                                   const Schedule& schedule,
                                                                   const AmortizationData& data) {
    LOG("Build notional schedule with amortizations expressed as percentages of previous notionals");
    vector<double> nominals = normaliseToSchedule(notionals, schedule, (Real)Null<Real>());
    Date startDate = parseDate(data.startDate());
    Date endDate = data.endDate() == "" ? Date::maxDate() : parseDate(data.endDate());
    Period amortPeriod = parsePeriod(data.frequency());
    double fraction = data.value();
    QL_REQUIRE(fraction >= 0.0 && fraction <= 1.0, "amortization fraction " << fraction << " out of range");
    Date lastAmortDate = Date::minDate();
    for (Size i = 0; i < schedule.size() - 1; i++) {
        if (i > 0 && schedule[i] > lastAmortDate + amortPeriod - 4 * Days && schedule[i] >= startDate &&
            schedule[i] < endDate) { // FIXME: tolerance
            nominals[i] = nominals[i - 1] * (1.0 - fraction);
            lastAmortDate = schedule[i];
        } else if (i > 0 && schedule[i] >= endDate)
            nominals[i] = nominals[i - 1];
    }
    LOG("Fixed amortization notional schedule done");
    return nominals;
}

vector<double> buildAmortizationScheduleFixedAnnuity(const vector<double>& notionals, const vector<double>& rates,
                                                     const Schedule& schedule, const AmortizationData& data,
                                                     const DayCounter& dc) {
    LOG("Build fixed annuity notional schedule");
    vector<double> nominals = normaliseToSchedule(notionals, schedule, (Real)Null<Real>());
    Date startDate = parseDate(data.startDate());
    Date endDate = data.endDate() == "" ? Date::maxDate() : parseDate(data.endDate());
    bool underflow = data.underflow();
    double annuity = data.value();
    Real amort = 0.0;
    for (Size i = 0; i < schedule.size() - 1; i++) {
        if (i > 0 && schedule[i] >= startDate && schedule[i] < endDate)
            nominals[i] = nominals[i - 1] - amort;
        else if (i > 0 && schedule[i] >= endDate)
            nominals[i] = nominals[i - 1];
        Real dcf = dc.yearFraction(schedule[i], schedule[i + 1]);
        Real rate = i < rates.size() ? rates[i] : rates.back();
        amort = annuity - rate * nominals[i] * dcf;
        if (amort > nominals[i] && underflow == false) {
            amort = std::max(nominals[i], 0.0);
        }
    }
    LOG("Fixed Annuity notional schedule done");
    return nominals;
}

void applyAmortization(std::vector<Real>& notionals, const LegData& data, const Schedule& schedule,
                       const bool annuityAllowed, const std::vector<Real>& rates) {
    Date lastEndDate = Date::minDate();
    for (auto const& amort : data.amortizationData()) {
        if (!amort.initialized())
            continue;
        Date startDate = parseDate(amort.startDate());
        QL_REQUIRE(startDate >= lastEndDate, "Amortization start date ("
                                                 << startDate << ") is earlier than last end date (" << lastEndDate
                                                 << ")");
        lastEndDate = parseDate(amort.endDate());
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
        } else
            QL_FAIL("AmortizationType " << amort.type() << " not supported");
        // check that for a floating leg we only have one amortization block, if the type is annuity
        // we recognise a floating leg by an empty (fixed) rates vector
        if (rates.empty() && amortizationType == AmortizationType::Annuity) {
            QL_REQUIRE(data.amortizationData().size() == 1,
                       "Floating Leg supports only one amortisation block of type Annuity");
        }
    }
}

void applyIndexing(Leg& leg, const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                   std::map<std::string, std::string>& qlToOREIndexNames, RequiredFixings& requiredFixings) {
    for (auto const& indexing : data.indexing()) {
        if (indexing.hasData()) {
            DLOG("apply indexing (index='" << indexing.index() << "') to leg of type " << data.legType());
            QL_REQUIRE(engineFactory, "applyIndexing: engineFactory required");

            // we allow indexing by equity, commodity and FX indices (technically any QuantLib::Index
            // will work, so the list of index types can be extended here if required)
            boost::shared_ptr<Index> index;
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
                                     engineFactory->configuration(MarketContext::pricing),
                                     indexing.indexFixingCalendar(), indexing.indexFixingDays());
            } else if (boost::starts_with(indexing.index(), "COMM-")) {
                auto tmp = parseCommodityIndex(indexing.index());
                index =
                    parseCommodityIndex(indexing.index(), tmp->fixingCalendar(),
                                        engineFactory->market()->commodityPriceCurve(tmp->underlyingName(), config));
            } else if (boost::starts_with(indexing.index(), "BOND-")) {
                // if we build a bond index, we add the required fixings for the bond underlying
                BondData bondData(parseBondIndex(indexing.index())->securityName(), 1.0);
                index = buildBondIndex(bondData, indexing.indexIsDirty(), indexing.indexIsRelative(),
                                       parseCalendar(indexing.indexFixingCalendar()),
                                       indexing.indexIsConditionalOnSurvival(), engineFactory, requiredFixings);
            } else {
                QL_FAIL("invalid index '" << indexing.index()
                                          << "' in indexing data, expected EQ-, FX-, COMM-, BOND- index");
            }

            QL_REQUIRE(index, "applyIndexing(): index is null, this is unexpected");

            // apply the indexing
            IndexedCouponLeg indLeg(leg, indexing.quantity(), index);
            indLeg.withInitialFixing(indexing.initialFixing());
            indLeg.withFixingDays(indexing.fixingDays());
            indLeg.inArrearsFixing(indexing.inArrearsFixing());
            if (indexing.valuationSchedule().hasData())
                indLeg.withValuationSchedule(makeSchedule(indexing.valuationSchedule()));
            if (!indexing.fixingCalendar().empty())
                indLeg.withFixingCalendar(parseCalendar(indexing.fixingCalendar()));
            if (!indexing.fixingConvention().empty())
                indLeg.withFixingConvention(parseBusinessDayConvention(indexing.fixingConvention()));
            leg = indLeg;

            // add to the qlToOREIndexNames map
            qlToOREIndexNames[index->name()] = indexing.index();
        }
    }
}

boost::shared_ptr<QuantExt::FxIndex> buildFxIndex(const string& fxIndex, const string& domestic, const string& foreign,
                                                  const boost::shared_ptr<Market>& market, const string& configuration,
                                                  const string& calendar, Size fixingDays, bool useXbsCurves) {
    // 1. Parse the index we have with no term structures
    boost::shared_ptr<QuantExt::FxIndex> fxIndexBase = parseFxIndex(fxIndex);

    // get market data objects - we set up the index using source/target, fixing days
    // and calendar from legData_[i].fxIndex()
    string source = fxIndexBase->sourceCurrency().code();
    string target = fxIndexBase->targetCurrency().code();

    // If useXbsCurves is true, try to link the FX index to xccy based curves. There will be none if source or target
    // is equal to the base currency of the run so we must use the try/catch.
    Handle<YieldTermStructure> sorTS;
    Handle<YieldTermStructure> tarTS;
    if (useXbsCurves) {
        sorTS = xccyYieldCurve(market, source, configuration);
        tarTS = xccyYieldCurve(market, target, configuration);
    } else {
        sorTS = market->discountCurve(source, configuration);
        tarTS = market->discountCurve(target, configuration);
    }

    Handle<Quote> spot = market->fxSpot(source + target);
    Calendar cal = parseCalendar(calendar);

    // Now check the ccy and foreignCcy from the legdata, work out if we need to invert or not
    bool invertFxIndex = false;
    if (domestic == target && foreign == source) {
        invertFxIndex = false;
    } else if (domestic == source && foreign == target) {
        invertFxIndex = true;
    } else {
        QL_FAIL("Cannot combine FX Index " << fxIndex << " with reset ccy " << domestic << " and reset foreignCurrency "
                                           << foreign);
    }

    auto fxi = boost::make_shared<FxIndex>(fxIndexBase->familyName(), fixingDays, fxIndexBase->sourceCurrency(),
                                           fxIndexBase->targetCurrency(), cal, spot, sorTS, tarTS, invertFxIndex);

    QL_REQUIRE(fxi, "Failed to build FXIndex " << fxIndex);

    return fxi;
}

boost::shared_ptr<QuantExt::BondIndex> buildBondIndex(const BondData& securityData, const bool dirty,
                                                      const bool relative, const Calendar& fixingCalendar,
                                                      const bool conditionalOnSurvival,
                                                      const boost::shared_ptr<EngineFactory>& engineFactory,
                                                      RequiredFixings& requiredFixings) {

    // build the bond, we would not need a full build with a pricing engine attached, but this is the easiest

    BondData data = securityData;
    data.populateFromBondReferenceData(engineFactory->referenceData());
    Bond bond(Envelope(), data);
    bond.build(engineFactory);

    RequiredFixings bondRequiredFixings = bond.requiredFixings();
    if (dirty) {
        // dirty prices require accrueds on past dates
        bondRequiredFixings.unsetPayDates();
    }
    requiredFixings.addData(bondRequiredFixings);

    auto qlBond = boost::dynamic_pointer_cast<QuantLib::Bond>(bond.instrument()->qlInstrument());
    QL_REQUIRE(qlBond, "buildBondIndex(): could not cast to QuantLib::Bond, this is unexpected");

    // get the curves

    string securityId = data.securityId();

    Handle<YieldTermStructure> discountCurve = engineFactory->market()->yieldCurve(
        data.referenceCurveId(), engineFactory->configuration(MarketContext::pricing));

    Handle<DefaultProbabilityTermStructure> defaultCurve;
    if (!data.creditCurveId().empty())
        defaultCurve = engineFactory->market()->defaultCurve(data.creditCurveId(),
                                                             engineFactory->configuration(MarketContext::pricing));

    Handle<YieldTermStructure> incomeCurve;
    if (!data.incomeCurveId().empty())
        incomeCurve = engineFactory->market()->yieldCurve(data.incomeCurveId(),
                                                          engineFactory->configuration(MarketContext::pricing));

    Handle<Quote> recovery;
    try {
        recovery =
            engineFactory->market()->recoveryRate(securityId, engineFactory->configuration(MarketContext::pricing));
    } catch (...) {
        ALOG("security specific recovery rate not found for security ID "
             << securityId << ", falling back on the recovery rate for credit curve Id " << data.creditCurveId());
        if (!data.creditCurveId().empty())
            recovery = engineFactory->market()->recoveryRate(data.creditCurveId(),
                                                             engineFactory->configuration(MarketContext::pricing));
    }

    Handle<Quote> spread;
    try {
        spread =
            engineFactory->market()->securitySpread(securityId, engineFactory->configuration(MarketContext::pricing));
    } catch (...) {
    }

    // build and return the index

    return boost::make_shared<QuantExt::BondIndex>(securityId, dirty, relative, fixingCalendar, qlBond, discountCurve,
                                                   defaultCurve, recovery, spread, incomeCurve, conditionalOnSurvival);
}

Leg joinLegs(const std::vector<Leg>& legs) {
    Leg masterLeg;
    for (Size i = 0; i < legs.size(); ++i) {
        // check if the periods of adjacent legs are consistent
        if (i > 0) {
            auto lcpn = boost::dynamic_pointer_cast<Coupon>(legs[i - 1].back());
            auto fcpn = boost::dynamic_pointer_cast<Coupon>(legs[i].front());
            QL_REQUIRE(lcpn, "joinLegs: expected coupon as last cashflow in leg #" << (i - 1));
            QL_REQUIRE(fcpn, "joinLegs: expected coupon as first cashflow in leg #" << i);
            QL_REQUIRE(lcpn->accrualEndDate() == fcpn->accrualStartDate(),
                       "joinLegs: accrual end date of last coupon in leg #"
                           << (i - 1) << " (" << lcpn->accrualEndDate()
                           << ") is not equal to accrual start date of first coupon in leg #" << i << " ("
                           << fcpn->accrualStartDate() << ")");
        }
        // copy legs together
        masterLeg.insert(masterLeg.end(), legs[i].begin(), legs[i].end());
    }
    return masterLeg;
}

} // namespace data
} // namespace ore
