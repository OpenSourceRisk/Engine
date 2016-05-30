/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/termstructures/blackvariancecurve3.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>

namespace QuantExt {

    BlackVarianceCurve3::BlackVarianceCurve3(
                                 Natural settlementDays,
                                 const Calendar& cal,
                                 BusinessDayConvention bdc,
                                 const DayCounter& dc,
                                 const std::vector<Time>& times,
                                 const std::vector<Handle<Quote> >& blackVolCurve)
    : BlackVarianceTermStructure(settlementDays, cal, bdc, dc),
      times_(times), quotes_(blackVolCurve) {

        QL_REQUIRE(times.size()==blackVolCurve.size(),
                   "mismatch between date vector and black vol vector");

        // cannot have dates[0]==referenceDate, since the
        // value of the vol at dates[0] would be lost
        // (variance at referenceDate must be zero)
        QL_REQUIRE(times[0] > 0, "cannot have times[0] <= 0");

        // Now insert 0 at the start of times_
        times_.insert(times_.begin(), 0);

        variances_ = std::vector<Real>(times_.size());
        variances_[0] = 0.0;
        for (Size j=1; j<times_.size(); j++) {
            QL_REQUIRE(times_[j]>times_[j-1], "times must be sorted unique!");
            registerWith(quotes_[j-1]);
        }
        varianceCurve_ = Linear().interpolate(times_.begin(), times_.end(),
                                              variances_.begin());
    }

    void BlackVarianceCurve3::update() {
        TermStructure::update(); // calls notifyObservers
        LazyObject::update(); // as does this
    }

    void BlackVarianceCurve3::performCalculations() const {
        for (Size j=1; j<= quotes_.size(); j++) {
            variances_[j] = times_[j] * quotes_[j-1]->value() *
                                        quotes_[j-1]->value();
            QL_REQUIRE(variances_[j]>=variances_[j-1],
                       "variance must be non-decreasing at j:" << j <<
                       " got var[j]:" << variances_[j] <<
                       " and var[j-1]:" << variances_[j-1]);
        }
        varianceCurve_.update();
    }

    Real BlackVarianceCurve3::blackVarianceImpl(Time t, Real) const {
        calculate();
        if (t<=times_.back()) {
            return varianceCurve_(t, true);
        } else {
            // extrapolate with flat vol
            return varianceCurve_(times_.back(), true)*t/times_.back();
        }
    }

}

