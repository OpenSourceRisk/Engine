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

/*! \file qle/termstructures/datedstrippedoptionlet.hpp
    \brief Stripped optionlet surface with fixed reference date
    \ingroup termstructures
*/

#pragma once

#include <qle/termstructures/datedstrippedoptionletbase.hpp>

#include <ql/indexes/iborindex.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletbase.hpp>

namespace QuantExt {
//! Stripped Optionlet Surface
/*! Class to hold a stripped optionlet surface with a fixed reference date and fixed volatilities

    \ingroup termstructures
*/
class DatedStrippedOptionlet : public DatedStrippedOptionletBase {
public:
    //! Construct from a StrippedOptionletBase object
    DatedStrippedOptionlet(const Date& referenceDate, const boost::shared_ptr<StrippedOptionletBase>& s);
    //! Construct from an explicitly provided optionlet surface
    DatedStrippedOptionlet(const Date& referenceDate, const Calendar& calendar, BusinessDayConvention bdc,
                           const vector<Date>& optionletDates, const vector<vector<Rate> >& strikes,
                           const vector<vector<Volatility> >& volatilities, const vector<Rate>& optionletAtmRates,
                           const DayCounter& dayCounter, VolatilityType type = ShiftedLognormal,
                           Real displacement = 0.0);

    //! \name DatedStrippedOptionletBase interface
    //@{
    const vector<Rate>& optionletStrikes(Size i) const;
    const vector<Volatility>& optionletVolatilities(Size i) const;

    const vector<Date>& optionletFixingDates() const;
    const vector<Time>& optionletFixingTimes() const;
    Size optionletMaturities() const;

    const vector<Rate>& atmOptionletRates() const;

    const Date& referenceDate() const;
    const Calendar& calendar() const;
    BusinessDayConvention businessDayConvention() const;
    const DayCounter& dayCounter() const;
    VolatilityType volatilityType() const;
    Real displacement() const;
    //@}

    //! \name LazyObject interface
    //@{
    void performCalculations() const {
        // Does nothing unless class becomes more dynamic e.g. take in volatilities as quote handles
    }
    //@}

private:
    void checkInputs() const;

    Date referenceDate_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    vector<Date> optionletDates_;
    Size nOptionletDates_;
    vector<Time> optionletTimes_;
    vector<vector<Rate> > optionletStrikes_;
    vector<vector<Volatility> > optionletVolatilities_;
    vector<Rate> optionletAtmRates_;
    DayCounter dayCounter_;
    VolatilityType type_;
    Real displacement_;
};
} // namespace QuantExt
