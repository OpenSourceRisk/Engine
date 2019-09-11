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

#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ored/portfolio/builders/capflooredyoyleg.hpp>
#include <ored/portfolio/builders/cms.hpp>
#include <ored/portfolio/builders/cmsspread.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
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
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/errors.hpp>
#include <ql/experimental/coupons/cmsspreadcoupon.hpp>
#include <ql/experimental/coupons/digitalcmsspreadcoupon.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/version.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/brlcdicouponpricer.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/floatingannuitycoupon.hpp>
#include <qle/cashflows/strippedcapflooredyoyinflationcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/cashflows/subperiodscouponpricer.hpp>
#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/cashflows/couponpricer.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

LegDataRegister<CashflowData> CashflowData::reg_("Cashflow");

void CashflowData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    amounts_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Cashflow", "Amount", "Date", dates_);
}

XMLNode* CashflowData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Cashflow", "Amount", amounts_, "Date", dates_);
    return node;
}

LegDataRegister<FixedLegData> FixedLegData::reg_("Fixed");

void FixedLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    rates_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Rates", "Rate", "startDate", rateDates_, true);
}

XMLNode* FixedLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    return node;
}

LegDataRegister<ZeroCouponFixedLegData> ZeroCouponFixedLegData::reg_("ZeroCouponFixed");

void ZeroCouponFixedLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    rates_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Rates", "Rate", "startDate", rateDates_, true);
    XMLNode* compNode = XMLUtils::getChildNode(node, "Compounding");
    if (compNode)
        compounding_ = XMLUtils::getChildValue(node, "Compounding", true);
    else 
        compounding_ = "Compounded";
    QL_REQUIRE(compounding_ == "Compounded" || compounding_ == "Simple", "Compounding method " << compounding_ << " not supported");
}

XMLNode* ZeroCouponFixedLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    XMLUtils::addChild(doc, node, "Compounding", compounding_);
    return node;
}

LegDataRegister<FloatingLegData> FloatingLegData::reg_("Floating");

void FloatingLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = internalIndexName(XMLUtils::getChildValue(node, "Index", true));
    indices_.insert(index_);
    // These are all optional
    spreads_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Spreads", "Spread", "startDate", spreadDates_);
    isInArrears_ = isAveraged_ = hasSubPeriods_ = includeSpread_ = false;
    if(XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears"))
        isInArrears_ = parseBool(XMLUtils::getNodeValue(arrNode));
    if(XMLNode* avgNode = XMLUtils::getChildNode(node, "IsAveraged"))
        isAveraged_ = parseBool(XMLUtils::getNodeValue(avgNode));
    if(XMLNode* spNode = XMLUtils::getChildNode(node, "HasSubPeriods"))
        hasSubPeriods_ = parseBool(XMLUtils::getNodeValue(spNode));
    if(XMLNode* incSpNode = XMLUtils::getChildNode(node, "IncludeSpread"))
        includeSpread_ = parseBool(XMLUtils::getNodeValue(incSpNode));
    if(auto n = XMLUtils::getChildNode(node, "FixingDays"))
        fixingDays_ = parseInteger(XMLUtils::getNodeValue(n));
    else
        fixingDays_ = Null<Size>();
    caps_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Caps", "Cap", "startDate", capDates_);
    floors_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Floors", "Floor", "startDate", floorDates_);
    gearings_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Gearings", "Gearing", "startDate", gearingDates_);
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
    if(fixingDays_ != Null<Size>())
        XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
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
    indices_.insert(index_);
    baseCPI_ = XMLUtils::getChildValueAsDouble(node, "BaseCPI", true);
    observationLag_ = XMLUtils::getChildValue(node, "ObservationLag", true);
    interpolated_ = XMLUtils::getChildValueAsBool(node, "Interpolated", true);
    XMLNode* subNomNode = XMLUtils::getChildNode(node, "SubtractInflationNotional");
    if (subNomNode)
        subtractInflationNominal_ = XMLUtils::getChildValueAsBool(node, "SubtractInflationNotional", true);
    else
        subtractInflationNominal_ = false;
    rates_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Rates", "Rate", "startDate", rateDates_, true);
}

XMLNode* CPILegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    XMLUtils::addChild(doc, node, "BaseCPI", baseCPI_);
    XMLUtils::addChild(doc, node, "ObservationLag", observationLag_);
    XMLUtils::addChild(doc, node, "Interpolated", interpolated_);
    XMLUtils::addChild(doc, node, "SubtractInflationNotional", subtractInflationNominal_);
    return node;
}

LegDataRegister<YoYLegData> YoYLegData::reg_("YY");

void YoYLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = XMLUtils::getChildValue(node, "Index", true);
    indices_.insert(index_);
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays", true);
    observationLag_ = XMLUtils::getChildValue(node, "ObservationLag", true);
    gearings_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Gearings", "Gearing", "startDate", gearingDates_);
    spreads_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Spreads", "Spread", "startDate", spreadDates_);
    caps_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Caps", "Cap", "startDate", capDates_);
    floors_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Floors", "Floor", "startDate", floorDates_);
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
    if(fixingDays_ != Null<Size>())
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
    spreads_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Spreads", "Spread", "startDate", spreadDates_);
    XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears");
    if (arrNode)
        isInArrears_ = XMLUtils::getChildValueAsBool(node, "IsInArrears", true);
    else
        isInArrears_ = false;                                       // default to fixing-in-advance
    if(auto n = XMLUtils::getChildNode(node, "FixingDays"))
        fixingDays_ = parseInteger(XMLUtils::getNodeValue(n));
    else
        fixingDays_ = Null<Size>();
    caps_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Caps", "Cap", "startDate", capDates_);
    floors_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Floors", "Floor", "startDate", floorDates_);
    gearings_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Gearings", "Gearing", "startDate", gearingDates_);
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
    if(fixingDays_ != Null<Size>())
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
    spreads_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Spreads", "Spread", "startDate", spreadDates_);
    XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears");
    if (arrNode)
        isInArrears_ = XMLUtils::getChildValueAsBool(node, "IsInArrears", true);
    else
        isInArrears_ = false;                                       // default to fixing-in-advance
    if(auto n = XMLUtils::getChildNode(node, "FixingDays"))
        fixingDays_ = parseInteger(XMLUtils::getNodeValue(n));
    else
        fixingDays_ = Null<Size>();
    caps_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Caps", "Cap", "startDate", capDates_);
    floors_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Floors", "Floor", "startDate", floorDates_);
    gearings_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Gearings", "Gearing", "startDate", gearingDates_);
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

    callStrikes_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "CallStrikes", "Strike", "startDate",
                                                                      callStrikeDates_, false);
    if (callStrikes_.size() > 0) {
        string cp = XMLUtils::getChildValue(node, "CallPosition", true);
        callPosition_ = parsePositionType(cp);
        isCallATMIncluded_ = XMLUtils::getChildValueAsBool(node, "IsCallATMIncluded", true);
        callPayoffs_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "CallPayoffs", "Payoff", "startDate",
                                                                      callPayoffDates_, false);
    }
    
    putStrikes_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "PutStrikes", "Strike", "startDate",
                                                                     putStrikeDates_, false);
    if (putStrikes_.size() > 0) {
        string pp = XMLUtils::getChildValue(node, "PutPosition", true);
        putPosition_ = parsePositionType(pp);
        isPutATMIncluded_ = XMLUtils::getChildValueAsBool(node, "IsPutATMIncluded", true);
        putPayoffs_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "PutPayoffs", "Payoff", "startDate",
                                                                     putPayoffDates_, false);
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
    eqName_ = XMLUtils::getChildValue(node, "Name");
    indices_.insert("EQ-" + eqName_);
    if (XMLUtils::getChildNode(node, "InitialPrice"))
        initialPrice_ = XMLUtils::getChildValueAsDouble(node, "InitialPrice");
    else
        initialPrice_ = Real();
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays");    
    XMLNode* tmp = XMLUtils::getChildNode(node, "ValuationSchedule");
    if (tmp)
        valuationSchedule_.fromXML(tmp);    
    if (XMLUtils::getChildNode(node, "NotionalReset"))
        notionalReset_ = XMLUtils::getChildValueAsBool(node, "NotionalReset");
    else
        notionalReset_ = false;
}

XMLNode* EquityLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "ReturnType", returnType_);
    if (returnType_ == "Total") {
        XMLUtils::addChild(doc, node, "DividendFactor", dividendFactor_);
    }
    XMLUtils::addChild(doc, node, "Name", eqName_);
    if (initialPrice_)
        XMLUtils::addChild(doc, node, "InitialPrice", initialPrice_);
    XMLUtils::addChild(doc, node, "NotionalReset", notionalReset_);

    if (valuationSchedule_.hasData()) {
        XMLNode* schedNode = valuationSchedule_.toXML(doc);
        XMLUtils::setNodeName(doc, schedNode, "ValuationSchedule"); 
        XMLUtils::appendNode(node, schedNode);
    } else {
        XMLUtils::addChild(doc, node, "FixingDays", static_cast<Integer>(fixingDays_));
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
                 const string& paymentCalendar)
    : concreteLegData_(concreteLegData), isPayer_(isPayer), currency_(currency), schedule_(scheduleData),
      dayCounter_(dayCounter), notionals_(notionals), notionalDates_(notionalDates),
      paymentConvention_(paymentConvention), notionalInitialExchange_(notionalInitialExchange),
      notionalFinalExchange_(notionalFinalExchange), notionalAmortizingExchange_(notionalAmortizingExchange),
      isNotResetXCCY_(isNotResetXCCY), foreignCurrency_(foreignCurrency), foreignAmount_(foreignAmount),
      fxIndex_(fxIndex), fixingDays_(fixingDays), fixingCalendar_(fixingCalendar), amortizationData_(amortizationData),
      paymentLag_(paymentLag), paymentCalendar_(paymentCalendar) {
    
    indices_ = concreteLegData_->indices();
    if (!fxIndex_.empty())
        indices_.insert(fxIndex_);
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
    notionals_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Notionals", "Notional", "startDate", notionalDates_);
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

    if (!amortizationData_.empty()) {
        XMLNode* amortisationsParentNode = doc.allocNode("Amortizations");
        for (auto& amort : amortizationData_) {
            if (amort.initialized()) {
                XMLUtils::appendNode(amortisationsParentNode, amort.toXML(doc));
            }
        }
        XMLUtils::appendNode(node, amortisationsParentNode);
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
    QL_REQUIRE(comp == QuantLib::Compounded || comp == QuantLib::Simple, "Compounding method " << zcFixedLegData->compounding() << " not supported");

    // we loop over the dates in the schedule, computing the compound factor.
    // For the Compounded rule:
    // (1+r)^dcf_0 *  (1+r)^dcf_1 * ... = (1+r)^(dcf_0 + dcf_1 + ...)
    // So we compute the sum of all DayCountFractions in the loop.
    // For the Simple rule:
    // (1 + r * dcf_0) * (1 + r * dcf_1)...
    double totalDCF = 0;
    double compoundFactor = 1;
    for (Size i = 0; i < dates.size() - 1; i++) {
        double dcf = dc.yearFraction(dates[i], dates[i+1]);
        if (comp == QuantLib::Simple)
            compoundFactor *= (1 + fixedRate * dcf);
        else
            totalDCF += dcf;
    }
    if (comp == QuantLib::Compounded)
        compoundFactor = pow(1.0 + fixedRate, totalDCF);
    fixedAmount *= (compoundFactor - 1);
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
    vector<double> spreads = buildScheduledVectorNormalised(floatData->spreads(), floatData->spreadDates(), schedule, 0.0);
    vector<double> gearings = buildScheduledVectorNormalised(floatData->gearings(), floatData->gearingDates(), schedule, 1.0);
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
                Date paymentDate = schedule.calendar().adjust(schedule[i+1], bdc);
                if (schedule[i] < startDate || i == 0) {
                    boost::shared_ptr<FloatingRateCoupon> coupon;
                    if(!floatData->hasSubPeriods()) {
                        coupon = boost::make_shared<IborCoupon>(paymentDate, notionals[i], schedule[i], schedule[i + 1],
                                                       fixingDays, index, gearings[i], spreads[i], Date(),
                                                       Date(), dc, floatData->isInArrears());
                        coupon->setPricer(boost::make_shared<BlackIborCouponPricer>());
                    }
                    else {
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
                            annuity, underflow, coupons.back(), paymentDate, schedule[i], schedule[i + 1],
                            fixingDays, index, gearings[i], spreads[i], Date(), Date(), dc,
                            floatData->isInArrears());
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

    if(floatData->hasSubPeriods()) {
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
} // namespace data

Leg makeOISLeg(const LegData& data, const boost::shared_ptr<OvernightIndex>& index) {
    boost::shared_ptr<FloatingLegData> floatData = boost::dynamic_pointer_cast<FloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << data.legType());

    if (floatData->caps().size() > 0 || floatData->floors().size() > 0)
        QL_FAIL("Caps and floors are not supported for OIS legs");

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Natural paymentLag = data.paymentLag();
    Calendar paymentCalendar = index->fixingCalendar();
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    vector<double> spreads = buildScheduledVectorNormalised(floatData->spreads(), floatData->spreadDates(), schedule, 0.0);
    vector<double> gearings = buildScheduledVectorNormalised(floatData->gearings(), floatData->gearingDates(), schedule, 1.0);

    applyAmortization(notionals, data, schedule, false);

    if (floatData->isAveraged()) {

        boost::shared_ptr<QuantExt::AverageONIndexedCouponPricer> couponPricer =
            boost::make_shared<QuantExt::AverageONIndexedCouponPricer>();
        QuantExt::AverageONLeg leg = QuantExt::AverageONLeg(schedule, index)
                                         .withNotionals(notionals)
                                         .withSpreads(spreads)
                                         .withGearings(gearings)
                                         .withPaymentDayCounter(dc)
                                         .withPaymentAdjustment(bdc)
                                         .withRateCutoff(2)
                                         .withAverageONIndexedCouponPricer(couponPricer);

        return leg;

    } else {

        Leg leg = OvernightLeg(schedule, index)
                               .withNotionals(notionals)
                               .withSpreads(spreads)
                               .withPaymentDayCounter(dc)
                               .withPaymentAdjustment(bdc)
                               .withPaymentCalendar(paymentCalendar)
                               .withPaymentLag(paymentLag)
                               .withGearings(gearings);

        // If the overnight index is BRL CDI, we need a special coupon pricer
        boost::shared_ptr<BRLCdi> brlCdiIndex = boost::dynamic_pointer_cast<BRLCdi>(index);
        if (brlCdiIndex)
            QuantExt::setCouponPricer(leg, boost::make_shared<BRLCdiCouponPricer>());

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

Leg makeNotionalLeg(const Leg& refLeg, const bool initNomFlow, const bool finalNomFlow, const bool amortNomFlow) {

    // Assumption - Cashflows on Input Leg are all coupons
    // This is the Leg to be populated
    Leg leg;

    // Initial Flow Amount
    if (initNomFlow) {
        double initFlowAmt = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[0])->nominal();
        Date initDate = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[0])->accrualStartDate();
        if (initFlowAmt != 0)
            leg.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(-initFlowAmt, initDate)));
    }

    // Amortization Flows
    if (amortNomFlow) {
        for (Size i = 1; i < refLeg.size(); i++) {
            Date flowDate = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[i])->accrualStartDate();
            Real initNom = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[i - 1])->nominal();
            Real newNom = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg[i])->nominal();
            Real flow = initNom - newNom;
            if (flow != 0)
                leg.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(flow, flowDate)));
        }
    }

    // Final Nominal Return at Maturity
    if (finalNomFlow) {
        double finalNomFlow = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg.back())->nominal();
        Date finalDate = boost::dynamic_pointer_cast<QuantLib::Coupon>(refLeg.back())->date();
        if (finalNomFlow != 0)
            leg.push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(finalNomFlow, finalDate)));
    }

    return leg;
}

Leg makeCPILeg(const LegData& data, const boost::shared_ptr<ZeroInflationIndex>& index) {
    boost::shared_ptr<CPILegData> cpiLegData = boost::dynamic_pointer_cast<CPILegData>(data.concreteLegData());
    QL_REQUIRE(cpiLegData, "Wrong LegType, expected CPI, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Period observationLag = parsePeriod(cpiLegData->observationLag());
    CPI::InterpolationType interpolationMethod = CPI::Flat;
    if (cpiLegData->interpolated())
        interpolationMethod = CPI::Linear;
    vector<double> rates = buildScheduledVector(cpiLegData->rates(), cpiLegData->rateDates(), schedule);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);

    applyAmortization(notionals, data, schedule, false);

    Leg leg = CPILeg(schedule, index, cpiLegData->baseCPI(), observationLag)
                  .withNotionals(notionals)
                  .withPaymentDayCounter(dc)
                  .withPaymentAdjustment(bdc)
                  .withPaymentCalendar(schedule.calendar())
                  .withFixedRates(rates)
                  .withObservationInterpolation(interpolationMethod)
                  .withSubtractInflationNominal(cpiLegData->subtractInflationNominal());
    Size n = leg.size();
    QL_REQUIRE(n > 0, "Empty CPI Leg");

    // QuantLib CPILeg automatically adds a Notional Cashflow at maturity date on a CPI swap
    // If Notional Exchange set to false, remove the final cashflow.

    if (!data.notionalFinalExchange()) {
        QL_REQUIRE(n > 1, "Cannot have Notional Final Exchange with just a single cashflow");
        boost::shared_ptr<CPICashFlow> cpicf = boost::dynamic_pointer_cast<CPICashFlow>(leg[n - 1]);
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(leg[n - 2]);
        if (cpicf && (cpicf->date() == coupon->date()))
            leg.pop_back();
    }

    return leg;
}

Leg makeYoYLeg(const LegData& data, const boost::shared_ptr<YoYInflationIndex>& index, const boost::shared_ptr<EngineFactory>& engineFactory) {
    boost::shared_ptr<YoYLegData> yoyLegData = boost::dynamic_pointer_cast<YoYLegData>(data.concreteLegData());
    QL_REQUIRE(yoyLegData, "Wrong LegType, expected YoY, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Period observationLag = parsePeriod(yoyLegData->observationLag());
    vector<double> gearings = buildScheduledVectorNormalised(yoyLegData->gearings(), yoyLegData->gearingDates(), schedule, 1.0);
    vector<double> spreads = buildScheduledVectorNormalised(yoyLegData->spreads(), yoyLegData->spreadDates(), schedule, 0.0);
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
        QL_REQUIRE(builder, "No builder found for CapFlooredIborLeg");
        boost::shared_ptr<CapFlooredYoYLegEngineBuilder> cappedFlooredYoYBuilder =
            boost::dynamic_pointer_cast<CapFlooredYoYLegEngineBuilder>(builder);
        string indexname = index->name();
        boost::shared_ptr<InflationCouponPricer> couponPricer =
            cappedFlooredYoYBuilder->engine(indexname.replace(indexname.find(" ", 0), 1, ""));

        // set coupon pricer for the leg
        Leg leg = yoyLeg.operator Leg();
        for (Size i = 0; i < leg.size(); i++) {
            boost::dynamic_pointer_cast<CappedFlooredYoYInflationCoupon>(leg[i])
                ->setPricer(boost::dynamic_pointer_cast<QuantLib::YoYInflationCouponPricer>(couponPricer));
        }

        // build naked option leg if required
        if (yoyLegData->nakedOption()) {
            leg = StrippedCappedFlooredYoYInflationCouponLeg(leg);
            for (auto const& t : leg) {
                auto s = boost::dynamic_pointer_cast<StrippedCappedFlooredYoYInflationCoupon>(t);
                if (s != nullptr)
                    s->registerWith(s->underlying());
            }
        }

        return leg;
    }
    else {
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

    vector<double> spreads = ore::data::buildScheduledVectorNormalised(cmsData->spreads(), cmsData->spreadDates(), schedule, 0.0);
    vector<double> gearings = ore::data::buildScheduledVectorNormalised(cmsData->gearings(), cmsData->gearingDates(), schedule, 1.0);
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
    vector<double> spreads =
        ore::data::buildScheduledVectorNormalised(cmsSpreadData->spreads(), cmsSpreadData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        ore::data::buildScheduledVectorNormalised(cmsSpreadData->gearings(), cmsSpreadData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    Size fixingDays = cmsSpreadData->fixingDays() == Null<Size>() ? swapSpreadIndex->fixingDays() : cmsSpreadData->fixingDays();

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
    vector<double> spreads =
        ore::data::buildScheduledVectorNormalised(cmsSpreadData->spreads(), cmsSpreadData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        ore::data::buildScheduledVectorNormalised(cmsSpreadData->gearings(), cmsSpreadData->gearingDates(), schedule, 1.0);
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

    Size fixingDays = cmsSpreadData->fixingDays() == Null<Size>() ? swapSpreadIndex->fixingDays() : cmsSpreadData->fixingDays();

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
                                                  .withReplication(boost::make_shared<DigitalReplication>());

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

Leg makeEquityLeg(const LegData& data, const boost::shared_ptr<EquityIndex>& equityCurve) {
    boost::shared_ptr<EquityLegData> eqLegData = boost::dynamic_pointer_cast<EquityLegData>(data.concreteLegData());
    QL_REQUIRE(eqLegData, "Wrong LegType, expected Equity, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    bool isTotalReturn = eqLegData->returnType() == "Total";
    Real dividendFactor = eqLegData->dividendFactor();
    Real initialPrice = eqLegData->initialPrice();
    bool notionalReset = eqLegData->notionalReset();
    Natural fixingDays = eqLegData->fixingDays();
    ScheduleData valuationData = eqLegData->valuationSchedule();
    Schedule valuationSchedule;
    if (valuationData.hasData())
        valuationSchedule = makeSchedule(valuationData);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);

    applyAmortization(notionals, data, schedule, false);

    Leg leg = EquityLeg(schedule, equityCurve)
                  .withNotionals(notionals)
                  .withPaymentDayCounter(dc)
                  .withPaymentAdjustment(bdc)
                  .withTotalReturn(isTotalReturn)
                  .withDividendFactor(dividendFactor)
                  .withInitialPrice(initialPrice)
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

// e.g. node is Notionals, get all the children Notional
vector<double> buildScheduledVector(const vector<double>& values, const vector<string>& dates,
                                    const Schedule& schedule) {
    if (values.size() < 2 || dates.size() == 0)
        return values;

    QL_REQUIRE(values.size() == dates.size(), "Value / Date size mismatch in buildScheduledVector."
                                                  << "Value:" << values.size() << ", Dates:" << dates.size());

    // Need to use schedule logic
    // Length of data will be 1 less than schedule
    //
    // Notional 100
    // Notional {startDate 2015-01-01} 200
    // Notional {startDate 2016-01-01} 300
    //
    // Given schedule June, Dec from 2014 to 2016 (6 dates, 5 coupons)
    // we return 100, 100, 200, 200, 300

    // The first node must not have a date.
    // If the second one has a date, all the rest must have, and we process
    // If the second one does not have a date, none of them must have one
    // and we return the vector uneffected.
    QL_REQUIRE(dates[0] == "", "Invalid date " << dates[0] << " for first node");
    if (dates[1] == "") {
        // They must all be empty and then we return values
        for (Size i = 2; i < dates.size(); i++) {
            QL_REQUIRE(dates[i] == "", "Invalid date " << dates[i] << " for node " << i
                                                       << ". Cannot mix dates and non-dates attributes");
        }
        return values;
    }

    // We have nodes with date attributes now
    Size len = schedule.size() - 1;
    vector<double> data(len);
    Size j = 0, max_j = dates.size() - 1; // j is the index of date/value vector. 0 to max_j
    Date d = parseDate(dates[j + 1]);     // The first start date
    for (Size i = 0; i < len; i++) {      // loop over data vector and populate it.
        // If j == max_j we just fall through and take the final value
        while (schedule[i] >= d && j < max_j) {
            j++;
            if (j < max_j) {
                QL_REQUIRE(dates[j + 1] != "", "Cannot have empty date attribute for node " << j + 1);
                d = parseDate(dates[j + 1]);
            }
        }
        data[i] = values[j];
    }

    return data;
}

vector<double> normaliseToSchedule(const vector<double>& values, const Schedule& schedule, const Real defaultValue) {
    vector<double> res = values;
    if (res.size() < schedule.size() - 1)
        res.resize(schedule.size() - 1, res.size() == 0 ? defaultValue : res.back());
    return res;
}

vector<double> buildScheduledVectorNormalised(const vector<double>& values, const vector<string>& dates,
                                              const Schedule& schedule, const Real defaultValue) {
    return normaliseToSchedule(buildScheduledVector(values, dates, schedule), schedule, defaultValue);
}

vector<double> buildAmortizationScheduleFixedAmount(const vector<double>& notionals, const Schedule& schedule,
                                                    const AmortizationData& data) {
    LOG("Build fixed amortization notional schedule");
    vector<double> nominals = normaliseToSchedule(notionals, schedule);
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
    vector<double> nominals = normaliseToSchedule(notionals, schedule);
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
    vector<double> nominals = normaliseToSchedule(notionals, schedule);
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
    vector<double> nominals = normaliseToSchedule(notionals, schedule);
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

} // namespace data
} // namespace ore
