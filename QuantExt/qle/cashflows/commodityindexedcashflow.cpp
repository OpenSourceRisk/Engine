/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ql/utilities/vectors.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>

using namespace QuantLib;
using std::vector;

namespace QuantExt {

CommodityIndexedCashFlow::CommodityIndexedCashFlow(Real quantity, const Date& pricingDate, const Date& paymentDate,
                                                   const ext::shared_ptr<CommodityIndex>& index, Real spread,
                                                   Real gearing, bool useFuturePrice, const Date& contractDate,
                                                   const ext::shared_ptr<FutureExpiryCalculator>& calc,
                                                   QuantLib::Natural dailyExpiryOffset, const ext::shared_ptr<FxIndex>& fxIndex)
    : CommodityCashFlow(quantity, spread, gearing, useFuturePrice, index, fxIndex), pricingDate_(pricingDate),
      paymentDate_(paymentDate), futureMonthOffset_(0), periodQuantity_(quantity), dailyExpiryOffset_(dailyExpiryOffset) {

    QL_REQUIRE(paymentDate != Date(), "CommodityIndexedCashFlow: payment date is null");
    init(calc, contractDate);
}

CommodityIndexedCashFlow::CommodityIndexedCashFlow(
    Real quantity, const Date& startDate, const Date& endDate, const ext::shared_ptr<CommodityIndex>& index,
    Natural paymentLag, const Calendar& paymentCalendar, BusinessDayConvention paymentConvention, Natural pricingLag,
    const Calendar& pricingLagCalendar, Real spread, Real gearing, PaymentTiming paymentTiming, bool isInArrears,
    bool useFuturePrice, bool useFutureExpiryDate, Natural futureMonthOffset,
    const ext::shared_ptr<FutureExpiryCalculator>& calc, const QuantLib::Date& paymentDateOverride,
    const QuantLib::Date& pricingDateOverride, QuantLib::Natural dailyExpiryOffset,
    const ext::shared_ptr<FxIndex>& fxIndex)
    : CommodityCashFlow(quantity, spread, gearing, useFuturePrice, index, fxIndex), pricingDate_(pricingDateOverride),
      paymentDate_(paymentDateOverride), useFutureExpiryDate_(useFutureExpiryDate),
      futureMonthOffset_(futureMonthOffset), periodQuantity_(quantity), dailyExpiryOffset_(dailyExpiryOffset) {

    // Derive the pricing date if an explicit override has not been provided
    if (pricingDate_ == Date()) {
        pricingDate_ = isInArrears ? endDate : startDate;
        if (!useFuturePrice_ || !useFutureExpiryDate_) {
            // We just use the pricing date rules to get the pricing date
            pricingDate_ = pricingLagCalendar.advance(pricingDate_, -static_cast<Integer>(pricingLag), Days, Preceding);
        } else {
            // We need to use the expiry date of the future contract
            QL_REQUIRE(calc, "CommodityIndexedCashFlow needs a valid future "
                                 << "expiry calculator when using first future");
            Date expiry = calc->expiryDate(pricingDate_, futureMonthOffset_);
            if (dailyExpiryOffset_ != Null<Natural>()) {
                expiry = index_->fixingCalendar().advance(expiry, dailyExpiryOffset_ * Days);
            }
            pricingDate_ = expiry;
        }
    }

    // We may not need the month and year if we are not using a future settlement price but get them
    // and pass them here in any case to init
    Date ref = isInArrears ? endDate : startDate;

    init(calc, ref, paymentTiming, startDate, endDate, paymentLag, paymentConvention, paymentCalendar);
}

void CommodityIndexedCashFlow::performCalculations() const {
    double fxRate = (fxIndex_) ? this->fxIndex()->fixing(pricingDate_) : 1.0;
    amount_ = periodQuantity_ * gearing_ * (fxRate * index_->fixing(pricingDate_) + spread_);
}

Real CommodityIndexedCashFlow::amount() const {
    calculate();
    return amount_;
}

void CommodityIndexedCashFlow::accept(AcyclicVisitor& v) {
    if (Visitor<CommodityIndexedCashFlow>* v1 = dynamic_cast<Visitor<CommodityIndexedCashFlow>*>(&v))
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

void CommodityIndexedCashFlow::setPeriodQuantity(Real periodQuantity) { periodQuantity_ = periodQuantity; }

void CommodityIndexedCashFlow::init(const ext::shared_ptr<FutureExpiryCalculator>& calc, const Date& contractDate,
                                    const PaymentTiming paymentTiming, const Date& startDate, const Date& endDate,
                                    const Natural paymentLag, const BusinessDayConvention paymentConvention,
                                    const Calendar& paymentCalendar) {

    pricingDate_ = index_->fixingCalendar().adjust(pricingDate_, Preceding);

    // If we are using the future settlement price as the reference price, then we need to create the
    // relevant "future index" here and update the cashflow's index with it.
    Date expiry;
    if (useFuturePrice_) {
        QL_REQUIRE(calc, "CommodityIndexedCashFlow needs a valid future expiry calculator when using "
                             << "the future settlement price as reference price");
        expiry = calc->expiryDate(contractDate, futureMonthOffset_);
        if (dailyExpiryOffset_ != Null<Natural>()) {
            expiry = index_->fixingCalendar().advance(expiry, dailyExpiryOffset_ * Days);
        }
        index_ = index_->clone(expiry);
    }

    // Derive the payment date if an explicit override has not been provided
    if (paymentDate_ == Date()) {
        if (paymentTiming == PaymentTiming::InAdvance) {
            QL_REQUIRE(startDate != Date(), "CommodityIndexedCashFlow: startDate is null, can not derive paymentDate.");
            paymentDate_ = startDate;
        } else if (paymentTiming == PaymentTiming::InArrears) {
            QL_REQUIRE(endDate != Date(), "CommodityIndexedCashFlow: endDate is null, can not derive paymentDate.");
            paymentDate_ = endDate;
        } else if (paymentTiming == PaymentTiming::RelativeToExpiry) {
            QL_REQUIRE(
                expiry != Date(),
                "CommodityIndexedCashFlow: payment relative to expiry date only possibly when future price is used.");
            paymentDate_ = expiry;
        }
        paymentDate_ = paymentCalendar.advance(paymentDate_, paymentLag, Days, paymentConvention);
    }

    // the pricing date has to lie on or before the payment date
    pricingDate_ = index_->fixingCalendar().adjust(std::min(paymentDate_, pricingDate_), Preceding);
    
    indices_[pricingDate_] = index_;
    
    registerWith(index_);
}

CommodityIndexedLeg::CommodityIndexedLeg(const Schedule& schedule, const ext::shared_ptr<CommodityIndex>& index)
    : schedule_(schedule), index_(index), paymentLag_(0), paymentCalendar_(NullCalendar()),
      paymentConvention_(Unadjusted), pricingLag_(0), pricingLagCalendar_(NullCalendar()),
      paymentTiming_(CommodityIndexedCashFlow::PaymentTiming::InArrears), inArrears_(true), useFuturePrice_(false),
      useFutureExpiryDate_(true), futureMonthOffset_(0), payAtMaturity_(false), dailyExpiryOffset_(Null<Natural>()) {}

CommodityIndexedLeg& CommodityIndexedLeg::withQuantities(Real quantity) {
    quantities_ = vector<Real>(1, quantity);
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withQuantities(const vector<Real>& quantities) {
    quantities_ = quantities;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPaymentLag(Natural paymentLag) {
    paymentLag_ = paymentLag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPaymentCalendar(const Calendar& paymentCalendar) {
    paymentCalendar_ = paymentCalendar;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPaymentConvention(BusinessDayConvention paymentConvention) {
    paymentConvention_ = paymentConvention;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPricingLag(Natural pricingLag) {
    pricingLag_ = pricingLag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPricingLagCalendar(const Calendar& pricingLagCalendar) {
    pricingLagCalendar_ = pricingLagCalendar;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withSpreads(Real spread) {
    spreads_ = vector<Real>(1, spread);
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withSpreads(const vector<Real>& spreads) {
    spreads_ = spreads;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withGearings(Real gearing) {
    gearings_ = vector<Real>(1, gearing);
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withGearings(const vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::paymentTiming(CommodityIndexedCashFlow::PaymentTiming paymentTiming) {
    paymentTiming_ = paymentTiming;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::inArrears(bool flag) {
    inArrears_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::useFuturePrice(bool flag) {
    useFuturePrice_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::useFutureExpiryDate(bool flag) {
    useFutureExpiryDate_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withFutureMonthOffset(Natural futureMonthOffset) {
    futureMonthOffset_ = futureMonthOffset;
    return *this;
}

CommodityIndexedLeg&
CommodityIndexedLeg::withFutureExpiryCalculator(const ext::shared_ptr<FutureExpiryCalculator>& calc) {
    calc_ = calc;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::payAtMaturity(bool flag) {
    payAtMaturity_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPricingDates(const vector<Date>& pricingDates) {
    pricingDates_ = pricingDates;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPaymentDates(const vector<Date>& paymentDates) {
    paymentDates_ = paymentDates;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withDailyExpiryOffset(Natural dailyExpiryOffset) {
    dailyExpiryOffset_ = dailyExpiryOffset;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withFxIndex(const ext::shared_ptr<FxIndex>& fxIndex) {
    fxIndex_ = fxIndex;
    return *this;
}

CommodityIndexedLeg::operator Leg() const {

    // Number of commodity indexed cashflows
    Size numberCashflows = schedule_.size() - 1;

    // Initial consistency checks
    QL_REQUIRE(!quantities_.empty(), "No quantities given");
    QL_REQUIRE(quantities_.size() <= numberCashflows,
               "Too many quantities (" << quantities_.size() << "), only " << numberCashflows << " required");
    if (useFuturePrice_) {
        QL_REQUIRE(calc_, "CommodityIndexedCashFlow needs a valid future expiry calculator when using first future");
    }
    if (!pricingDates_.empty()) {
        QL_REQUIRE(pricingDates_.size() == numberCashflows, "Expected the number of explicit pricing dates ("
                                                                << pricingDates_.size()
                                                                << ") to equal the number of calculation periods ("
                                                                << numberCashflows << ")");
    }
    if (!paymentDates_.empty()) {
        QL_REQUIRE(paymentDates_.size() == numberCashflows, "Expected the number of explicit payment dates ("
                                                                << paymentDates_.size()
                                                                << ") to equal the number of calculation periods ("
                                                                << numberCashflows << ")");
    }

    // If pay at maturity, populate payment date.
    Date paymentDate;
    if (payAtMaturity_) {
        paymentDate = paymentCalendar_.advance(schedule_.dates().back(), paymentLag_ * Days, paymentConvention_);
    }

    // Leg to hold the result
    Leg leg;
    leg.reserve(numberCashflows);
    for (Size i = 0; i < numberCashflows; ++i) {

        Date start = schedule_.date(i);
        Date end = schedule_.date(i + 1);
        Real quantity = detail::get(quantities_, i, 1.0);
        Real spread = detail::get(spreads_, i, 0.0);
        Real gearing = detail::get(gearings_, i, 1.0);
        Date pricingDate = detail::get(pricingDates_, i, Date());

        // If explicit payment dates provided, use them.
        if (!paymentDates_.empty()) {
            paymentDate = paymentDates_[i];
        }

        leg.push_back(ext::make_shared<CommodityIndexedCashFlow>(
            quantity, start, end, index_, paymentLag_, paymentCalendar_, paymentConvention_, pricingLag_,
            pricingLagCalendar_, spread, gearing, paymentTiming_, inArrears_, useFuturePrice_, useFutureExpiryDate_,
            futureMonthOffset_, calc_, paymentDate, pricingDate, dailyExpiryOffset_, fxIndex_));
    }

    return leg;
}

} // namespace QuantExt
