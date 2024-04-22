/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file modelimpliedyieldtermstructure.hpp
    \brief yield term structure implied by an IR model
    \ingroup models
*/

#pragma once

#include <qle/models/irmodel.hpp>

#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {
using namespace QuantLib;

//! IR Implied Yield Term Structure
/*! The termstructure has the reference date of the model's
    termstructure at construction, but you can vary this
    as well as the state.
    The purely time based variant is mainly there for
    perfomance reasons, note that it does not provide the
    full term structure interface and does not send
    notifications on reference time updates.

        \ingroup models
 */

class ModelImpliedYieldTermStructure : public YieldTermStructure {
public:
    ModelImpliedYieldTermStructure(const QuantLib::ext::shared_ptr<IrModel>& model, const DayCounter& dc = DayCounter(),
                                   const bool purelyTimeBased = false);

    Date maxDate() const override;
    Time maxTime() const override;

    const Date& referenceDate() const override;

    virtual void referenceDate(const Date& d);
    virtual void referenceTime(const Time t);
    void state(const Array& s);
    void move(const Date& d, const Array& s);
    void move(const Time t, const Array& s);

    virtual void update() override;

protected:
    Real discountImpl(Time t) const override;

    const QuantLib::ext::shared_ptr<IrModel> model_;
    const bool purelyTimeBased_;
    Date referenceDate_;
    Real relativeTime_;
    Array state_;
};

//! Model Implied Yts Fwd Corrected
/*! the target curve should have a reference date consistent with
  the model's term structure

  \ingroup models
*/
class ModelImpliedYtsFwdFwdCorrected : public ModelImpliedYieldTermStructure {
public:
    ModelImpliedYtsFwdFwdCorrected(const QuantLib::ext::shared_ptr<IrModel>& model,
                                   const Handle<YieldTermStructure> targetCurve, const DayCounter& dc = DayCounter(),
                                   const bool purelyTimeBased = false);

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
class ModelImpliedYtsSpotCorrected : public ModelImpliedYieldTermStructure {
public:
    ModelImpliedYtsSpotCorrected(const QuantLib::ext::shared_ptr<IrModel>& model, const Handle<YieldTermStructure> targetCurve,
                                 const DayCounter& dc, const bool purelyTimeBased);

protected:
    Real discountImpl(Time t) const override;

private:
    const Handle<YieldTermStructure> targetCurve_;
};

// inline

inline Date ModelImpliedYieldTermStructure::maxDate() const {
    // we don't care - let the underlying classes throw
    // exceptions if applicable
    return Date::maxDate();
}

inline Time ModelImpliedYieldTermStructure::maxTime() const {
    // see maxDate
    return QL_MAX_REAL;
}

inline const Date& ModelImpliedYieldTermStructure::referenceDate() const {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    return referenceDate_;
}

inline void ModelImpliedYieldTermStructure::referenceDate(const Date& d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    referenceDate_ = d;
    update();
}

inline void ModelImpliedYtsFwdFwdCorrected::referenceDate(const Date& d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    referenceDate_ = d;
    update();
}

inline void ModelImpliedYieldTermStructure::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    relativeTime_ = t;
    notifyObservers();
}

inline void ModelImpliedYtsFwdFwdCorrected::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    relativeTime_ = t;
    notifyObservers();
}

inline void ModelImpliedYieldTermStructure::state(const Array& s) {
    state_ = s;
    notifyObservers();
}

inline void ModelImpliedYieldTermStructure::move(const Date& d, const Array& s) {
    state_ = s;
    referenceDate(d);
}

inline void ModelImpliedYieldTermStructure::move(const Time t, const Array& s) {
    state_ = s;
    referenceTime(t);
    notifyObservers();
}

inline void ModelImpliedYieldTermStructure::update() {
    if (!purelyTimeBased_) {
        relativeTime_ = dayCounter().yearFraction(model_->termStructure()->referenceDate(), referenceDate_);
    }
    notifyObservers();
}

inline Real ModelImpliedYieldTermStructure::discountImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    return model_->discountBond(relativeTime_, relativeTime_ + t, state_);
}

inline Real ModelImpliedYtsFwdFwdCorrected::discountImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    // if relativeTime_ is close to zero, we return the discount factor directly from the target curve
    if (QuantLib::close_enough(relativeTime_, 0.0)) {
        return targetCurve_->discount(t);
    } else {
        return model_->discountBond(relativeTime_, relativeTime_ + t, state_, targetCurve_);
    }
}

inline Real ModelImpliedYtsSpotCorrected::discountImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    return ModelImpliedYieldTermStructure::discountImpl(t) * targetCurve_->discount(t) *
           model_->termStructure()->discount(relativeTime_) / model_->termStructure()->discount(relativeTime_ + t);
}

} // namespace QuantExt
