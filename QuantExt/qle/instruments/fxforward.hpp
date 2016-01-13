/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/instruments/fxforward.hpp
    \brief defaultable fxforward instrument
*/

#ifndef quantext_fxforward_hpp
#define quantext_fxforward_hpp

#include <ql/currency.hpp>
#include <ql/money.hpp>
#include <ql/exchangerate.hpp>
#include <ql/instrument.hpp>
#include <ql/quote.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! <strong> FX Forward </strong>
    
    /*! This class holds the term sheet data for an FX Forward instrument.
        
        It is a defaultable FX Forward instrument, too, as it derives from 
        Defaultable, so that it provides the interface for accessing CVA and 
        default-free or risky NPV which need to be filled by an appropriate
        defaultable engine.
        
        \ingroup instruments
    */
    class FxForward : public Instrument {
      public:	
        class arguments;
        class results;
        class engine;
        //! \name Constructors
        //@{
        /*! \param notional1, currency1
                   There are notional1 units of currency1.
            \param notional2, currency2
                   There are notional2 units of currency2.
            \param startDate
                   Not used by the discounting FX forward engine.
            \param maturityDate
                   Date on which currency amounts are exchanged.
            \param payCurrency1
                   Pay notional1 if true, otherwise pay notional2.
        */
        FxForward(const Real& notional1,
            const Currency& currency1,
            const Real& notional2,
            const Currency& currency2,
            const Date& startDate, 
            const Date& maturityDate,
			const bool& payCurrency1);
        
        /*! \param nominal
                   FX forward nominal amount.
            \param forwardRate, forwardDate
                   FX forward rate and date for the exchange.
            \param sellingNominal
                   Sell (pay) nominal if true, otherwise buy (receive) nominal.
        */
        FxForward(const Money& nominal,
            const ExchangeRate& forwardRate,
            const Date& forwardDate,
            bool sellingNominal);

        /*! \param nominal
                   FX forward nominal amount.
            \param fxForwardQuote
                   FX forward quote giving the rate and exchange date.
            \param sellingNominal
                   Sell (pay) nominal if true, otherwise buy (receive) nominal.
        */
        FxForward(const Money& nominal,
            const Quote& fxForwardQuote, // TODO: Quote or shared_ptr?
            bool sellingNominal);
        //@}
        
        //! \name Results
        //@{
        //! Return NPV in base currency.
        const Money& npvPriceCcy() const {
            calculate();
            return npv_;
        }
        //! Return the fair FX forward rate.
        const ExchangeRate& fairForwardRate() const {
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
        Real currency1Notional() { return notional1_; }
        Real currency2Notional() { return notional2_; }
        Currency currency1() { return currency1_; }
        Currency currency2() { return currency2_; }
        Date startDate() const { return startDate_; }
        Date maturityDate() { return maturityDate_; }
        bool payCurrency1() { return payCurrency1_; }
        boost::shared_ptr<PricingEngine> engine() { return engine_; }
        //@}
    
      private:
        //! \name Instrument interface
        //@{
        void setupExpired() const;
        //@}

		Real notional1_;
		Currency currency1_;
		Real notional2_;
		Currency currency2_;
		Date startDate_;
		Date maturityDate_;
		bool payCurrency1_;

        // results
        mutable Money npv_;
        mutable ExchangeRate fairForwardRate_;
    };

    class FxForward::arguments : public virtual PricingEngine::arguments {
      public:
		Real notional1;
		Currency currency1;
		Real notional2;
		Currency currency2;
		Date startDate;
		Date maturityDate;
		bool payCurrency1;
        void validate() const;
    };

    class FxForward::results : public Instrument::results {
      public:
        Money npv;
        ExchangeRate fairForwardRate;
        void reset();
    };

    class FxForward::engine : public 
        GenericEngine<FxForward::arguments, FxForward::results> {};
}

#endif
