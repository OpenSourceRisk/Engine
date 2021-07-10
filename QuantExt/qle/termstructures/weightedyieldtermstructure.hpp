/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file weightedyieldtermstructure.hpp
    \brief yield term structure given as a weighted average of yield term structures
    \ingroup models
*/

#ifndef quantext_weighted_yts_hpp
#define quantext_weighted_yts_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! weighted yield term structure
/*! this yield term structure is defined by discount factors given by a weighted geometric average of discount factors
  of underlying curves; this corresponds to a weighted arithmetic average of instantaneous forward rates and  can be
  used to interpolate e.g. a Euribor2M curve between Euribor1M and Euribor3M (using w1=w2=0.5)
*/
class WeightedYieldTermStructure : public YieldTermStructure {
public:
    WeightedYieldTermStructure(const Handle<YieldTermStructure>& yts1, const Handle<YieldTermStructure>& yts2,
                               const Real w1, const Real w2)
        : YieldTermStructure(yts1->dayCounter()), yts1_(yts1), yts2_(yts2), w1_(w1), w2_(w2) {
        QL_REQUIRE(yts1->dayCounter() == yts2->dayCounter(),
                   "WeightedYieldTermStructure(): sources have inconsistent day counters ("
                       << yts1->dayCounter().name() << " vs. " << yts2->dayCounter().name());
        registerWith(yts1);
        registerWith(yts2);
    }
    Date maxDate() const;
    const Date& referenceDate() const;

protected:
    Real discountImpl(Time t) const;
    const Handle<YieldTermStructure> yts1_, yts2_;
    const Real w1_, w2_;
};

// inline

inline Date WeightedYieldTermStructure::maxDate() const { return std::min(yts1_->maxDate(), yts2_->maxDate()); }

inline const Date& WeightedYieldTermStructure::referenceDate() const {
    QL_REQUIRE(yts1_->referenceDate() == yts2_->referenceDate(),
               "WeightedYieldTermStructure::referenceDate(): inconsitent reference dates in sources ("
                   << yts1_->referenceDate() << " vs. " << yts2_->referenceDate());
    return yts1_->referenceDate();
}

inline Real WeightedYieldTermStructure::discountImpl(Time t) const {
    return std::pow(yts1_->discount(t), w1_) * std::pow(yts2_->discount(t), w2_);
}

} // namespace QuantExt

#endif
