/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ql/event.hpp>

#include <qle/instruments/fxforward.hpp>

using namespace QuantLib;

namespace QuantExt {

    FxForward::FxForward(const Real& notional1,
        const Currency& currency1,
		const Real& notional2,
		const Currency& currency2,
		const Date& startDate,
		const Date& maturityDate,
		const bool& payCurrency1)
    : notional1_(notional1),
      currency1_(currency1),
	  notional2_(notional2),
	  currency2_(currency2),
	  startDate_(startDate),
	  maturityDate_(maturityDate),
      payCurrency1_(payCurrency1) {}

    FxForward::FxForward(const Money& nominal,
        const ExchangeRate& forwardRate,
        const Date& forwardDate,
        bool sellingNominal)
    : notional1_(nominal.value()),
      currency1_(nominal.currency()),
      startDate_(Date()),
      maturityDate_(forwardDate),
      payCurrency1_(sellingNominal) {

        QL_REQUIRE(currency1_ == forwardRate.source() ||
            currency1_ == forwardRate.target(),
            "Currency of nominal does not match either currency in the "
            "exchange rate.");

        Money otherNominal = forwardRate.exchange(nominal);
        notional2_ = otherNominal.value();
        currency2_ = otherNominal.currency();
    }

    FxForward::FxForward(const Money& nominal,
        const Quote& fxForwardQuote,
        bool sellingNominal)
    : notional1_(nominal.value()),
      currency1_(nominal.currency()),
      startDate_(Date()),
      // TODO
      //maturityDate_(fxForwardQuote.exchangeDate()),
      maturityDate_(),
      payCurrency1_(sellingNominal) {

        QL_REQUIRE(fxForwardQuote.isValid(),
            "The FX Forward quote is not valid.");

        // TODO
        //ExchangeRate forwardRate = *fxForwardQuote.exchangeRate();
        ExchangeRate forwardRate;

        QL_REQUIRE(currency1_ == forwardRate.source() ||
            currency1_ == forwardRate.target(),
            "Currency of nominal does not match either currency in the "
            "forward rate quote.");

        Money otherNominal = forwardRate.exchange(nominal);
        notional2_ = otherNominal.value();
        currency2_ = otherNominal.currency();
    }

    bool FxForward::isExpired() const {
		return detail::simple_event(maturityDate_).hasOccurred();
    }

    void FxForward::setupExpired() const {
        Instrument::setupExpired();
        npv_ = Money(0.0, Currency());
        fairForwardRate_ = ExchangeRate();
    }

    void FxForward::setupArguments(PricingEngine::arguments* args) const {
		
        FxForward::arguments* arguments = 
            dynamic_cast<FxForward::arguments*>(args);
        
        QL_REQUIRE(arguments, "wrong argument type in fxforward");
        
        arguments->notional1 = notional1_;
		arguments->currency1 = currency1_;
		arguments->notional2 = notional2_;
		arguments->currency2 = currency2_;
		arguments->startDate = startDate_;
		arguments->maturityDate = maturityDate_;
        arguments->payCurrency1 = payCurrency1_;
    }

    void FxForward::fetchResults(const PricingEngine::results* r) const {
        
        Instrument::fetchResults(r);

        const FxForward::results* results =
            dynamic_cast<const FxForward::results*>(r);
        
        QL_REQUIRE(results, "wrong result type");

        npv_ = results->npv;
        fairForwardRate_ = results->fairForwardRate;
    }

    void FxForward::arguments::validate() const {
		QL_REQUIRE(notional1 > 0.0,
            "notional1  should be positive: " << notional1);
		QL_REQUIRE(notional2 > 0.0,
            "notional2 should be positive: " << notional2);
    }

    void FxForward::results::reset() {

        Instrument::results::reset();
        
        npv = Money(0.0, Currency());
        fairForwardRate = ExchangeRate();
    }

}
