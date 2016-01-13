/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ql/cashflows/cashflows.hpp>
#include <ql/exchangerate.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <qle/pricingengines/crossccyswapengine.hpp>
//#include <qle/quotes/fxquote.hpp>

namespace QuantExt {

    CrossCcySwapEngine::CrossCcySwapEngine(const Currency& sourceCcy,
        const Handle<YieldTermStructure>& sourceDiscountCurve,
        const Currency& targetCcy,
        const Handle<YieldTermStructure>& targetDiscountCurve,
        const Currency& npvCcy,
        const Handle<Quote>& spotFX,
        boost::optional<bool> includeSettlementDateFlows, 
        const Date& settlementDate, 
        const Date& npvDate)
    : sourceCcy_(sourceCcy), 
      sourceDiscountCurve_(sourceDiscountCurve),
      targetCcy_(targetCcy),
      targetDiscountCurve_(targetDiscountCurve),
      npvCcy_(npvCcy),
      spotFX_(spotFX),
      includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDate_(settlementDate),
      npvDate_(npvDate) {
        registerWith(sourceDiscountCurve_);
        registerWith(targetDiscountCurve_);
        registerWith(spotFX_);
    }
    
    void CrossCcySwapEngine::calculate() const {

        // Initial checks.
        std::vector<Currency>::iterator it;
        it = std::find(arguments_.currencies.begin(), 
            arguments_.currencies.end(), npvCcy_);
        QL_REQUIRE(it != arguments_.currencies.end(), "NPV currency "
			"must equal one of the currencies in the instrument.");

        QL_REQUIRE(!sourceDiscountCurve_.empty() &&
			!targetDiscountCurve_.empty(),
			"Discounting term structure handle is empty.");

        QL_REQUIRE(!spotFX_.empty(), "FX spot quote handle is empty.");

		QL_REQUIRE(sourceDiscountCurve_->referenceDate() ==
			targetDiscountCurve_->referenceDate(),
			"Term structures should have the same reference date.");

        // To avoid if-else
        boost::shared_ptr<YieldTermStructure> npvCcyDiscountCurve = (npvCcy_ ==
            sourceCcy_) ? *sourceDiscountCurve_ : *targetDiscountCurve_;
        
        // Instrument settlement date
        Date referenceDate = sourceDiscountCurve_->referenceDate();
        Date settlementDate = settlementDate_;
        if (settlementDate_ == Date()) {
            settlementDate = referenceDate;
        } else {
            QL_REQUIRE(settlementDate >= referenceDate, "Settlement date (" <<
                settlementDate << ") cannot be before discount curve "
                "reference date (" << referenceDate << ")");
        }

        // Prepare the results containers
        Size numLegs = arguments_.legs.size();
        // - Instrument::Results
        if (npvDate_ == Date()) {
            results_.valuationDate = referenceDate;
        } else {
            QL_REQUIRE(npvDate_ >= referenceDate,
                "NPV date (" << npvDate_  << ") cannot be before "
                "discount curve reference date (" << referenceDate << ")");
            results_.valuationDate = npvDate_;
        }
        results_.value = 0.0;
        results_.errorEstimate = Null<Real>();
        // - Swap::Results
        results_.npvDateDiscount = npvCcyDiscountCurve->discount(
            results_.valuationDate);
        results_.legNPV.resize(numLegs);
        results_.legBPS.resize(numLegs);
        results_.startDiscounts.resize(numLegs);
        results_.endDiscounts.resize(numLegs);
        // - CrossCcySwap::Results
        results_.inCcyLegNPV.resize(numLegs);
        results_.inCcyLegBPS.resize(numLegs);
        results_.npvDateDiscounts.resize(numLegs);

        bool includeReferenceDateFlows = includeSettlementDateFlows_ ?
            *includeSettlementDateFlows_ : Settings::instance().
                includeReferenceDateEvents();
        
        // Set up the FX spot rate correctly
        Date spotDate = referenceDate;
        boost::shared_ptr<ExchangeRate> spotFX;
        // TODO: do we need this?
        /*
		boost::shared_ptr<FxQuote> fxQuote =
			boost::dynamic_pointer_cast<FxQuote>(*spotFX_);
        if (fxQuote) {
            spotDate = fxQuote->exchangeDate();
            QL_REQUIRE(spotDate >= referenceDate,
                "Spot date (" << spotDate  << ") cannot be before "
                "discount curve reference date (" << referenceDate << ")");
            spotFX = fxQuote->exchangeRate();
        } else {
            spotFX.reset(new ExchangeRate(sourceCcy_, 
                targetCcy_, spotFX_->value()));
        }
        */
        // or is this enough?
        spotFX.reset(new ExchangeRate(sourceCcy_, 
            targetCcy_, spotFX_->value()));



        for (Size legNo = 0; legNo < numLegs; legNo++) {
            try {
                // Choose the correct discount curve for the leg.
                Handle<YieldTermStructure> legDiscountCurve;
                if (arguments_.currencies[legNo] == sourceCcy_) {
                    legDiscountCurve = sourceDiscountCurve_;
                } else {
                    legDiscountCurve = targetDiscountCurve_;
                }
                results_.npvDateDiscounts[legNo] = legDiscountCurve->
                    discount(results_.valuationDate);

                // Calculate the NPV and BPS of each leg in its currency.
                CashFlows::npvbps(arguments_.legs[legNo],
                    **legDiscountCurve,
                    includeReferenceDateFlows,
                    settlementDate,
                    results_.valuationDate,
                    results_.inCcyLegNPV[legNo],
                    results_.inCcyLegBPS[legNo]);
                results_.inCcyLegNPV[legNo] *= arguments_.payer[legNo];
                results_.inCcyLegBPS[legNo] *= arguments_.payer[legNo];

                // Convert to NPV currency if necessary.
                if (arguments_.currencies[legNo] != npvCcy_) {
                    /* Discount back from valuation date, compound up to spot 
                       date, and convert at the spot rate */
                    DiscountFactor spotDiscount = legDiscountCurve->
                        discount(spotDate);
                    Real spotNPV = results_.inCcyLegNPV[legNo] * 
                        results_.npvDateDiscounts[legNo] / spotDiscount;
                    Real spotBPS = results_.inCcyLegBPS[legNo] * 
                        results_.npvDateDiscounts[legNo] / spotDiscount;
                    Money spotNPVAmt(spotNPV, arguments_.currencies[legNo]);
                    Money spotBPSAmt(spotBPS, arguments_.currencies[legNo]);
                    spotNPVAmt = spotFX->exchange(spotNPVAmt);
                    spotBPSAmt = spotFX->exchange(spotBPSAmt);
                    /* Discount back from spot date and compound up to 
                       valuation date */
                    results_.legNPV[legNo] = spotNPVAmt.value() * 
                        npvCcyDiscountCurve->discount(spotDate) / 
                        results_.npvDateDiscount;
                    results_.legBPS[legNo] = spotBPSAmt.value() * 
                        npvCcyDiscountCurve->discount(spotDate) / 
                        results_.npvDateDiscount;
                } else {
                    results_.legNPV[legNo] = results_.inCcyLegNPV[legNo];
                    results_.legBPS[legNo] = results_.inCcyLegBPS[legNo];
                }

                // Get start date and end date discount for the leg
                Date startDate = CashFlows::startDate(arguments_.legs[legNo]);
                if (startDate >= referenceDate) {
                    results_.startDiscounts[legNo] = 
                        legDiscountCurve->discount(startDate);
                } else {
                   results_.startDiscounts[legNo] = Null<DiscountFactor>();
                }

                Date maturityDate = CashFlows::maturityDate(
                    arguments_.legs[legNo]);
                if (maturityDate >= referenceDate) {
                   results_.endDiscounts[legNo] = 
                       legDiscountCurve->discount(maturityDate);
                } else {
                   results_.endDiscounts[legNo] = Null<DiscountFactor>();
                }

            } catch (std::exception &e) {
                QL_FAIL(io::ordinal(legNo + 1) << " leg: " << e.what());
            }
            results_.value += results_.legNPV[legNo];
        }
    }

}
