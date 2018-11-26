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

#ifndef quantext_lgm_implied_survivalprob_ts_hpp
#define quantext_lgm_implied_survivalprob_ts_hpp

#include <qle/models/crossassetmodel.hpp>

#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! The termstructure has the reference date of the model's
    termstructure at construction, but you can vary this
    as well as the state.
    The purely time based variant is mainly there for
    perfomance reasons, note that it does not provide the
    full term structure interface and does not send
    notifications on reference time updates.

    \ingroup models
 */

class LgmImpliedDefaultTermStructure : public SurvivalProbabilityStructure {
public:
    LgmImpliedDefaultTermStructure(const boost::shared_ptr<CrossAssetModel>& model, const Size index,
                                   const Size currency, const DayCounter& dc = DayCounter(),
                                   const bool purelyTimeBased = false);

    Date maxDate() const;
    Time maxTime() const;

    const Date& referenceDate() const;

    void referenceDate(const Date& d);
    void referenceTime(const Time t);
    void state(const Real z, const Real y);
    void move(const Date& d, const Real z, const Real y);
    void move(const Time t, const Real z, const Real y);

    void update();

protected:
    Rate hazardRateImpl(Time) const;
    Probability survivalProbabilityImpl(Time) const;

    const boost::shared_ptr<CrossAssetModel> model_;
    const Size index_, currency_;
    const bool purelyTimeBased_;
    Date referenceDate_;
    Real relativeTime_, z_, y_;
};

// inline

inline Date LgmImpliedDefaultTermStructure::maxDate() const {
    // we don't care - let the underlying classes throw
    // exceptions if applicable
    return Date::maxDate();
}

inline Time LgmImpliedDefaultTermStructure::maxTime() const {
    // see maxDate
    return QL_MAX_REAL;
}

inline const Date& LgmImpliedDefaultTermStructure::referenceDate() const {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    return referenceDate_;
}

inline void LgmImpliedDefaultTermStructure::referenceDate(const Date& d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    referenceDate_ = d;
    update();
}

inline void LgmImpliedDefaultTermStructure::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    relativeTime_ = t;
}

inline void LgmImpliedDefaultTermStructure::state(const Real z, const Real y) {
    z_ = z;
    y_ = y;
}

inline void LgmImpliedDefaultTermStructure::move(const Date& d, const Real z, const Real y) {
    state(z, y);
    referenceDate(d);
}

inline void LgmImpliedDefaultTermStructure::move(const Time t, const Real z, const Real y) {
    state(z, y);
    referenceTime(t);
}

inline void LgmImpliedDefaultTermStructure::update() {
    if (!purelyTimeBased_) {
        relativeTime_ = dayCounter().yearFraction(model_->irlgm1f(0)->termStructure()->referenceDate(), referenceDate_);
    }
    notifyObservers();
}

inline Real LgmImpliedDefaultTermStructure::survivalProbabilityImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    return model_->crlgm1fS(index_, currency_, relativeTime_, relativeTime_ + t, z_, y_).second;
}

} // namespace QuantExt

#endif
