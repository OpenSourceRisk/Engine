/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
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

#include <qle/cashflows/cashflowtable.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

CashflowRow::CashflowRow()
    : startDate_(Null<Date>()), endDate_(Null<Date>()), startNotional_(Null<Real>()), endNotional_(Null<Real>()),
      couponAmount_(Null<Real>()), allInRate_(Null<Rate>()), rate_(Null<Rate>()), spread_(Null<Spread>()),
      discount_(Null<DiscountFactor>()) {}

CashflowRow& CashflowRow::withStartDate(const Date& startDate) {
    startDate_ = startDate;
    return *this;
}

CashflowRow& CashflowRow::withEndDate(const Date& endDate) {
    endDate_ = endDate;
    return *this;
}

CashflowRow& CashflowRow::withStartNotional(Real startNotional) {
    startNotional_ = startNotional;
    return *this;
}

CashflowRow& CashflowRow::withEndNotional(Real endNotional) {
    endNotional_ = endNotional;
    return *this;
}

CashflowRow& CashflowRow::withCouponAmount(Real couponAmount) {
    couponAmount_ = couponAmount;
    return *this;
}

CashflowRow& CashflowRow::withAllInRate(Rate allInRate) {
    allInRate_ = allInRate;
    return *this;
}

CashflowRow& CashflowRow::withRate(Rate rate) {
    rate_ = rate;
    return *this;
}

CashflowRow& CashflowRow::withSpread(Spread spread) {
    spread_ = spread;
    return *this;
}

CashflowRow& CashflowRow::withDiscount(DiscountFactor discount) {
    discount_ = discount;
    return *this;
}

void CashflowTable::add(const CashflowRow& cashflowRow) { rows_.push_back(cashflowRow); }

Size CashflowTable::size() const { return rows_.size(); }

const CashflowRow& CashflowTable::operator[](Size i) const {
    // Throws out_of_range error if i is out of range
    // TODO: is this enough e.g. QuantLib::Schedule for example
    return rows_.at(i);
}

CashflowRow& CashflowTable::operator[](Size i) { return rows_[i]; }

} // namespace QuantExt
