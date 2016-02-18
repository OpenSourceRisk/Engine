/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file lgmimpliedyieldtermstructure.hpp
    \brief yield term structure implied by a LGM model
*/

#ifndef quantext_statically_corrected_yts_hpp
#define quantext_statically_corrected_yts_hpp

#include <qle/termstructures/dynamicstype.hpp>

#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! This termstructure takes a floating reference date term structure
  and two fixed reference date term structures, applying a static
  correction to the floating ts implied by the two fixed ones.
  Usually the floating term structure will coincide with
  the first fixed at construction time. Also, the two fixed
  termstructures should have the same reference date and all three
  termstructures should have the same day counter. */
class StaticallyCorrectedYieldTermStructure : public YieldTermStructure {
  public:
    StaticallyCorrectedYieldTermStructure(
        const Handle<YieldTermStructure> &floatingTermStructure,
        const Handle<YieldTermStructure> &fixedSourceTermStructure,
        const Handle<YieldTermStructure> &fixedTargetTermStructure,
        const YieldCurveRollDown &rollDown = ForwardForward)
        : YieldTermStructure(floatingTermStructure->settlementDays(),
                             floatingTermStructure->calendar(),
                             floatingTermStructure->dayCounter()),
          x_(floatingTermStructure), source_(fixedSourceTermStructure),
          target_(fixedTargetTermStructure), rollDown_(rollDown),
          referenceDate_(fixedSourceTermStructure->referenceDate()) {
        registerWith(floatingTermStructure);
        registerWith(fixedSourceTermStructure);
        registerWith(fixedTargetTermStructure);
    }

    Date maxDate() const { return x_->maxDate(); }

  protected:
    Real discountImpl(Time t) const;

  private:
    const Handle<YieldTermStructure> x_, source_, target_;
    const YieldCurveRollDown rollDown_;
    const Date referenceDate_;
};

// inline

inline Real StaticallyCorrectedYieldTermStructure::discountImpl(Time t) const {
    Real t0 = source_->timeFromReference(referenceDate_);
    Real c = rollDown_ == ConstantDiscounts
                 ? target_->discount(t) / source_->discount(t)
                 : target_->discount(t0 + t) * source_->discount(t0) /
                       (target_->discount(t0) * source_->discount(t0 + t));

    return x_->discount(t) * c;
}

} // namesapce QuantExt

#endif
