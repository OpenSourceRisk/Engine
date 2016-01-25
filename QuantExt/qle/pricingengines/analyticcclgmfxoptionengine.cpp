/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/models/xassetanalytics.hpp>

#include <ql/pricingengines/blackcalculator.hpp>

namespace QuantExt {

using namespace XAssetAnalytics;

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

    Real H0 = Hz(0).eval(model_.get(), t);
    Real Hi = Hz(foreignCurrency_ + 1).eval(model_.get(), t);

    // just a shortcut
    const Size &i = foreignCurrency_;

    const XAssetModel *x = model_.get();

    Real variance =
        // first term
        H0 * H0 * integral(x, P(az(0), az(0)), 0.0, t) -
        2.0 * H0 * integral(x, P(Hz(0), az(0), az(0)), 0.0, t) +
        integral(x, P(Hz(0), Hz(0), az(0), az(0)), 0.0, t) +
        // second term
        Hi * Hi * integral(x, P(az(i + 1), az(i + 1)), 0.0, t) -
        2.0 * Hi * integral(x, P(Hz(i + 1), az(i + 1), az(i + 1)), 0.0, t) +
        integral(x, P(Hz(i + 1), Hz(i + 1), az(i + 1), az(i + 1)), 0.0, t) +
        // term two three/fourth
        integral(x, P(sx(i), sx(i)), 0.0, t) -
        // third term
        2.0 *
            (H0 * Hi * integral(x, P(az(0), az(i + 1), rzz(0, i + 1)), 0.0, t) -
             H0 * integral(x, P(Hz(i + 1), az(i + 1), az(0), rzz(i + 1, 0)),
                           0.0, t) -
             Hi * integral(x, P(Hz(0), az(0), az(i + 1), rzz(0, i + 1)), 0.0,
                           t) +
             integral(x, P(Hz(0), Hz(i + 1), az(0), az(i + 1), rzz(0, i + 1)),
                      0.0, t)) +
        // forth term
        2.0 * (H0 * integral(x, P(az(0), sx(i), rzx(0, i)), 0.0, t) -
               integral(x, P(Hz(0), az(0), sx(i), rzx(0, i)), 0.0, t)) -
        // fifth term
        2.0 * (Hi * integral(x, P(az(i + 1), sx(i), rzx(i + 1, i)), 0.0, t) -
               integral(x, P(Hz(i + 1), az(i + 1), sx(i), rzx(i + 1, i)), 0.0,
                        t));

    BlackCalculator black(payoff, fxForward, std::sqrt(variance),
                          domesticDiscount);

    results_.value = black.value();

} // calculate()

} // namespace QuantExt
