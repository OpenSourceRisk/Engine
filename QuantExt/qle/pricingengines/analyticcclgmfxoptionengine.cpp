/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/models/crossassetanalytics.hpp>

#include <ql/pricingengines/blackcalculator.hpp>

namespace QuantExt {

using namespace CrossAssetAnalytics;

AnalyticCcLgmFxOptionEngine::AnalyticCcLgmFxOptionEngine(
    const boost::shared_ptr<CrossAssetModel> &model, const Size foreignCurrency)
    : model_(model), foreignCurrency_(foreignCurrency), cacheEnabled_(false),
      cacheDirty_(true) {}

Real AnalyticCcLgmFxOptionEngine::value(
    const Time t0, const Time t,
    const boost::shared_ptr<StrikedTypePayoff> payoff,
    const Real domesticDiscount, const Real fxForward) const {
    Real H0 = Hz(0).eval(model_.get(), t);
    Real Hi = Hz(foreignCurrency_ + 1).eval(model_.get(), t);

    // just a shortcut
    const Size &i = foreignCurrency_;

    const CrossAssetModel *x = model_.get();

    if (cacheDirty_ || !cacheEnabled_ ||
        !(close_enough(cachedT0_, t0) && close_enough(cachedT_, t))) {
        cachedIntegrals_ =
            // first term
            H0 * H0 * (zetaz(0).eval(x, t) - zetaz(0).eval(x, t0)) -
            2.0 * H0 * integral(x, P(Hz(0), az(0), az(0)), t0, t) +
            integral(x, P(Hz(0), Hz(0), az(0), az(0)), t0, t) +
            // second term
            Hi * Hi * (zetaz(i + 1).eval(x, t) - zetaz(i + 1).eval(x, t0)) -
            2.0 * Hi * integral(x, P(Hz(i + 1), az(i + 1), az(i + 1)), t0, t) +
            integral(x, P(Hz(i + 1), Hz(i + 1), az(i + 1), az(i + 1)), t0, t) -
            // third term
            2.0 *
                (H0 * Hi *
                     integral(x, P(az(0), az(i + 1), rzz(0, i + 1)), t0, t) -
                 H0 * integral(x, P(Hz(i + 1), az(i + 1), az(0), rzz(i + 1, 0)),
                               t0, t) -
                 Hi * integral(x, P(Hz(0), az(0), az(i + 1), rzz(0, i + 1)), t0,
                               t) +
                 integral(x,
                          P(Hz(0), Hz(i + 1), az(0), az(i + 1), rzz(0, i + 1)),
                          t0, t));
        cacheDirty_ = false;
        cachedT0_ = t0;
        cachedT_ = t;
    }

    Real variance =
        cachedIntegrals_ +
        // term two three/fourth
        (vx(i).eval(x, t) - vx(i).eval(x, t0)) +
        // forth term
        2.0 * (H0 * integral(x, P(az(0), sx(i), rzx(0, i)), t0, t) -
               integral(x, P(Hz(0), az(0), sx(i), rzx(0, i)), t0, t)) -
        // fifth term
        2.0 *
            (Hi * integral(x, P(az(i + 1), sx(i), rzx(i + 1, i)), t0, t) -
             integral(x, P(Hz(i + 1), az(i + 1), sx(i), rzx(i + 1, i)), t0, t));

    BlackCalculator black(payoff, fxForward, std::sqrt(variance),
                          domesticDiscount);

    return black.value();
}

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

    results_.value = value(0.0, t, payoff, domesticDiscount, fxForward);

} // calculate()

} // namespace QuantExt
