/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/payment.hpp
    \brief payment instrument
    \ingroup instruments
*/

#ifndef quantext_payment_hpp
#define quantext_payment_hpp

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/currency.hpp>
#include <ql/instrument.hpp>
#include <ql/quote.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Payment Instrument

/*! This class holds the data for single payment.

        \ingroup instruments
*/
class Payment : public Instrument {
public:
    class arguments;
    class results;
    class engine;

    Payment(const Real amount, const Currency& currency, const Date& date);

    //! \name Instrument interface
    //@{
    bool isExpired() const;
    void setupArguments(PricingEngine::arguments*) const;
    void fetchResults(const PricingEngine::results*) const;
    //@}
    //! \name Additional interface
    //@{
    Currency currency() const { return currency_; }
    boost::shared_ptr<SimpleCashFlow> cashFlow() const { return cashflow_; }
    //@}

private:
    //! \name Instrument interface
    //@{
    void setupExpired() const;
    //@}
    Currency currency_;
    boost::shared_ptr<SimpleCashFlow> cashflow_;
};

class Payment::arguments : public virtual PricingEngine::arguments {
public:
    Currency currency;
    boost::shared_ptr<SimpleCashFlow> cashflow;
    void validate() const;
};

//! \ingroup instruments
class Payment::results : public Instrument::results {
public:
    void reset();
};

//! \ingroup instruments
class Payment::engine : public GenericEngine<Payment::arguments, Payment::results> {};
} // namespace QuantExt

#endif
