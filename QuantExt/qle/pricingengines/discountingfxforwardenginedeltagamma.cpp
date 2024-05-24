/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/discountingfxforwardenginedeltagamma.hpp>
#include <qle/pricingengines/discountingswapenginedeltagamma.hpp>

#include <qle/currencies/currencycomparator.hpp>

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/event.hpp>

namespace QuantExt {
using namespace QuantLib;

DiscountingFxForwardEngineDeltaGamma::DiscountingFxForwardEngineDeltaGamma(
    const Currency& domCcy, const Handle<YieldTermStructure>& domCurve, const Currency& forCcy,
    const Handle<YieldTermStructure>& forCurve, const Handle<Quote>& spotFx, const std::vector<Time>& bucketTimes,
    const bool computeDelta, const bool computeGamma, const bool linearInZero,
    boost::optional<bool> includeSettlementDateFlows, const Date& settlementDate, const Date& npvDate,
    const bool applySimmExemptions)
    : domCcy_(domCcy), forCcy_(forCcy), domCurve_(domCurve), forCurve_(forCurve), spotFx_(spotFx),
      bucketTimes_(bucketTimes), computeDelta_(computeDelta), computeGamma_(computeGamma), linearInZero_(linearInZero),
      includeSettlementDateFlows_(includeSettlementDateFlows), settlementDate_(settlementDate), npvDate_(npvDate),
      applySimmExemptions_(applySimmExemptions) {
    registerWith(domCurve_);
    registerWith(forCurve_);
    registerWith(spotFx_);
}

void DiscountingFxForwardEngineDeltaGamma::calculate() const {

    QL_REQUIRE(!domCurve_.empty(), "domestic curve is empty");
    QL_REQUIRE(!forCurve_.empty(), "foreign curve is empty");
    QL_REQUIRE(!spotFx_.empty(), "FX quote is empty");

    // we build this engine similar to the currency swap engine

    std::vector<Leg> legs;
    legs.push_back(Leg(1, QuantLib::ext::make_shared<SimpleCashFlow>(arguments_.nominal1, arguments_.maturityDate)));
    legs.push_back(Leg(1, QuantLib::ext::make_shared<SimpleCashFlow>(arguments_.nominal2, arguments_.maturityDate)));

    std::vector<Currency> currencies = {arguments_.currency1, arguments_.currency2};

    std::vector<Real> payer = {arguments_.payCurrency1 ? -1.0 : 1.0, arguments_.payCurrency1 ? 1.0 : -1.0};

    results_.value = 0.0;

    std::map<Currency, std::map<Date, Real>, CurrencyComparator> deltaDiscountRaw, gammaDiscountRaw;
    std::map<Currency, Real, CurrencyComparator> fxSpot, fxSpotDelta;
    std::map<Date, Real> empty;
    std::map<std::pair<Date, Date>, Real> empty2;
    Real empty3 = 0.0;

    Real domFlow = 0, forFlow = 0, domNPV = 0, forNPV = 0;
    for (Size i = 0; i < 2; ++i) {
        try {
            Handle<YieldTermStructure> yts;
            if (currencies[i] == domCcy_)
                yts = domCurve_;
            else if (currencies[i] == forCcy_)
                yts = forCurve_;
            else {
                QL_FAIL("ccy " << currencies[i] << " not handled.");
            }
            Real npv = 0.0, bps = 0.0, simpleCashFlowNpv = 0.0;
            detail::NpvDeltaGammaCalculator calc(
                yts, payer[i], npv, bps, computeDelta_, computeGamma_, false, deltaDiscountRaw[currencies[i]], empty,
                empty, gammaDiscountRaw[currencies[i]], empty2, empty2, empty, empty3,
                applySimmExemptions_ && arguments_.isPhysicallySettled, simpleCashFlowNpv);
            for (Size ii = 0; ii < legs[i].size(); ++ii) {
                CashFlow& cf = *legs[i][ii];
                if (cf.date() <= yts->referenceDate()) {
                    continue;
                }
                cf.accept(calc);
		        if (currencies[i] == domCcy_)
		            domFlow = cf.amount();
		        if (currencies[i] == forCcy_)
		            forFlow = cf.amount();
            }
            if (currencies[i] == domCcy_) {
                domNPV = npv;
                results_.additionalResults["npvDom"] = npv;
                results_.value += npv + simpleCashFlowNpv;
            } else {
                forNPV = npv;
                results_.additionalResults["npvFor"] = npv;
                results_.value += (npv + simpleCashFlowNpv) * spotFx_->value();
                fxSpot[currencies[i]] = spotFx_->value();
                fxSpotDelta[currencies[i]] += npv;
            }
        } catch (const std::exception& e) {
            QL_FAIL("DiscountingFxForwardEngineDeltaGamma, leg " << i << ": " << e.what());
        }
    } // for i = 0, 1 (legs)

    results_.additionalResults["fxSpot"] = fxSpot;
    results_.additionalResults["deltaFxSpot"] = fxSpotDelta;

    Date npvDate = npvDate_;
    if (npvDate == Null<Date>()) {
        npvDate = domCurve_->referenceDate();
    }
    Date settlementDate = settlementDate_;
    if (settlementDate == Null<Date>()) {
        settlementDate = npvDate;
    }

    // convert raw deltas to given bucketing structure
    if (computeDelta_) {
        std::map<Currency, std::vector<Real>, CurrencyComparator> deltaDiscount;
        for (std::map<Currency, std::map<Date, Real>, CurrencyComparator>::const_iterator i = deltaDiscountRaw.begin();
             i != deltaDiscountRaw.end(); ++i) {
            Handle<YieldTermStructure> yts = i->first == domCcy_ ? domCurve_ : forCurve_;
            deltaDiscount[i->first] =
                detail::rebucketDeltas(bucketTimes_, i->second, yts->referenceDate(), yts->dayCounter(), linearInZero_);
        }
        results_.additionalResults["deltaDiscount"] = deltaDiscount;
        results_.additionalResults["bucketTimes"] = bucketTimes_;
    }

    // convert raw gammas to given bucketing structure
    if (computeGamma_) {
        std::map<Currency, Matrix, CurrencyComparator> gamma;
        for (std::vector<Currency>::const_iterator i = currencies.begin(); i != currencies.end(); ++i) {
            Handle<YieldTermStructure> yts = *i == domCcy_ ? domCurve_ : forCurve_;
            Matrix tmp = detail::rebucketGammas(bucketTimes_, gammaDiscountRaw[*i], empty2, empty2, false,
                                                yts->referenceDate(), yts->dayCounter(), linearInZero_);
            gamma[*i] = tmp;
        }
        results_.additionalResults["gamma"] = gamma;
        results_.additionalResults["bucketTimes"] = bucketTimes_;
    }

    // Align notional with ISDA AANA/GRID guidance as of November 2020 for deliverable forwards
    if (fabs(domNPV) > fabs(forNPV) * spotFx_->value()) {
        results_.additionalResults["currentNotional"] = domFlow;
     	results_.additionalResults["notionalCurrency"] = domCcy_.code();
    } else {
        results_.additionalResults["currentNotional"] = forFlow;
	    results_.additionalResults["notionalCurrency"] = forCcy_.code();
     }
    
} // calculate

} // namespace QuantExt
