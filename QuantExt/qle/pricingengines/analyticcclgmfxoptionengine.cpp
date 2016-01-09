/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <ql/pricingengines/blackcalculator.hpp>

namespace QuantExt {

AnalyticCcLgmFxOptionEngine::AnalyticCcLgmFxOptionEngine(
    const boost::shared_ptr<XAssetModel> &model, const Size foreignCurrency)
    : model_(model), foreignCurrency_(foreignCurrency) {}

void AnalyticCcLgmFxOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.exercise->type() == Exercise::European,
               "only European options are allowed");

    boost::shared_ptr<StrikedTypePayoff> payoff =
        boost::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    QL_REQUIRE(payoff != NULL, "only striked payoff is allowed");

    Date expiry = arguments_.exercise->lastDate();
    Time t = model_->irlgm1f(0)->termStructure()->timeFromReference(expiry);

    if (t <= 0.0) {
        // option is expired, we do not value any possibly non settled
        // flows, i.e. set the npv to zero in this case
        results_.value = 0.0;
        return;
    }

    Real foreignDiscount = model_->irlgm1f(foreignCurrency_ + 1)
                               ->termStructure()
                               ->discount(expiry);
    Real domesticDiscount =
        model_->irlgm1f(0)->termStructure()->discount(expiry);

    Real fxForward = model_->fxbs(foreignCurrency_)->fxSpotToday()->value() *
                     foreignDiscount / domesticDiscount;

    Real H0 = model_->irlgm1f(0)->H(t);
    Real Hi = model_->irlgm1f(foreignCurrency_ + 1)->H(t);

    const Size na = Null<Size>();
    // just a shortcut
    const Size &i = foreignCurrency_;

    Real variance =
        // first term
        H0 * H0 * model_->integral(na, na, 0, 0, na, na, 0.0, t) -
        2.0 * H0 * model_->integral(0, na, 0, 0, na, na, 0.0, t) +
        model_->integral(0, 0, 0, 0, na, na, 0.0, t) +
        // second term
        Hi * Hi * model_->integral(na, na, i + 1, i + 1, na, na, 0.0, t) -
        2.0 * Hi * model_->integral(i + 1, na, i + 1, i + 1, na, na, 0.0, t) +
        model_->integral(i + 1, i + 1, i + 1, i + 1, na, na, 0.0, t) +
        // term two three/fourth
        model_->integral(na, na, na, na, i, i, 0.0, t) -
        // third term
        2.0 * (H0 * Hi * model_->integral(na, na, 0, i + 1, na, na, 0.0, t) -
               H0 * model_->integral(i + 1, na, i + 1, 0, na, na, 0.0, t) -
               Hi * model_->integral(0, na, 0, i + 1, na, na, 0.0, t) +
               model_->integral(0, i + 1, 0, i + 1, na, na, 0.0, t)) +
        // forth term
        2.0 * (H0 * model_->integral(na, na, 0, na, na, i, 0.0, t) -
               model_->integral(0, na, 0, na, na, i, 0.0, t)) -
        // fifth term
        2.0 * (Hi * model_->integral(na, na, i + 1, na, na, i, 0.0, t) -
               model_->integral(i + 1, na, i + 1, na, na, i, 0.0, t));

    BlackCalculator black(payoff, fxForward, std::sqrt(variance),
                          domesticDiscount);

    results_.value = black.value();

} // calculate()

} // namespace QuantExt
