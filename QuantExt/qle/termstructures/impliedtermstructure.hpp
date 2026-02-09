/*
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2008 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*
 Copyright (C) 2025 Quaternion Risk Management Ltd.
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

/*! \file impliedtermstructure.hpp
    \brief Implied term structure
*/

#pragma once

#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

//! Implied term structure at a given date or time in the future
/*! The given date will be the implied reference date.

  \note This term structure will remain linked to the original
        structure, i.e., any changes in the latter will be
        reflected in this structure as well.

  \warning If the reference time variant is used, the reference date
           of the new ts will be wrong (namely simply the ref date
           of the original ts). Therefore only time based inspectors
           will yield valid results.
*/
class ImpliedTermStructure : public YieldTermStructure {
public:
    ImpliedTermStructure(const Handle<YieldTermStructure>&, const Date& referenceDate);
    ImpliedTermStructure(const Handle<YieldTermStructure>&, const Time referenceTime);
    //! \name YieldTermStructure interface
    //@{
    DayCounter dayCounter() const;
    Calendar calendar() const;
    Natural settlementDays() const;
    Date maxDate() const;

protected:
    DiscountFactor discountImpl(Time) const;
    //@}
private:
    Handle<YieldTermStructure> originalCurve_;
    const Time referenceTime_;
};

// inline definitions

inline ImpliedTermStructure::ImpliedTermStructure(const Handle<YieldTermStructure>& h, const Date& referenceDate)
    : YieldTermStructure(referenceDate), originalCurve_(h), referenceTime_(Null<Time>()) {
    registerWith(originalCurve_);
}

inline ImpliedTermStructure::ImpliedTermStructure(const Handle<YieldTermStructure>& h, const Time referenceTime)
    : YieldTermStructure(h->referenceDate()), originalCurve_(h), referenceTime_(referenceTime) {
    registerWith(originalCurve_);
}

inline DayCounter ImpliedTermStructure::dayCounter() const { return originalCurve_->dayCounter(); }

inline Calendar ImpliedTermStructure::calendar() const { return originalCurve_->calendar(); }

inline Natural ImpliedTermStructure::settlementDays() const { return originalCurve_->settlementDays(); }

inline Date ImpliedTermStructure::maxDate() const { return originalCurve_->maxDate(); }

inline DiscountFactor ImpliedTermStructure::discountImpl(Time t) const {
    /* t is relative to the current reference date
       and needs to be converted to the time relative
       to the reference date of the original curve */
    if (referenceTime_ != Null<Time>()) {
        return originalCurve_->discount(referenceTime_ + t) / originalCurve_->discount(referenceTime_);
    } else {
        Date ref = referenceDate();
        Time originalTime = t + dayCounter().yearFraction(originalCurve_->referenceDate(), ref);
        /* discount at evaluation date cannot be cached
           since the original curve could change between
           invocations of this method */
        return originalCurve_->discount(originalTime, true) / originalCurve_->discount(ref, true);
    }
}
} // namespace QuantExt


