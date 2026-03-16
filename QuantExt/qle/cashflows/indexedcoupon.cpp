/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/cashflows/indexedcoupon.hpp>

#include <ql/patterns/visitor.hpp>
#include <ql/time/daycounter.hpp>

namespace QuantExt {

IndexedCoupon::IndexedCoupon(const QuantLib::ext::shared_ptr<Coupon>& c, const Real qty, const QuantLib::ext::shared_ptr<Index>& index,
                             const Date& fixingDate)
    : Coupon(c->date(), 0.0, c->accrualStartDate(), c->accrualEndDate(), c->referencePeriodStart(),
             c->referencePeriodEnd(), c->exCouponDate()),
      c_(c), qty_(qty), index_(index), fixingDate_(fixingDate), initialFixing_(Null<Real>()) {
    QL_REQUIRE(index, "IndexedCoupon: index is null");
    QL_REQUIRE(fixingDate != Date(), "IndexedCoupon: fixingDate is null");
    registerWith(c);
    registerWith(index);
}

IndexedCoupon::IndexedCoupon(const QuantLib::ext::shared_ptr<Coupon>& c, const Real qty, const Real initialFixing)
    : Coupon(c->date(), c->nominal(), c->accrualStartDate(), c->accrualEndDate(), c->referencePeriodStart(),
             c->referencePeriodEnd(), c->exCouponDate()),
      c_(c), qty_(qty), initialFixing_(initialFixing) {
    QL_REQUIRE(initialFixing != Null<Real>(), "IndexedCoupon: initial fixing is null");
    registerWith(c);
}

void IndexedCoupon::update() { notifyObservers(); }

Real IndexedCoupon::amount() const { return c_->amount() * multiplier(); }

Real IndexedCoupon::accruedAmount(const Date& d) const { return c_->accruedAmount(d) * multiplier(); }

Real IndexedCoupon::multiplier() const { return index_ ? qty_ * index_->fixing(fixingDate_) : qty_ * initialFixing_; }

Real IndexedCoupon::nominal() const { return c_->nominal() * multiplier(); }

Real IndexedCoupon::rate() const { return c_->rate(); }

DayCounter IndexedCoupon::dayCounter() const { return c_->dayCounter(); }

QuantLib::ext::shared_ptr<Coupon> IndexedCoupon::underlying() const { return c_; }

Real IndexedCoupon::quantity() const { return qty_; }

const Date& IndexedCoupon::fixingDate() const { return fixingDate_; }

Real IndexedCoupon::initialFixing() const { return initialFixing_; }

QuantLib::ext::shared_ptr<Index> IndexedCoupon::index() const { return index_; }

void IndexedCoupon::accept(AcyclicVisitor& v) {
    Visitor<IndexedCoupon>* v1 = dynamic_cast<Visitor<IndexedCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        Coupon::accept(v);
}

IndexWrappedCashFlow::IndexWrappedCashFlow(const QuantLib::ext::shared_ptr<CashFlow>& c, const Real qty,
                                           const QuantLib::ext::shared_ptr<Index>& index, const Date& fixingDate)
    : c_(c), qty_(qty), index_(index), fixingDate_(fixingDate), initialFixing_(Null<Real>()) {
    QL_REQUIRE(index, "IndexWrappedCashFlow: index is null");
    QL_REQUIRE(fixingDate != Date(), "IndexWrappedCashFlow: fixingDate is null");
    registerWith(c);
    registerWith(index);
}

IndexWrappedCashFlow::IndexWrappedCashFlow(const QuantLib::ext::shared_ptr<CashFlow>& c, const Real qty,
                                           const Real initialFixing)
    : c_(c), qty_(qty), initialFixing_(initialFixing) {
    QL_REQUIRE(initialFixing != Null<Real>(), "IndexWrappedCashFlow: initial fixing is null");
    registerWith(c);
}

void IndexWrappedCashFlow::update() { notifyObservers(); }

Date IndexWrappedCashFlow::date() const { return c_->date(); }
Real IndexWrappedCashFlow::amount() const { return c_->amount() * multiplier(); }

Real IndexWrappedCashFlow::multiplier() const {
    return index_ ? qty_ * index_->fixing(fixingDate_) : qty_ * initialFixing_;
}

QuantLib::ext::shared_ptr<CashFlow> IndexWrappedCashFlow::underlying() const { return c_; }

Real IndexWrappedCashFlow::quantity() const { return qty_; }

const Date& IndexWrappedCashFlow::fixingDate() const { return fixingDate_; }

Real IndexWrappedCashFlow::initialFixing() const { return initialFixing_; }

QuantLib::ext::shared_ptr<Index> IndexWrappedCashFlow::index() const { return index_; }

void IndexWrappedCashFlow::accept(AcyclicVisitor& v) {
    Visitor<IndexWrappedCashFlow>* v1 = dynamic_cast<Visitor<IndexWrappedCashFlow>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

IndexedCouponLeg::IndexedCouponLeg(const Leg& underlyingLeg, const Real qty, const QuantLib::ext::shared_ptr<Index>& index)
    : underlyingLeg_(underlyingLeg), qty_(qty), index_(index), initialFixing_(Null<Real>()),
      initialNotionalFixing_(Null<Real>()), fixingDays_(0), fixingCalendar_(NullCalendar()),
      fixingConvention_(Preceding), inArrearsFixing_(true) {
    QL_REQUIRE(index, "IndexedCouponLeg: index required");
}

IndexedCouponLeg& IndexedCouponLeg::withInitialFixing(const Real initialFixing) {
    initialFixing_ = initialFixing;
    return *this;
}

IndexedCouponLeg& IndexedCouponLeg::withInitialNotionalFixing(const Real initialNotionalFixing) {
    initialNotionalFixing_ = initialNotionalFixing;
    return *this;
}

IndexedCouponLeg& IndexedCouponLeg::withValuationSchedule(const Schedule& valuationSchedule) {
    valuationSchedule_ = valuationSchedule;
    return *this;
}

IndexedCouponLeg& IndexedCouponLeg::withFixingDays(const Size fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

IndexedCouponLeg& IndexedCouponLeg::withFixingCalendar(const Calendar& fixingCalendar) {
    fixingCalendar_ = fixingCalendar;
    return *this;
}

IndexedCouponLeg& IndexedCouponLeg::withFixingConvention(const BusinessDayConvention& fixingConvention) {
    fixingConvention_ = fixingConvention;
    return *this;
}

IndexedCouponLeg& IndexedCouponLeg::inArrearsFixing(const bool inArrearsFixing) {
    inArrearsFixing_ = inArrearsFixing;
    return *this;
}

IndexedCouponLeg::operator Leg() const {
    Leg resultLeg;
    resultLeg.reserve(underlyingLeg_.size());

    for (Size i = 0; i < underlyingLeg_.size(); ++i) {
        bool firstValuationDate = (i == 0);
        Date fixingDate;

        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(underlyingLeg_[i])) {
            if (valuationSchedule_.empty()) {
                fixingDate = inArrearsFixing_ ? cpn->accrualEndDate() : cpn->accrualStartDate();
            } else {
                if (valuationSchedule_.size() == underlyingLeg_.size() + 1) {
                    // valuation schedule corresponds one to one to underlying schedule
                    fixingDate = inArrearsFixing_ ? valuationSchedule_.date(i + 1) : valuationSchedule_.date(i);
                } else {
                    // look for the latest valuation date less or equal to the underlying accrual start date (if
                    // the indexing is using in advance fixing) resp. accrual end date (for in arrears fixing)
                    auto valDates = valuationSchedule_.dates();
                    Date refDate = inArrearsFixing_ ? cpn->accrualEndDate() : cpn->accrualStartDate();
                    Size index =
                        std::distance(valDates.begin(), std::upper_bound(valDates.begin(), valDates.end(), refDate));
                    QL_REQUIRE(index > 0, "IndexedCouponLeg: First valuation date ("
                                              << valDates.front() << ") must be leq accrual "
                                              << (inArrearsFixing_ ? "end" : "start") << " date ("
                                              << cpn->accrualStartDate() << ") of the " << (i + 1)
                                              << "th coupon in the underlying leg");
                    fixingDate = valuationSchedule_.date(index - 1);
                    firstValuationDate = (index == 1);
                }
            }

            fixingDate = fixingCalendar_.advance(fixingDate, -static_cast<int>(fixingDays_), Days, fixingConvention_);
            if (index_ != nullptr)
                fixingDate = index_->fixingCalendar().adjust(fixingDate, Preceding);

            if (firstValuationDate && initialFixing_ != Null<Real>()) {
                resultLeg.push_back(QuantLib::ext::make_shared<IndexedCoupon>(cpn, qty_, initialFixing_));
            } else {
                resultLeg.push_back(QuantLib::ext::make_shared<IndexedCoupon>(cpn, qty_, index_, fixingDate));
            }
        } else if (auto csf = QuantLib::ext::dynamic_pointer_cast<CashFlow>(underlyingLeg_[i])) {
            fixingDate = fixingCalendar_.advance(csf->date(), -static_cast<int>(fixingDays_), Days, fixingConvention_);
            if (firstValuationDate && initialNotionalFixing_ != Null<Real>()) {
                // use firstNotionalFixing if the first flow is a cashflow (not a coupon)
                resultLeg.push_back(QuantLib::ext::make_shared<IndexWrappedCashFlow>(csf, qty_, initialNotionalFixing_));
            } else if (!valuationSchedule_.empty() && valuationSchedule_.dates().size() > 0 &&
                       valuationSchedule_.date(0) == csf->date() && initialFixing_ != Null<Real>()) {
                // use the initial fixing if the cashflow date equals the first date in the val schedule
                resultLeg.push_back(QuantLib::ext::make_shared<IndexWrappedCashFlow>(csf, qty_, initialFixing_));
            } else {
                // use a flow with free fixing otherwise
                resultLeg.push_back(QuantLib::ext::make_shared<IndexWrappedCashFlow>(csf, qty_, index_, fixingDate));
            }
        } else {
            QL_FAIL("IndexedCouponLeg: coupon or cashflow required");
        }
    }

    return resultLeg;
}

QuantLib::ext::shared_ptr<CashFlow> unpackIndexedCouponOrCashFlow(const QuantLib::ext::shared_ptr<CashFlow>& c) {
    if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c))
        return unpackIndexedCoupon(cpn);
    else
        return unpackIndexWrappedCashFlow(c);
}

QuantLib::ext::shared_ptr<Coupon> unpackIndexedCoupon(const QuantLib::ext::shared_ptr<Coupon>& c) {
    if (auto cpn = QuantLib::ext::dynamic_pointer_cast<IndexedCoupon>(c)) {
        QuantLib::ext::shared_ptr<Coupon> unpacked_cpn = cpn->underlying();
        return unpackIndexedCoupon(unpacked_cpn);
    } else
        return c;
}

QuantLib::ext::shared_ptr<CashFlow> unpackIndexWrappedCashFlow(const QuantLib::ext::shared_ptr<CashFlow>& c) {
    if (auto cf = QuantLib::ext::dynamic_pointer_cast<IndexWrappedCashFlow>(c)) {
        QuantLib::ext::shared_ptr<CashFlow> unpacked_cf = cf->underlying();
        return unpackIndexWrappedCashFlow(unpacked_cf);
    } else
        return c;
}

Real getIndexedCouponOrCashFlowMultiplier(const QuantLib::ext::shared_ptr<CashFlow>& c) {
    if (auto cpn = QuantLib::ext::dynamic_pointer_cast<IndexedCoupon>(c)) {
        return cpn->multiplier() * getIndexedCouponOrCashFlowMultiplier(cpn->underlying());
    } else if (auto cf = QuantLib::ext::dynamic_pointer_cast<IndexWrappedCashFlow>(c)) {
        return cf->multiplier() * getIndexedCouponOrCashFlowMultiplier(cf->underlying());
    } else {
        return 1.0;
    }
}

std::vector<std::tuple<Date, QuantLib::ext::shared_ptr<Index>, Real>>
getIndexedCouponOrCashFlowFixingDetails(const QuantLib::ext::shared_ptr<CashFlow>& c) {
    if (auto cpn = QuantLib::ext::dynamic_pointer_cast<IndexedCoupon>(c)) {
        auto v = getIndexedCouponOrCashFlowFixingDetails(cpn->underlying());
        v.push_back(std::make_tuple(cpn->fixingDate(), cpn->index(), cpn->multiplier()));
        return v;
    } else if (auto cf = QuantLib::ext::dynamic_pointer_cast<IndexWrappedCashFlow>(c)) {
        auto v = getIndexedCouponOrCashFlowFixingDetails(cf->underlying());
        v.push_back(std::make_tuple(cf->fixingDate(), cf->index(), cf->multiplier()));
        return v;
    } else {
        return {};
    }
}

} // namespace QuantExt
