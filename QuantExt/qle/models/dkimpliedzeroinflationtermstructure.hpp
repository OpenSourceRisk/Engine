/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file dkimpliedzeroinflationtermstructure.hpp
    \brief zero inflation term structure implied by a DK model
    \ingroup models
*/

#ifndef quantext_dk_implied_zits_hpp
#define quantext_dk_implied_zits_hpp

#include <qle/models/crossassetmodel.hpp>

#include <ql/termstructures/inflationtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Dk Implied Zero Inflation Term Structure
/*! The termstructure has the reference date of the model's
termstructure at construction, but you can vary this
as well as the state.
The purely time based variant is mainly there for
perfomance reasons, note that it does not provide the
full term structure interface and does not send
notifications on reference time updates.

\ingroup models
*/

class DkImpliedZeroInflationTermStructure : public ZeroInflationTermStructure {
public:
    DkImpliedZeroInflationTermStructure(const boost::shared_ptr<CrossAssetModel>& model, Size index);

    Date maxDate() const;
    Time maxTime() const;
    Date baseDate() const;

    const Date& referenceDate() const;

    void referenceDate(const Date& d);
    void state(const Real s_z, const Real s_y);
    void move(const Date& d, const Real s_z, const Real s_y);

    void update();
    Real I_t(Time t) const;

protected:
    Real zeroRateImpl(Time t) const;

    const boost::shared_ptr<CrossAssetModel> model_;
    Size index_;
    Date referenceDate_;
    Real relativeTime_, state_z_, state_y_;
};

// inline

inline Date DkImpliedZeroInflationTermStructure::maxDate() const {
    // we don't care - let the underlying classes throw
    // exceptions if applicable
    return Date::maxDate();
}

inline Time DkImpliedZeroInflationTermStructure::maxTime() const {
    // see maxDate
    return QL_MAX_REAL;
}

inline Date DkImpliedZeroInflationTermStructure::baseDate() const { return referenceDate_ - observationLag_; }

inline const Date& DkImpliedZeroInflationTermStructure::referenceDate() const { return referenceDate_; }

inline void DkImpliedZeroInflationTermStructure::referenceDate(const Date& d) {
    referenceDate_ = d;
    relativeTime_ = dayCounter().yearFraction(model_->infdk(index_)->termStructure()->referenceDate(), referenceDate_);
    update();
}

inline void DkImpliedZeroInflationTermStructure::state(const Real s_z, const Real s_y) {
    state_z_ = s_z;
    state_y_ = s_y;
    notifyObservers();
}

inline void DkImpliedZeroInflationTermStructure::move(const Date& d, const Real s_z, const Real s_y) {
    state(s_z, s_y);
    referenceDate(d);
}

inline void DkImpliedZeroInflationTermStructure::update() { notifyObservers(); }

inline Real DkImpliedZeroInflationTermStructure::zeroRateImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    std::pair<Real, Real> ii = model_->infdkI(index_, relativeTime_, relativeTime_ + t, state_z_, state_y_);
    return std::pow(ii.second, 1 / t) - 1;
}

inline Real DkImpliedZeroInflationTermStructure::I_t(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    std::pair<Real, Real> ii = model_->infdkI(index_, relativeTime_, relativeTime_ + t, state_z_, state_y_);
    return ii.first;
}

} // namespace QuantExt

#endif
