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

/*! \file qle/termstructures/flatcorrelation.hpp
    \brief Term structure of flat correlations
*/

#ifndef quantext_flat_correlation_hpp
#define quantext_flat_correlation_hpp

#include <ql/math/comparison.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Flat correlation structure
/*! \ingroup correlationtermstructures */
class FlatCorrelation : public CorrelationTermStructure {
public:
    //! \name Constructors
    //@{
    FlatCorrelation(const Date& referenceDate, const Handle<Quote>& correlation, const DayCounter&);
    FlatCorrelation(const Date& referenceDate, Real correlation, const DayCounter&);
    FlatCorrelation(Natural settlementDays, const Calendar& calendar, const Handle<Quote>& correlation,
                    const DayCounter&);
    FlatCorrelation(Natural settlementDays, const Calendar& calendar, Real correlation, const DayCounter&);
    //@}
    //! \name TermStructure interface
    //@{
    Date maxDate() const override { return Date::maxDate(); }
    Time maxTime() const override { return QL_MAX_REAL; }
    //@}
    //! \name Inspectors
    //@{
    const Handle<Quote>& quote() const { return correlation_; }
    //@}

private:
    //! \name CorrelationTermStructure interface
    //@{
    Real correlationImpl(Time, Real) const override { return correlation_->value(); }
    //@}

    Handle<Quote> correlation_;
};

} // namespace QuantExt

#endif
