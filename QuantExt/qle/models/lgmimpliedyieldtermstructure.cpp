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

#include <qle/models/lgmimpliedyieldtermstructure.hpp>

namespace QuantExt {

LgmImpliedYieldTermStructure::LgmImpliedYieldTermStructure(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                                                           const DayCounter& dc, const bool purelyTimeBased,
                                                           const bool cacheValues)
    : YieldTermStructure(dc == DayCounter() ? model->parametrization()->termStructure()->dayCounter() : dc),
      cacheValues_(cacheValues), model_(model), purelyTimeBased_(purelyTimeBased),
      referenceDate_(purelyTimeBased ? Null<Date>() : model_->parametrization()->termStructure()->referenceDate()),
      state_(0.0) {
    registerWith(model_);
    update();
}

LgmImpliedYtsFwdFwdCorrected::LgmImpliedYtsFwdFwdCorrected(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                                                           const Handle<YieldTermStructure> targetCurve,
                                                           const DayCounter& dc, const bool purelyTimeBased,
                                                           const bool cacheValues)
    : LgmImpliedYieldTermStructure(model, dc, purelyTimeBased, cacheValues), targetCurve_(targetCurve) {

    if (cacheValues_) {
        dt_ = targetCurve_->discount(relativeTime_);
        zeta_ = model_->parametrization()->zeta(relativeTime_);
        Ht_ = model_->parametrization()->H(relativeTime_);
    }
    registerWith(targetCurve_);
}

LgmImpliedYtsSpotCorrected::LgmImpliedYtsSpotCorrected(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                                                       const Handle<YieldTermStructure> targetCurve,
                                                       const DayCounter& dc, const bool purelyTimeBased,
                                                       const bool cacheValues)
    : LgmImpliedYieldTermStructure(model, dc, purelyTimeBased, cacheValues), targetCurve_(targetCurve) {
    registerWith(targetCurve_);
}

} // namespace QuantExt
