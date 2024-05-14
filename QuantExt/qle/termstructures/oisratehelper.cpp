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

#include <ql/cashflows/cashflows.hpp>
#include <ql/instruments/makeois.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/utilities/null_deleter.hpp>

#include <qle/termstructures/oisratehelper.hpp>

namespace QuantExt {

OISRateHelper::OISRateHelper(Natural settlementDays, const Period& swapTenor, const Handle<Quote>& fixedRate,
                             const QuantLib::ext::shared_ptr<OvernightIndex>& overnightIndex, const DayCounter& fixedDayCounter,
                             const Calendar& fixedCalendar, Natural paymentLag, bool endOfMonth,
                             Frequency paymentFrequency, BusinessDayConvention fixedConvention,
                             BusinessDayConvention paymentAdjustment, DateGeneration::Rule rule,
                             const Handle<YieldTermStructure>& discountingCurve, bool telescopicValueDates,
                             Pillar::Choice pillar, Date customPillarDate)
    : RelativeDateRateHelper(fixedRate), settlementDays_(settlementDays), swapTenor_(swapTenor),
      overnightIndex_(overnightIndex), fixedDayCounter_(fixedDayCounter), fixedCalendar_(fixedCalendar),
      paymentLag_(paymentLag), endOfMonth_(endOfMonth), paymentFrequency_(paymentFrequency),
      fixedConvention_(fixedConvention), paymentAdjustment_(paymentAdjustment), rule_(rule),
      discountHandle_(discountingCurve), telescopicValueDates_(telescopicValueDates), pillarChoice_(pillar) {

    pillarDate_ = customPillarDate;

    bool onIndexHasCurve = !overnightIndex_->forwardingTermStructure().empty();
    bool haveDiscountCurve = !discountHandle_.empty();
    QL_REQUIRE(!(onIndexHasCurve && haveDiscountCurve), "Have both curves nothing to solve for.");

    if (!onIndexHasCurve) {
        QuantLib::ext::shared_ptr<IborIndex> clonedIborIndex(overnightIndex_->clone(termStructureHandle_));
        overnightIndex_ = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(clonedIborIndex);
        overnightIndex_->unregisterWith(termStructureHandle_);
    }

    registerWith(overnightIndex_);
    registerWith(discountHandle_);
    initializeDates();
}

void OISRateHelper::initializeDates() {

    Calendar paymentCalendar_ = overnightIndex_->fixingCalendar();

    swap_ = MakeOIS(swapTenor_, overnightIndex_, 0.0)
                .withSettlementDays(settlementDays_)
                .withFixedLegDayCount(fixedDayCounter_)
                .withEndOfMonth(endOfMonth_)
                .withPaymentFrequency(paymentFrequency_)
                .withRule(rule_)
                .withPaymentCalendar(paymentCalendar_)
                .withPaymentAdjustment(paymentAdjustment_)
                .withPaymentLag(paymentLag_)
                .withDiscountingTermStructure(discountRelinkableHandle_)
                .withTelescopicValueDates(telescopicValueDates_);
    // TODO: patch QL?
    //.withFixedAccrualConvention(fixedConvention_)
    //..withFixedCalendar(fixedCalendar_)

    earliestDate_ = swap_->startDate();
    maturityDate_ = swap_->maturityDate();

    Date lastPaymentDate = std::max(swap_->overnightLeg().back()->date(), swap_->fixedLeg().back()->date());
    latestRelevantDate_ = std::max(maturityDate_, lastPaymentDate);

    switch (pillarChoice_) {
    case Pillar::MaturityDate:
        pillarDate_ = maturityDate_;
        break;
    case Pillar::LastRelevantDate:
        pillarDate_ = latestRelevantDate_;
        break;
    case Pillar::CustomDate:
        // pillarDate_ already assigned at construction time
        QL_REQUIRE(pillarDate_ >= earliestDate_, "pillar date (" << pillarDate_
                                                                 << ") must be later "
                                                                    "than or equal to the instrument's earliest date ("
                                                                 << earliestDate_ << ")");
        QL_REQUIRE(pillarDate_ <= latestRelevantDate_, "pillar date ("
                                                           << pillarDate_
                                                           << ") must be before "
                                                              "or equal to the instrument's latest relevant date ("
                                                           << latestRelevantDate_ << ")");
        break;
    default:
        QL_FAIL("unknown Pillar::Choice(" << Integer(pillarChoice_) << ")");
    }

    // for backwards compatibility
    latestDate_ = pillarDate_;
}

void OISRateHelper::setTermStructure(YieldTermStructure* t) {
    // do not set the relinkable handle as an observer
    // force recalculation when needed
    bool observer = false;

    QuantLib::ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
    termStructureHandle_.linkTo(temp, observer);

    if (discountHandle_.empty())
        discountRelinkableHandle_.linkTo(temp, observer);
    else
        discountRelinkableHandle_.linkTo(*discountHandle_, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

Real OISRateHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "term structure not set");
    // we didn't register as observers - force calculation
    swap_->deepUpdate();
    return swap_->fairRate();
}

void OISRateHelper::accept(AcyclicVisitor& v) {
    Visitor<OISRateHelper>* v1 = dynamic_cast<Visitor<OISRateHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}

DatedOISRateHelper::DatedOISRateHelper(const Date& startDate, const Date& endDate, const Handle<Quote>& fixedRate,
                                       const QuantLib::ext::shared_ptr<OvernightIndex>& overnightIndex,
                                       const DayCounter& fixedDayCounter, const Calendar& fixedCalendar,
                                       Natural paymentLag, Frequency paymentFrequency,
                                       BusinessDayConvention fixedConvention, BusinessDayConvention paymentAdjustment,
                                       DateGeneration::Rule rule, const Handle<YieldTermStructure>& discountingCurve,
                                       bool telescopicValueDates, Pillar::Choice pillar, Date customPillarDate)
    : RateHelper(fixedRate), overnightIndex_(overnightIndex), fixedDayCounter_(fixedDayCounter),
      fixedCalendar_(fixedCalendar), paymentLag_(paymentLag), paymentFrequency_(paymentFrequency),
      fixedConvention_(fixedConvention), paymentAdjustment_(paymentAdjustment), rule_(rule),
      discountHandle_(discountingCurve), telescopicValueDates_(telescopicValueDates), pillarChoice_(pillar) {

    pillarDate_ = customPillarDate;

    bool onIndexHasCurve = !overnightIndex_->forwardingTermStructure().empty();
    bool haveDiscountCurve = !discountHandle_.empty();
    QL_REQUIRE(!(onIndexHasCurve && haveDiscountCurve), "Have both curves nothing to solve for.");

    if (!onIndexHasCurve) {
        QuantLib::ext::shared_ptr<IborIndex> clonedIborIndex(overnightIndex_->clone(termStructureHandle_));
        overnightIndex_ = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(clonedIborIndex);
        overnightIndex_->unregisterWith(termStructureHandle_);
    }

    registerWith(overnightIndex_);
    registerWith(discountHandle_);

    swap_ = MakeOIS(Period(), overnightIndex_, 0.0)
                .withEffectiveDate(startDate)
                .withTerminationDate(endDate)
                .withFixedLegDayCount(fixedDayCounter_)
                .withPaymentFrequency(paymentFrequency_)
                .withRule(rule_)
                // TODO: patch QL
                //.withFixedAccrualConvention(fixedConvention_)
                // .withFixedCalendar(fixedCalendar_)
                .withPaymentCalendar(overnightIndex_->fixingCalendar())
                .withPaymentAdjustment(paymentAdjustment_)
                .withPaymentLag(paymentLag_)
                .withDiscountingTermStructure(termStructureHandle_)
                .withTelescopicValueDates(telescopicValueDates_);

    earliestDate_ = swap_->startDate();
    maturityDate_ = swap_->maturityDate();

    Date lastPaymentDate = std::max(swap_->overnightLeg().back()->date(), swap_->fixedLeg().back()->date());
    latestRelevantDate_ = std::max(maturityDate_, lastPaymentDate);

    switch (pillarChoice_) {
    case Pillar::MaturityDate:
        pillarDate_ = maturityDate_;
        break;
    case Pillar::LastRelevantDate:
        pillarDate_ = latestRelevantDate_;
        break;
    case Pillar::CustomDate:
        // pillarDate_ already assigned at construction time
        QL_REQUIRE(pillarDate_ >= earliestDate_, "pillar date (" << pillarDate_
                                                                 << ") must be later "
                                                                    "than or equal to the instrument's earliest date ("
                                                                 << earliestDate_ << ")");
        QL_REQUIRE(pillarDate_ <= latestRelevantDate_, "pillar date ("
                                                           << pillarDate_
                                                           << ") must be before "
                                                              "or equal to the instrument's latest relevant date ("
                                                           << latestRelevantDate_ << ")");
        break;
    default:
        QL_FAIL("unknown Pillar::Choice(" << Integer(pillarChoice_) << ")");
    }

    // for backwards compatibility
    latestDate_ = pillarDate_;
}

void DatedOISRateHelper::setTermStructure(YieldTermStructure* t) {
    // do not set the relinkable handle as an observer -
    // force recalculation when needed
    bool observer = false;

    QuantLib::ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
    termStructureHandle_.linkTo(temp, observer);

    if (discountHandle_.empty())
        discountRelinkableHandle_.linkTo(temp, observer);
    else
        discountRelinkableHandle_.linkTo(*discountHandle_, observer);

    RateHelper::setTermStructure(t);
}

Real DatedOISRateHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "term structure not set");
    // we didn't register as observers - force calculation
    swap_->deepUpdate();
    return swap_->fairRate();
}

void DatedOISRateHelper::accept(AcyclicVisitor& v) {
    Visitor<DatedOISRateHelper>* v1 = dynamic_cast<Visitor<DatedOISRateHelper>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
