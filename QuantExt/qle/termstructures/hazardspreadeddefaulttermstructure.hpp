/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/termstructures/hazardspreadeddefaulttermstructure.hpp
    \brief adds a constant hazard rate spread to a default term structure
*/

#ifndef quantext_hazard_spreaded__defaulttermstructure_hpp
#define quantext_hazard_spreaded__defaulttermstructure_hpp

#include <ql/termstructures/credit/hazardratestructure.hpp>
#include <ql/quote.hpp>

using namespace QuantLib;

namespace QuantExt {

//FIXME: why do we need this?
class HazardSpreadedDefaultTermStructure : public HazardRateStructure {
  public:
    //! \name Constructors
    //@{
    HazardSpreadedDefaultTermStructure(
        const Handle<DefaultProbabilityTermStructure> &source,
        const Handle<Quote> &spread);
    //@}
    //! \name TermStructure interface
    //@{
    virtual DayCounter dayCounter() const;
    virtual Date maxDate() const;
    virtual Time maxTime() const;
    virtual const Date &referenceDate() const;
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

inline Probability
HazardSpreadedDefaultTermStructure::survivalProbabilityImpl(Time t) const {
    return source_->survivalProbability(t) * std::exp(-spread_->value() * t);
}

inline DayCounter HazardSpreadedDefaultTermStructure::dayCounter() const {
    return source_->dayCounter();
}

inline Date HazardSpreadedDefaultTermStructure::maxDate() const {
    return source_->maxDate();
}

inline Time HazardSpreadedDefaultTermStructure::maxTime() const {
    return source_->maxTime();
}

inline const Date &HazardSpreadedDefaultTermStructure::referenceDate() const {
    return source_->referenceDate();
}

inline Calendar HazardSpreadedDefaultTermStructure::calendar() const {
    return source_->calendar();
}

inline Natural HazardSpreadedDefaultTermStructure::settlementDays() const {
    return source_->settlementDays();
}

} // namespace QuantExt

#endif
