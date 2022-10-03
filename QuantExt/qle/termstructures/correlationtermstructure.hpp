/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/correlationtermstructure.hpp
    \brief Term structure of correlations
*/

#ifndef quantext_correlation_term_structure_hpp
#define quantext_correlation_term_structure_hpp

#include <ql/math/comparison.hpp>
#include <ql/quote.hpp>
#include <ql/termstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Correlation term structure
/*! This abstract class defines the interface of concrete
    correlation term structures which will be derived from this one.

    \ingroup termstructures
*/
class CorrelationTermStructure : public TermStructure {
public:
    //! \name Constructors
    //@{
    CorrelationTermStructure(const DayCounter& dc = DayCounter());
    CorrelationTermStructure(const Date& referenceDate, const Calendar& cal = Calendar(),
                             const DayCounter& dc = DayCounter());
    CorrelationTermStructure(Natural settlementDays, const Calendar& cal, const DayCounter& dc = DayCounter());
    //@}

    //! \name Correlations
    //@{
    Real correlation(Time t, Real strike = Null<Real>(), bool extrapolate = false) const;
    Real correlation(const Date& d, Real strike = Null<Real>(), bool extrapolate = false) const;
    //@}

    //! The minimum time for which the curve can return values
    virtual Time minTime() const;

protected:
    /*! \name Calculations
        This method must be implemented in derived classes to
        perform the actual calculations.
    */
    //@{
    //! Correlation calculation
    virtual Real correlationImpl(Time t, Real strike) const = 0;
    //@}

    //! Extra time range check for minimum time, then calls TermStructure::checkRange
    virtual void checkRange(Time t, Real strike, bool extrapolate) const;
};

//! Wrapper class that inverts the correlation
class NegativeCorrelationTermStructure : public CorrelationTermStructure {
public:
    NegativeCorrelationTermStructure(const Handle<CorrelationTermStructure>& c);
    Date maxDate() const override { return c_->maxDate(); }
    const Date& referenceDate() const override { return c_->referenceDate(); }
    Calendar calendar() const override { return c_->calendar(); }
    Natural settlementDays() const override { return c_->settlementDays(); }

private:
    virtual Real correlationImpl(Time t, Real strike) const override;
    Handle<CorrelationTermStructure> c_;
};

//! Wrapper class that extracts a value at a given time from the term structure
class CorrelationValue : public Observer, public Quote {
public:
    CorrelationValue(const Handle<CorrelationTermStructure>& correlation, const Time t,
                     const Real strike = Null<Real>());
    Real value() const override;
    bool isValid() const override;
    void update() override;

private:
    Handle<CorrelationTermStructure> correlation_;
    const Time t_;
    const Real strike_;
};

} // namespace QuantExt

#endif
