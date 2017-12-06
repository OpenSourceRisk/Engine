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

/*! \file qle/instruments/deposit.hpp
    \brief deposit instrument
    \ingroup instruments
*/

#ifndef quantext_deposit_hpp
#define quantext_deposit_hpp

#include <ql/cashflow.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/instrument.hpp>
#include <ql/quote.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Deposit Instrument

/*! This class holds the term sheet data for a Deposit Instrument.

        \ingroup instruments
*/
class Deposit : public Instrument {
public:
    class arguments;
    class results;
    class engine;

    Deposit(const Real nominal, const Rate rate, const Period& tenor, const Natural fixingDays,
            const Calendar& calendar, const BusinessDayConvention convention, const bool endOfMonth,
            const DayCounter& dayCounter, const Date& tradeDate, const bool isLong = true,
            const Period forwardStart = 0 * Days);

    //! \name Instrument interface
    //@{
    bool isExpired() const;
    void setupArguments(PricingEngine::arguments*) const;
    void fetchResults(const PricingEngine::results*) const;
    //@}
    //! \name Additional interface
    //@{
    Date fixingDate() const { return fixingDate_; }
    Date startDate() const { return startDate_; }
    Date maturityDate() const { return maturityDate_; }
    Real fairRate() const {
        calculate();
        return fairRate_;
    }
    const Leg& leg() const { return leg_; }
    //@}

private:
    //! \name Instrument interface
    //@{
    void setupExpired() const;
    //@}
    Date fixingDate_, startDate_, maturityDate_;
    boost::shared_ptr<IborIndex> index_;
    Leg leg_;
    // results
    mutable Real fairRate_;
};

//! \ingroup instruments
class Deposit::arguments : public virtual PricingEngine::arguments {
public:
    boost::shared_ptr<IborIndex> index;
    Leg leg;
    void validate() const;
};

//! \ingroup instruments
class Deposit::results : public Instrument::results {
public:
    Real fairRate;
    void reset();
};

//! \ingroup instruments
class Deposit::engine : public GenericEngine<Deposit::arguments, Deposit::results> {};
} // namespace QuantExt

#endif
