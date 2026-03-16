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

/*! \file cirppimplieddefaulttermstructure.hpp
    \brief default probability structure implied by a CIRPP model
    \ingroup models
*/

#ifndef quantext_cirpp_implied_survivalprob_ts_hpp
#define quantext_cirpp_implied_survivalprob_ts_hpp

#include <qle/models/crcirpp.hpp>

#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

/*! The termstructure has the reference date of the model's
    termstructure at construction, but you can vary this
    as well as the state.
    The purely time based variant is mainly there for
    performance reasons, note that it does not provide the
    full term structure interface and does not send
    notifications on reference time updates.

    \ingroup models
 */

class CirppImpliedDefaultTermStructure : public SurvivalProbabilityStructure {
public:
    CirppImpliedDefaultTermStructure(const QuantLib::ext::shared_ptr<CrCirpp>& model, const Size index,
                                     const DayCounter& dc = DayCounter(), const bool purelyTimeBased = false);

    Date maxDate() const override;
    Time maxTime() const override;

    const Date& referenceDate() const override;

    void referenceDate(const Date& d);
    void referenceTime(const Time t);
    void state(const Real y);
    void move(const Date& d, const Real y);
    void move(const Time t, const Real y);

    void update() override;

protected:
    Probability survivalProbabilityImpl(Time) const override;

    const QuantLib::ext::shared_ptr<CrCirpp> model_;
    const Size index_;
    const bool purelyTimeBased_;
    Date referenceDate_;
    Real relativeTime_, y_;
};

// inline

inline Date CirppImpliedDefaultTermStructure::maxDate() const {
    // we don't care - let the underlying classes throw
    // exceptions if applicable
    return Date::maxDate();
}

inline Time CirppImpliedDefaultTermStructure::maxTime() const {
    // see maxDate
    return QL_MAX_REAL;
}

inline const Date& CirppImpliedDefaultTermStructure::referenceDate() const {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    return referenceDate_;
}

inline void CirppImpliedDefaultTermStructure::referenceDate(const Date& d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    referenceDate_ = d;
    update();
}

inline void CirppImpliedDefaultTermStructure::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    relativeTime_ = t;
}

inline void CirppImpliedDefaultTermStructure::state(const Real y) {
    y_ = y;
}

inline void CirppImpliedDefaultTermStructure::move(const Date& d, const Real y) {
    state(y);
    referenceDate(d);
}

inline void CirppImpliedDefaultTermStructure::move(const Time t, const Real y) {
    state(y);
    referenceTime(t);
}

inline void CirppImpliedDefaultTermStructure::update() {
    if (!purelyTimeBased_) {
        relativeTime_ = dayCounter().yearFraction(model_->defaultCurve()->referenceDate(), referenceDate_);
    }
    notifyObservers();
}

inline Real CirppImpliedDefaultTermStructure::survivalProbabilityImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    if (QuantLib::close_enough(t, 0))
        return 1.0;
    return model_->survivalProbability(relativeTime_, relativeTime_+ t, y_);
}

} // namespace QuantExt

#endif
