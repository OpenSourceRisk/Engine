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

using namespace QuantLib;

namespace QuantExt {

//! Lgm Implied Yield Term Structure
/*! The termstructure has the reference date of the model's
    termstructure at construction, but you can vary this
    as well as the state.
    The purely time based variant is mainly there for
    perfomance reasons, note that it does not provide the
    full term structure interface and does not send
    notifications on reference time updates.

        \ingroup models
 */

class LgmImpliedYieldTermStructure : public YieldTermStructure {
public:
    LgmImpliedYieldTermStructure(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                                 const DayCounter& dc = DayCounter(), const bool purelyTimeBased = false);

    Date maxDate() const;
    Time maxTime() const;

    const Date& referenceDate() const;

    void referenceDate(const Date& d);
    void referenceTime(const Time t);
    void state(const Real s);
    void move(const Date& d, const Real s);
    void move(const Time t, const Real s);

    void update();

protected:
    Real discountImpl(Time t) const;

    const boost::shared_ptr<LinearGaussMarkovModel> model_;
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
    LgmImpliedYtsFwdFwdCorrected(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                                 const Handle<YieldTermStructure> targetCurve, const DayCounter& dc = DayCounter(),
                                 const bool purelyTimeBased = false);

protected:
    Real discountImpl(Time t) const;

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
    LgmImpliedYtsSpotCorrected(const boost::shared_ptr<LinearGaussMarkovModel>& model,
                               const Handle<YieldTermStructure> targetCurve, const DayCounter& dc,
                               const bool purelyTimeBased);

protected:
    Real discountImpl(Time t) const;

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

inline void LgmImpliedYieldTermStructure::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    relativeTime_ = t;
}

inline void LgmImpliedYieldTermStructure::state(const Real s) {
    state_ = s;
    notifyObservers();
}

inline void LgmImpliedYieldTermStructure::move(const Date& d, const Real s) {
    state(s);
    referenceDate(d);
}

inline void LgmImpliedYieldTermStructure::move(const Time t, const Real s) {
    state(s);
    referenceTime(t);
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
    return LgmImpliedYieldTermStructure::discountImpl(t) * targetCurve_->discount(relativeTime_ + t) /
           targetCurve_->discount(relativeTime_) * model_->parametrization()->termStructure()->discount(relativeTime_) /
           model_->parametrization()->termStructure()->discount(relativeTime_ + t);
}

inline Real LgmImpliedYtsSpotCorrected::discountImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    return LgmImpliedYieldTermStructure::discountImpl(t) * targetCurve_->discount(t) *
           model_->parametrization()->termStructure()->discount(relativeTime_) /
           model_->parametrization()->termStructure()->discount(relativeTime_ + t);
}

} // namespace QuantExt

#endif
