/*
 Copyright (C) 2026 AcadiaSoft, Inc.

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

#include <qle/instruments/swapoptimized.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>

namespace QuantExt {
using namespace QuantLib;

SwapOptimized::SwapOptimized(const std::vector<Leg>& legs, const std::vector<bool>& payer,
                             const Handle<YieldTermStructure>& discountCurve)
    : discountCurve_(discountCurve) {

    registerWith(discountCurve_);

    for (Size i = 0; i < legs.size(); ++i) {
        for (Size j = 0; j < legs[i].size(); ++j) {

            auto const& cf = legs[i][j];

            registerWith(cf);

            CashflowData d;

            d.payDate_ = cf->date();
            d.multiplier_ = payer[i] ? -1.0 : 1.0;
            d.amount_ = legs[i][j]->amount();

            if (ext::dynamic_pointer_cast<SimpleCashFlow>(cf) || ext::dynamic_pointer_cast<FixedRateCoupon>(cf)) {

                d.type_ = Type::Fixed;

            } else if (auto cpn = ext::dynamic_pointer_cast<IborCoupon>(cf)) {

                d.type_ = Type::Ibor;
                d.multiplier_ *= cpn->nominal();
                d.indexName_ = cpn->iborIndex()->name();
                d.indexTimeSeries_ = &cpn->iborIndex()->timeSeries();
                d.fixingDate_ = cpn->fixingDate();
                d.forwardCurve_ = cpn->iborIndex()->forwardingTermStructure().currentLink().get();
                d.index_d1_ = cpn->fixingValueDate();
                d.index_d2_ = cpn->fixingEndDate();
                d.gearing_ = cpn->gearing();
                d.spread_ = cpn->spread();
                d.accrualPeriod_ = cpn->accrualPeriod();
                d.indexPeriod_ = cpn->spanningTime();

            } else {
                QL_FAIL("SwapOptimized: coupon type leg #" << i << ", cashflow #" << j << " not handled.");
            }

            cashflowData_.push_back(d);

        } // loop j (coupons of leg i)
    } // loop i (legs)

    std::sort(cashflowData_.begin(), cashflowData_.end(),
              [](const CashflowData& x, const CashflowData& y) { return x.payDate_ < y.payDate_; });
}

bool SwapOptimized::isExpired() const {
    Date today = Settings::instance().evaluationDate();
    return cashflowData_.back().payDate_ <= today;
}

void SwapOptimized::performCalculations() const {

    double tmp = 0.0;

    const Date& today = discountCurve_->referenceDate();

    CashflowData ref;
    ref.payDate_ = today;

    auto start = std::upper_bound(cashflowData_.begin(), cashflowData_.end(), ref,
                                  [](const CashflowData& x, const CashflowData& y) { return x.payDate_ < y.payDate_; });

    for (auto c = start; c != cashflowData_.end(); ++c) {

        double dsc = discountCurve_->discount(c->payDate_);

        if (c->type_ == Type::Fixed) {

            tmp += c->multiplier_ * c->amount_ * dsc;

        } else {

            double rate;
            if (c->fixingDate_ < today) {
                rate = c->indexTimeSeries_->operator[](c->fixingDate_);
                QL_REQUIRE(rate != Null<Real>(), "SwapOptimized::performCalculations(): missing fixing for "
                                                     << c->indexName_ << " on " << c->fixingDate_);
            } else
                rate = (c->forwardCurve_->discount(c->index_d1_) / c->forwardCurve_->discount(c->index_d2_) - 1.0) /
                       c->indexPeriod_;
            tmp += (c->multiplier_ * c->gearing_ * rate + c->spread_) * c->accrualPeriod_ * dsc;
        }
    }

    NPV_ = tmp;
}

} // namespace QuantExt
