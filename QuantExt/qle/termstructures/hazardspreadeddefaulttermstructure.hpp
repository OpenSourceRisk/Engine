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

/*! \file qle/termstructures/hazardspreadeddefaulttermstructure.hpp
    \brief adds a constant hazard rate spread to a default term structure
    \ingroup termstructures
*/

#ifndef quantext_hazard_spreaded__defaulttermstructure_hpp
#define quantext_hazard_spreaded__defaulttermstructure_hpp

#include <ql/quote.hpp>
#include <ql/termstructures/credit/hazardratestructure.hpp>

using namespace QuantLib;

namespace QuantExt {

// FIXME: why do we need this?
//! HazardS preaded Default Term Structure
/*! \ingroup termstructures
 */
class HazardSpreadedDefaultTermStructure : public HazardRateStructure {
public:
    //! \name Constructors
    //@{
    HazardSpreadedDefaultTermStructure(const Handle<DefaultProbabilityTermStructure>& source,
                                       const Handle<Quote>& spread);
    //@}
    //! \name TermStructure interface
    //@{
    virtual DayCounter dayCounter() const;
    virtual Date maxDate() const;
    virtual Time maxTime() const;
    virtual const Date& referenceDate() const;
    virtual Calendar calendar() const;
    virtual Natural settlementDays() const;
    //@}
private:
    //! \name HazardRateStructure interface
    //@{
    Rate hazardRateImpl(Time) const;
    //@}

    //! \name DefaultProbabilityTermStructure interface
    //@{
    Probability survivalProbabilityImpl(Time) const;
    //@}

    Handle<DefaultProbabilityTermStructure> source_;
    Handle<Quote> spread_;
};

// inline definitions

inline Rate HazardSpreadedDefaultTermStructure::hazardRateImpl(Time t) const {
    return source_->hazardRate(t) + spread_->value();
}

inline Probability HazardSpreadedDefaultTermStructure::survivalProbabilityImpl(Time t) const {
    return source_->survivalProbability(t) * std::exp(-spread_->value() * t);
}

inline DayCounter HazardSpreadedDefaultTermStructure::dayCounter() const { return source_->dayCounter(); }

inline Date HazardSpreadedDefaultTermStructure::maxDate() const { return source_->maxDate(); }

inline Time HazardSpreadedDefaultTermStructure::maxTime() const { return source_->maxTime(); }

inline const Date& HazardSpreadedDefaultTermStructure::referenceDate() const { return source_->referenceDate(); }

inline Calendar HazardSpreadedDefaultTermStructure::calendar() const { return source_->calendar(); }

inline Natural HazardSpreadedDefaultTermStructure::settlementDays() const { return source_->settlementDays(); }

} // namespace QuantExt

#endif
