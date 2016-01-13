/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ql/event.hpp>

#include <qle/pricingengines/discountingfxforwardengine.hpp>
#include <iostream>

namespace QuantExt {

    DiscountingFxForwardEngine::DiscountingFxForwardEngine(
		const Currency& ccy1,
		const Handle<YieldTermStructure>& currency1Discountcurve,
		const Currency& ccy2,
		const Handle<YieldTermStructure>& currency2Discountcurve,
		const Currency& priceCurrency,
		const Handle<Quote>& spotFX,
		boost::optional<bool> includeSettlementDateFlows,
		const Date& settlementDate,
		const Date& npvDate)
	: ccy1_(ccy1),
	  currency1Discountcurve_(currency1Discountcurve),
	  ccy2_(ccy2),
	  currency2Discountcurve_(currency2Discountcurve),
	  priceCurrency_(priceCurrency),
	  spotFX_(spotFX),
      includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDate_(settlementDate),
	  npvDate_(npvDate) {
		
		registerWith(currency1Discountcurve_);
		registerWith(currency2Discountcurve_); 
		registerWith(spotFX_);
    }

    void DiscountingFxForwardEngine::calculate() const {
		// Initial checks.
		QL_REQUIRE(ccy1_ == arguments_.currency1 &&
			ccy2_ == arguments_.currency2,
			"Instrument and engine arguments must be ordered.");

		QL_REQUIRE(priceCurrency_ == arguments_.currency1 ||
			priceCurrency_ == arguments_.currency2,
			"priceCurrency must be one of ccy1 or ccy2.");

		QL_REQUIRE(!currency1Discountcurve_.empty() &&
			!currency2Discountcurve_.empty(),
			"Discounting term structure handle is empty.");

		QL_REQUIRE(currency1Discountcurve_->referenceDate() ==
			currency2Discountcurve_->referenceDate(),
			"Term structures should have the same reference date.");

        QL_REQUIRE(arguments_.maturityDate >=
			currency1Discountcurve_->referenceDate(),
			"FX forward maturity should exceed or equal the "
            "discount curve reference date.");

		// Set the spot date and spot rate
		Date referenceDate = currency1Discountcurve_->referenceDate();
		Date spotDate = referenceDate;
        boost::shared_ptr<ExchangeRate> spotFX;
		boost::shared_ptr<Quote> fxQuote =
			boost::dynamic_pointer_cast<Quote>(*spotFX_);
        // TODO
        /*
        if (fxQuote) {
            spotDate = fxQuote->exchangeDate();
            QL_REQUIRE(spotDate >= referenceDate,
                "Spot date (" << spotDate  << ") should equal or exceed "
                "discount curve reference date (" << referenceDate << ")");
            spotFX = fxQuote->exchangeRate();
        } else {
            if (ccy1_ == priceCurrency_) {
                spotFX.reset(new ExchangeRate(ccy2_,
                    priceCurrency_, spotFX_->value()));
            } else {
                spotFX.reset(new ExchangeRate(ccy1_,
                    priceCurrency_, spotFX_->value()));
            }
        }
        */

        // Set the valuation date in the results.
        if (npvDate_ == Date()) {
            results_.valuationDate = referenceDate;
        } else {
            QL_REQUIRE(npvDate_ >= referenceDate,
                "NPV date (" << npvDate_  << ") should equal or exceed "
                "discount curve reference date (" << referenceDate << ")");
            results_.valuationDate = npvDate_;
        }
        
        results_.value = 0.0;
        results_.errorEstimate = Null<Real>();
        //results_.currency = priceCurrency_;
        results_.npv = Money(0.0, priceCurrency_);
        results_.fairForwardRate = ExchangeRate();
        
        if (!detail::simple_event(arguments_.maturityDate).hasOccurred(
                settlementDate_, includeSettlementDateFlows_)) {
            
            // Leg NPVs in their own ccy.
            Real npvLegOne = arguments_.notional1 *
                currency1Discountcurve_->discount(arguments_.maturityDate);
            Real npvLegTwo = arguments_.notional2 *
                currency2Discountcurve_->discount(arguments_.maturityDate);
            
            // Value the forward.
            if (ccy1_ == priceCurrency_) {
                results_.value -= npvLegOne;
                npvLegTwo /= currency2Discountcurve_->discount(spotDate);
                Money spotValueLegTwo(npvLegTwo, ccy2_);
                spotValueLegTwo = spotFX->exchange(spotValueLegTwo);
                results_.value += spotValueLegTwo.value() *
                    currency1Discountcurve_->discount(spotDate);
                results_.value /= currency1Discountcurve_->discount(results_.valuationDate);
            } else {
                results_.value += npvLegTwo;
                npvLegOne /= currency1Discountcurve_->discount(spotDate);
                Money spotValueLegOne(npvLegOne, ccy1_);
                spotValueLegOne = spotFX->exchange(spotValueLegOne);
                results_.value -= spotValueLegOne.value() *
                    currency2Discountcurve_->discount(spotDate);
                results_.value /= currency2Discountcurve_->discount(results_.valuationDate);
            }
            if (!arguments_.payCurrency1) results_.value *= -1.0;
            
            // Fill in other results.
            results_.npv = Money(results_.value, priceCurrency_);
            
            // ... and the fair forward rate.
            Decimal fairForward = currency1Discountcurve_->discount(spotDate) /
                currency2Discountcurve_->discount(spotDate);
            fairForward *=
                currency2Discountcurve_->discount(arguments_.maturityDate) /
                currency1Discountcurve_->discount(arguments_.maturityDate);
            if (ccy1_ == spotFX->target()) {
                fairForward *= spotFX->rate();
            } else {
                fairForward = spotFX->rate() / fairForward;
            }
            results_.fairForwardRate = ExchangeRate(spotFX->source(),
                spotFX->target(), fairForward);
        }

    }

}
