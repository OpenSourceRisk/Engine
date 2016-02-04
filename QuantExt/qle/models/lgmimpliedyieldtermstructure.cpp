/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/models/lgmimpliedyieldtermstructure.hpp>

namespace QuantExt {

LgmImpliedYieldTermStructure::LgmImpliedYieldTermStructure(
    const boost::shared_ptr<LinearGaussMarkovModel> &model,
    const DayCounter &dc, const bool purelyTimeBased)
    : YieldTermStructure(
          dc == DayCounter()
              ? model->parametrization()->termStructure()->dayCounter()
              : dc),
      model_(model), purelyTimeBased_(purelyTimeBased),
      referenceDate_(
          purelyTimeBased
              ? Null<Date>()
              : model_->parametrization()->termStructure()->referenceDate()),
      state_(0.0) {
    registerWith(model_);
    update();
}

LgmImpliedYtsFwdFwdCorrected::LgmImpliedYtsFwdFwdCorrected(
    const boost::shared_ptr<LinearGaussMarkovModel> &model,
    const Handle<YieldTermStructure> targetCurve, const DayCounter &dc,
    const bool purelyTimeBased)
    : LgmImpliedYieldTermStructure(model, dc, purelyTimeBased),
      targetCurve_(targetCurve) {
    registerWith(targetCurve_);
}

LgmImpliedYtsSpotCorrected::LgmImpliedYtsSpotCorrected(
    const boost::shared_ptr<LinearGaussMarkovModel> &model,
    const Handle<YieldTermStructure> targetCurve, const DayCounter &dc,
    const bool purelyTimeBased)
    : LgmImpliedYieldTermStructure(model, dc, purelyTimeBased),
      targetCurve_(targetCurve) {
    registerWith(targetCurve_);
}

} // namespace QuantExt
