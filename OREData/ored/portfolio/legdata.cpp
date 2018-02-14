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
#include <ored/portfolio/builders/cms.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>

#include <boost/make_shared.hpp>
#include <ql/cashflow.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/cpicouponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/errors.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/floatingannuitycoupon.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

namespace {
void addChildrenWithOptionalAttributes(XMLDocument& doc, XMLNode* n, const string& names, const string& name,
                                       const vector<Real>& values, const string& attrName,
                                       const vector<string>& attrs) {
    if (attrs.empty())
        XMLUtils::addChildren(doc, n, names, name, values);
    else
        XMLUtils::addChildrenWithAttributes(doc, n, names, name, values, attrName, attrs);
}
} // namespace

void CashflowData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    amounts_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Cashflow", "Amount", "Date", dates_);
}

XMLNode* CashflowData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    addChildrenWithOptionalAttributes(doc, node, "Cashflow", "Amount", amounts_, "Date", dates_);
    return node;
}
void FixedLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    rates_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Rates", "Rate", "startDate", rateDates_, true);
}

XMLNode* FixedLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    return node;
}

void FloatingLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = XMLUtils::getChildValue(node, "Index", true);
    spreads_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Spreads", "Spread", "startDate", spreadDates_, true);
    // These are all optional
    XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears");
    if (arrNode)
        isInArrears_ = XMLUtils::getChildValueAsBool(node, "IsInArrears", true);
    else
        isInArrears_ = false; // default to fixing-in-advance
    XMLNode* avgNode = XMLUtils::getChildNode(node, "IsAveraged");
    if (avgNode)
        isAveraged_ = XMLUtils::getChildValueAsBool(node, "IsAveraged", true);
    else
        isAveraged_ = false;
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays"); // defaults to 0
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
    XMLUtils::addChildren(doc, node, "Spreads", "Spread", spreads_);
    XMLUtils::addChild(doc, node, "IsInArrears", isInArrears_);
    XMLUtils::addChild(doc, node, "IsAveraged", isAveraged_);
    XMLUtils::addChild(doc, node, "FixingDays", fixingDays_);
    addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate", gearingDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    return node;
}

void CPILegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = XMLUtils::getChildValue(node, "Index", true);
    baseCPI_ = XMLUtils::getChildValueAsDouble(node, "BaseCPI", true);
    observationLag_ = XMLUtils::getChildValue(node, "ObservationLag", true);
    interpolated_ = XMLUtils::getChildValueAsBool(node, "Interpolated", true);
    XMLNode* subNomNode = XMLUtils::getChildNode(node, "SubtractInflationNotionnal");
    if (subNomNode)
        subtractInflationNominal_ = XMLUtils::getChildValueAsBool(node, "SubtractInflationNotional", true);
    else
        subtractInflationNominal_ = false;
    rates_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Rates", "Rate", "startDate", rateDates_, true);
}

XMLNode* CPILegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", index_);
    addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    XMLUtils::addChild(doc, node, "BaseCPI", baseCPI_);
    XMLUtils::addChild(doc, node, "ObservationLag", observationLag_);
    XMLUtils::addChild(doc, node, "Interpolated", interpolated_);
    XMLUtils::addChild(doc, node, "SubtractInflationNotional", subtractInflationNominal_);
    return node;
}

void YoYLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    index_ = XMLUtils::getChildValue(node, "Index", true);
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays", true);
    observationLag_ = XMLUtils::getChildValue(node, "ObservationLag", true);
    interpolated_ = XMLUtils::getChildValueAsBool(node, "Interpolated", true);
    gearings_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Gearings", "Gearing", "startDate", gearingDates_);
    spreads_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Spreads", "Spread", "startDate", spreadDates_);
}

XMLNode* YoYLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", index_);
    XMLUtils::addChild(doc, node, "ObservationLag", observationLag_);
    XMLUtils::addChild(doc, node, "FixingDays", static_cast<int>(fixingDays_));
    addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate", gearingDates_);
    addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate", spreadDates_);
    return node;
}

XMLNode* CMSLegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", swapIndex_);
    XMLUtils::addChildren(doc, node, "Spreads", "Spread", spreads_);
    XMLUtils::addChild(doc, node, "IsInArrears", isInArrears_);
    XMLUtils::addChild(doc, node, "FixingDays", fixingDays_);
    addChildrenWithOptionalAttributes(doc, node, "Caps", "Cap", caps_, "startDate", capDates_);
    addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate", gearingDates_);
    XMLUtils::addChild(doc, node, "NakedOption", nakedOption_);
    return node;
}

void CMSLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    swapIndex_ = XMLUtils::getChildValue(node, "Index", true);
    spreads_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Spreads", "Spread", "startDate", spreadDates_, true);
    // These are all optional
    XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears");
    if (arrNode)
        isInArrears_ = XMLUtils::getChildValueAsBool(node, "IsInArrears", true);
    else
        isInArrears_ = false;                                       // default to fixing-in-advance
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays"); // defaults to 0
    caps_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Caps", "Cap", "startDate", capDates_);
    floors_ = XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Floors", "Floor", "startDate", floorDates_);
    gearings_ =
        XMLUtils::getChildrenValuesAsDoublesWithAttributes(node, "Gearings", "Gearing", "startDate", gearingDates_);
    if (XMLUtils::getChildNode(node, "NakedOption"))
        nakedOption_ = XMLUtils::getChildValueAsBool(node, "NakedOption", false);
    else
        nakedOption_ = false;
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
    if(endDate_ != "")
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
                 const double foreignAmount, const string& fxIndex, int fixingDays,
                 const std::vector<AmortizationData>& amortizationData)
    : concreteLegData_(concreteLegData), isPayer_(isPayer), currency_(currency), schedule_(scheduleData),
      dayCounter_(dayCounter), notionals_(notionals), notionalDates_(notionalDates),
      paymentConvention_(paymentConvention), notionalInitialExchange_(notionalInitialExchange),
      notionalFinalExchange_(notionalFinalExchange), notionalAmortizingExchange_(notionalAmortizingExchange),
      isNotResetXCCY_(isNotResetXCCY), foreignCurrency_(foreignCurrency), foreignAmount_(foreignAmount),
      fxIndex_(fxIndex), fixingDays_(fixingDays), amortizationData_(amortizationData) {}

void LegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "LegData");
    string legType = XMLUtils::getChildValue(node, "LegType", true);
    isPayer_ = XMLUtils::getChildValueAsBool(node, "Payer");
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    dayCounter_ = XMLUtils::getChildValue(node, "DayCounter"); // optional
    paymentConvention_ = XMLUtils::getChildValue(node, "PaymentConvention");
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
            foreignCurrency_ = XMLUtils::getChildValue(fxResetNode, "ForeignCurrency");
            foreignAmount_ = XMLUtils::getChildValueAsDouble(fxResetNode, "ForeignAmount");
            fxIndex_ = XMLUtils::getChildValue(fxResetNode, "FXIndex");
            fixingDays_ = XMLUtils::getChildValueAsInt(fxResetNode, "FixingDays");
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
}

boost::shared_ptr<LegAdditionalData> LegData::initialiseConcreteLegData(const string& legType) {
    if (legType == "Fixed") {
        return boost::make_shared<FixedLegData>();
    } else if (legType == "Floating") {
        return boost::make_shared<FloatingLegData>();
    } else if (legType == "Cashflow") {
        return boost::make_shared<CashflowData>();
    } else if (legType == "CPI") {
        return boost::make_shared<CPILegData>();
    } else if (legType == "YY") {
        return boost::make_shared<YoYLegData>();
    } else if (legType == "CMS") {
        return boost::make_shared<CMSLegData>();
    } else {
        QL_FAIL("Unkown leg type " << legType);
    }
}

XMLNode* LegData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("LegData");
    QL_REQUIRE(node, "Failed to create LegData node");
    XMLUtils::addChild(doc, node, "LegType", legType());
    XMLUtils::addChild(doc, node, "Payer", isPayer_);
    XMLUtils::addChild(doc, node, "Currency", currency_);
    if (dayCounter_ != "")
        XMLUtils::addChild(doc, node, "DayCounter", dayCounter_);
    if (paymentConvention_ != "")
        XMLUtils::addChild(doc, node, "PaymentConvention", paymentConvention_);
    addChildrenWithOptionalAttributes(doc, node, "Notionals", "Notional", notionals_, "startDate", notionalDates_);

    if (!isNotResetXCCY_) {
        XMLNode* resetNode = doc.allocNode("FXReset");
        XMLUtils::addChild(doc, resetNode, "ForeignCurrency", foreignCurrency_);
        XMLUtils::addChild(doc, resetNode, "ForeignAmount", foreignAmount_);
        XMLUtils::addChild(doc, resetNode, "FXIndex", fxIndex_);
        XMLUtils::addChild(doc, resetNode, "FixingDays", fixingDays_);
        XMLUtils::appendNode(node, resetNode);
    }

    XMLNode* exchangeNode = doc.allocNode("Exchanges");
    XMLUtils::addChild(doc, exchangeNode, "NotionalInitialExchange", notionalInitialExchange_);
    XMLUtils::addChild(doc, exchangeNode, "NotionalFinalExchange", notionalFinalExchange_);
    XMLUtils::addChild(doc, exchangeNode, "NotionalAmortizingExchange", notionalAmortizingExchange_);
    XMLUtils::appendNode(node, exchangeNode);

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

namespace {
void applyAmortization(std::vector<Real>& notionals, const LegData& data, const Schedule& schedule,
                       const std::vector<Real>& rates = std::vector<Real>()) {
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
} // namespace

Leg makeFixedLeg(const LegData& data) {
    boost::shared_ptr<FixedLegData> fixedLegData = boost::dynamic_pointer_cast<FixedLegData>(data.concreteLegData());
    QL_REQUIRE(fixedLegData, "Wrong LegType, expected Fixed, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    vector<double> rates = buildScheduledVector(fixedLegData->rates(), fixedLegData->rateDates(), schedule);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    applyAmortization(notionals, data, schedule, rates);
    Leg leg = FixedRateLeg(schedule).withNotionals(notionals).withCouponRates(rates, dc).withPaymentAdjustment(bdc);
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
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    vector<double> spreads = buildScheduledVector(floatData->spreads(), floatData->spreadDates(), schedule);

    applyAmortization(notionals, data, schedule);
    // handle float annuity, which is not done in applyAmortization, for this we can only have one block
    if (!data.amortizationData().empty()) {
        AmortizationType amortizationType = parseAmortizationType(data.amortizationData().front().type());
        if (amortizationType == AmortizationType::Annuity) {
            LOG("Build floating annuity notional schedule");
            QL_REQUIRE(!hasCapsFloors, "Caps/Floors not supported in floating annuity coupons");
            QL_REQUIRE(floatData->gearings().size() == 0, "Gearings not supported in floating annuity coupons");
            DayCounter dc = index->dayCounter();
            vector<double> spreads = floatData->spreads();
            Date startDate = parseDate(data.amortizationData().front().startDate());
            double annuity = data.amortizationData().front().value();
            bool underflow = data.amortizationData().front().underflow();
            vector<boost::shared_ptr<Coupon>> coupons;
            for (Size i = 0; i < schedule.size() - 1; i++) {
                Real spread = (i < spreads.size() ? spreads[i] : spreads.back());
                if (schedule[i] < startDate || i == 0) {
                    Real nom = (i < notionals.size() ? notionals[i] : notionals.back());
                    boost::shared_ptr<IborCoupon> coupon = boost::make_shared<IborCoupon>(
                        schedule[i + 1], nom, schedule[i], schedule[i + 1], index->fixingDays(), index, 1.0, spread);
                    coupon->setPricer(boost::shared_ptr<IborCouponPricer>(new BlackIborCouponPricer));
                    coupons.push_back(coupon);
                    LOG("FloatingAnnuityCoupon: " << i << " " << coupon->nominal() << " " << coupon->amount());
                } else {
                    QL_REQUIRE(coupons.size() > 0,
                               "FloatingAnnuityCoupon needs at least one predecessor, e.g. a plain IborCoupon");
                    LOG("FloatingAnnuityCoupon, previous nominal/coupon: " << i << " " << coupons.back()->nominal()
                                                                           << " " << coupons.back()->amount());
                    boost::shared_ptr<QuantExt::FloatingAnnuityCoupon> coupon =
                        boost::make_shared<QuantExt::FloatingAnnuityCoupon>(
                            annuity, underflow, coupons.back(), schedule[i + 1], schedule[i], schedule[i + 1],
                            index->fixingDays(), index, 1.0, spread);
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

    IborLeg iborLeg = IborLeg(schedule, index)
                          .withNotionals(notionals)
                          .withSpreads(spreads)
                          .withPaymentDayCounter(dc)
                          .withPaymentAdjustment(bdc)
                          .withFixingDays(floatData->fixingDays())
                          .inArrears(floatData->isInArrears());

    if (floatData->gearings().size() > 0)
        iborLeg.withGearings(buildScheduledVector(floatData->gearings(), floatData->gearingDates(), schedule));

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

    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    vector<double> spreads = buildScheduledVector(floatData->spreads(), floatData->spreadDates(), schedule);

    applyAmortization(notionals, data, schedule);
    // amortization type annuit is not allowed, check this
    if (!data.amortizationData().empty()) {
        AmortizationType amortizationType = parseAmortizationType(data.amortizationData().front().type());
        QL_REQUIRE(amortizationType != AmortizationType::Annuity,
                   "AmortizationType " << data.amortizationData().front().type() << " not supported for OIS legs");
    }

    if (floatData->isAveraged()) {

        boost::shared_ptr<QuantExt::AverageONIndexedCouponPricer> couponPricer =
            boost::make_shared<QuantExt::AverageONIndexedCouponPricer>();
        QuantExt::AverageONLeg leg = QuantExt::AverageONLeg(schedule, index)
                                         .withNotionals(notionals)
                                         .withSpreads(spreads)
                                         .withPaymentDayCounter(dc)
                                         .withPaymentAdjustment(bdc)
                                         .withRateCutoff(2)
                                         .withAverageONIndexedCouponPricer(couponPricer);

        if (floatData->gearings().size() > 0)
            leg.withGearings(buildScheduledVector(floatData->gearings(), floatData->gearingDates(), schedule));

        return leg;

    } else {

        OvernightLeg leg = OvernightLeg(schedule, index)
                               .withNotionals(notionals)
                               .withSpreads(spreads)
                               .withPaymentDayCounter(dc)
                               .withPaymentAdjustment(bdc);

        if (floatData->gearings().size() > 0)
            leg.withGearings(buildScheduledVector(floatData->gearings(), floatData->gearingDates(), schedule));

        return leg;
    }
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

    Leg leg = CPILeg(schedule, index, cpiLegData->baseCPI(), observationLag)
                  .withNotionals(data.notionals())
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
        boost::shared_ptr<CPICashFlow> cpicf = boost::dynamic_pointer_cast<CPICashFlow>(leg[n - 1]);
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(leg[n - 2]);
        if (cpicf && (cpicf->date() == coupon->date()))
            leg.pop_back();
    }

    return leg;
}

Leg makeYoYLeg(const LegData& data, const boost::shared_ptr<YoYInflationIndex>& index) {
    boost::shared_ptr<YoYLegData> yoyLegData = boost::dynamic_pointer_cast<YoYLegData>(data.concreteLegData());
    QL_REQUIRE(yoyLegData, "Wrong LegType, expected YoY, got " << data.legType());

    Schedule schedule = makeSchedule(data.schedule());
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    Period observationLag = parsePeriod(yoyLegData->observationLag());
    vector<double> gearings = buildScheduledVector(yoyLegData->gearings(), yoyLegData->gearingDates(), schedule);
    vector<double> spreads = buildScheduledVector(yoyLegData->spreads(), yoyLegData->spreadDates(), schedule);

    // floors and caps not suported yet by QL yoy coupon pricer...
    Leg leg = yoyInflationLeg(schedule, schedule.calendar(), index, observationLag)
                  .withNotionals(data.notionals())
                  .withPaymentDayCounter(dc)
                  .withPaymentAdjustment(bdc)
                  .withFixingDays(yoyLegData->fixingDays())
                  .withGearings(gearings)
                  .withSpreads(spreads);
    QL_REQUIRE(leg.size() > 0, "Empty YoY Leg");
    return leg;
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

    vector<double> spreads = ore::data::buildScheduledVector(cmsData->spreads(), cmsData->spreadDates(), schedule);

    CmsLeg cmsLeg = CmsLeg(schedule, swapIndex)
                        .withNotionals(data.notionals())
                        .withSpreads(spreads)
                        .withPaymentDayCounter(dc)
                        .withPaymentAdjustment(bdc)
                        .withFixingDays(cmsData->fixingDays())
                        .inArrears(cmsData->isInArrears());

    if (cmsData->gearings().size() > 0)
        cmsLeg.withGearings(buildScheduledVector(cmsData->gearings(), cmsData->gearingDates(), schedule));

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
    }
    return tmpLeg;
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

} // namespace data
} // namespace ore
