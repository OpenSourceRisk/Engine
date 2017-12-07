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

/*! \file qle/instruments/fxforward.hpp
    \brief defaultable fxforward instrument

        \ingroup instruments
*/

#ifndef quantext_fxforward_hpp
#define quantext_fxforward_hpp

#include <ql/currency.hpp>
#include <ql/exchangerate.hpp>
#include <ql/instrument.hpp>
#include <ql/money.hpp>
#include <ql/quote.hpp>

using namespace QuantLib;

namespace QuantExt {

//! <strong> FX Forward </strong>

/*! This class holds the term sheet data for an FX Forward instrument.

        \ingroup instruments
*/
class FxForward : public Instrument {
public:
    class arguments;
    class results;
    class engine;
    //! \name Constructors
    //@{
    /*! \param nominal1, currency1
               There are nominal1 units of currency1.
        \param nominal2, currency2
               There are nominal2 units of currency2.
        \param maturityDate
               Date on which currency amounts are exchanged.
        \param payCurrency1
               Pay nominal1 if true, otherwise pay nominal2.
    */
    FxForward(const Real& nominal1, const Currency& currency1, const Real& nominal2, const Currency& currency2,
              const Date& maturityDate, const bool& payCurrency1);

    /*! \param nominal1
               FX forward nominal amount (domestic currency)
        \param forwardRate
               FX rate of the exchange
        \param forwardDate
               Date of the exchange.
        \param sellingNominal
               Sell (pay) nominal1 if true, otherwise buy (receive) nominal.
    */
    FxForward(const Money& nominal1, const ExchangeRate& forwardRate, const Date& forwardDate, bool sellingNominal);

    /*! \param nominal1
               FX forward nominal amount 1 (domestic currency)
        \param fxForwardQuote
               FX forward quote giving the rate in domestic units
               per one foreign unit
        \param currency2
               currency for nominal2 (foreign currency)
        \param maturityDate
               FX Forward maturity date
        \param sellingNominal
               Sell (pay) nominal1 if true, otherwise buy (receive) nominal1.
    */
    FxForward(const Money& nominal1, const Handle<Quote>& fxForwardQuote, const Currency& currency2,
              const Date& maturityDate, bool sellingNominal);
    //@}

    //! \name Results
    //@{
    //! Return NPV as money (the price currency is set in the pricing engine)
    const Money& npvMoney() const {
        calculate();
        return npv_;
    }
    //! Return the fair FX forward rate.
    const ExchangeRate& fairForwardRate() const {
        calculate();
        return fairForwardRate_;
    }
    //@}

    //! \name Instrument interface
    //@{
    bool isExpired() const;
    void setupArguments(PricingEngine::arguments*) const;
    void fetchResults(const PricingEngine::results*) const;
    //@}

    //! \name Additional interface
    //@{
    Real currency1Nominal() const { return nominal1_; }
    Real currency2Nominal() const { return nominal2_; }
    Currency currency1() const { return currency1_; }
    Currency currency2() const { return currency2_; }
    Date maturityDate() const { return maturityDate_; }
    bool payCurrency1() const { return payCurrency1_; }
    //@}

private:
    //! \name Instrument interface
    //@{
    void setupExpired() const;
    //@}

    Real nominal1_;
    Currency currency1_;
    Real nominal2_;
    Currency currency2_;
    Date maturityDate_;
    bool payCurrency1_;

    // results
    mutable Money npv_;
    mutable ExchangeRate fairForwardRate_;
};

//! \ingroup instruments
class FxForward::arguments : public virtual PricingEngine::arguments {
public:
    Real nominal1;
    Currency currency1;
    Real nominal2;
    Currency currency2;
    Date maturityDate;
    bool payCurrency1;
    void validate() const;
};

//! \ingroup instruments
class FxForward::results : public Instrument::results {
public:
    Money npv;
    ExchangeRate fairForwardRate;
    void reset();
};

//! \ingroup instruments
class FxForward::engine : public GenericEngine<FxForward::arguments, FxForward::results> {};
} // namespace QuantExt

#endif
