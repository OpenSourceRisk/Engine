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

/*! \file lgmimpliedyieldtermstructure.hpp
    \brief yield term structure implied by a LGM model
    \ingroup models
*/

#ifndef quantext_lgm_implied_yts_hpp
#define quantext_lgm_implied_yts_hpp

#include <qle/models/lgm.hpp>

#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Lgm Implied Yield Term Structure
/*! The termstructure has the reference date of the model's
    termstructure at construction, but you can vary this
    as well as the state.
    The purely time based variant is mainly there for
    performance reasons, note that it does not provide the
    full term structure interface and does not send
    notifications on reference time updates.

        \ingroup models
 */

class LgmImpliedYieldTermStructure : public YieldTermStructure {
public:
    LgmImpliedYieldTermStructure(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                 const DayCounter& dc = DayCounter(), const bool purelyTimeBased = false,
                                 const bool cacheValues = false);

    Date maxDate() const override;
    Time maxTime() const override;

    const Date& referenceDate() const override;

    virtual void referenceDate(const Date& d);
    virtual void referenceTime(const Time t);
    void state(const Real s);
    void move(const Date& d, const Real s);
    void move(const Time t, const Real s);

    virtual void update() override;

protected:
    Real discountImpl(Time t) const override;
    mutable Real dt_;
    mutable Real zeta_;
    mutable Real Ht_;
    bool cacheValues_;

    const QuantLib::ext::shared_ptr<LinearGaussMarkovModel> model_;
    const bool purelyTimeBased_;
    Date referenceDate_;
    Real relativeTime_, state_;
};

//! Lgm Implied Yts Fwd Corrected
/*! the target curve should have a reference date consistent with
  the model's term structure

  \ingroup models
*/
class LgmImpliedYtsFwdFwdCorrected : public LgmImpliedYieldTermStructure {
public:
    LgmImpliedYtsFwdFwdCorrected(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                 const Handle<YieldTermStructure> targetCurve, const DayCounter& dc = DayCounter(),
                                 const bool purelyTimeBased = false, const bool cacheValues = false);

    void referenceDate(const Date& d) override;
    void referenceTime(const Time t) override;

protected:
    Real discountImpl(Time t) const override;

private:
    const Handle<YieldTermStructure> targetCurve_;
};

//! Lgm Implied Yts Spot Corrected
/*! the target curve should have a reference date consistent with
  the model's term structure

  \ingroup models
*/
class LgmImpliedYtsSpotCorrected : public LgmImpliedYieldTermStructure {
public:
    LgmImpliedYtsSpotCorrected(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                               const Handle<YieldTermStructure> targetCurve, const DayCounter& dc,
                               const bool purelyTimeBased, const bool cacheValues = false);

protected:
    Real discountImpl(Time t) const override;

private:
    const Handle<YieldTermStructure> targetCurve_;
};

// inline

inline Date LgmImpliedYieldTermStructure::maxDate() const {
    // we don't care - let the underlying classes throw
    // exceptions if applicable
    return Date::maxDate();
}

inline Time LgmImpliedYieldTermStructure::maxTime() const {
    // see maxDate
    return QL_MAX_REAL;
}

inline const Date& LgmImpliedYieldTermStructure::referenceDate() const {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    return referenceDate_;
}

inline void LgmImpliedYieldTermStructure::referenceDate(const Date& d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    referenceDate_ = d;
    update();
}

inline void LgmImpliedYtsFwdFwdCorrected::referenceDate(const Date& d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    Date oldDate = referenceDate_;
    referenceDate_ = d;
    update();
    if (cacheValues_ && oldDate != referenceDate_) {
        dt_ = targetCurve_->discount(relativeTime_);
        zeta_ = model_->parametrization()->zeta(relativeTime_);
        Ht_ = model_->parametrization()->H(relativeTime_);
    }
}

inline void LgmImpliedYieldTermStructure::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    relativeTime_ = t;
    notifyObservers();
}

inline void LgmImpliedYtsFwdFwdCorrected::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    if (cacheValues_ && relativeTime_ != t) {
        dt_ = targetCurve_->discount(t);
        zeta_ = model_->parametrization()->zeta(t);
        Ht_ = model_->parametrization()->H(t);
    }
    relativeTime_ = t;

    notifyObservers();
}

inline void LgmImpliedYieldTermStructure::state(const Real s) {
    state_ = s;
    notifyObservers();
}

inline void LgmImpliedYieldTermStructure::move(const Date& d, const Real s) {

    state_ = s;
    referenceDate(d);
}

inline void LgmImpliedYieldTermStructure::move(const Time t, const Real s) {
    state_ = s;
    referenceTime(t);

    notifyObservers();
}

inline void LgmImpliedYieldTermStructure::update() {
    if (!purelyTimeBased_) {
        relativeTime_ =
            dayCounter().yearFraction(model_->parametrization()->termStructure()->referenceDate(), referenceDate_);
    }

    notifyObservers();
}

inline Real LgmImpliedYieldTermStructure::discountImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    return model_->discountBond(relativeTime_, relativeTime_ + t, state_);
}

inline Real LgmImpliedYtsFwdFwdCorrected::discountImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    // if relativeTime_ is close to zero, we return the discount factor directly from the target curve
    if (QuantLib::close_enough(relativeTime_, 0.0)) {
        return targetCurve_->discount(t);
    } else {
        Real HT = model_->parametrization()->H(relativeTime_ + t);
        if (!cacheValues_) {
            dt_ = targetCurve_->discount(relativeTime_);
            zeta_ = model_->parametrization()->zeta(relativeTime_);
            Ht_ = model_->parametrization()->H(relativeTime_);
        }
        return std::exp(-(HT - Ht_) * state_ - 0.5 * (HT * HT - Ht_ * Ht_) * zeta_) *
               targetCurve_->discount(relativeTime_ + t) / dt_;
    }
}

inline Real LgmImpliedYtsSpotCorrected::discountImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    return LgmImpliedYieldTermStructure::discountImpl(t) * targetCurve_->discount(t) *
           model_->parametrization()->termStructure()->discount(relativeTime_) /
           model_->parametrization()->termStructure()->discount(relativeTime_ + t);
}

} // namespace QuantExt

#endif
