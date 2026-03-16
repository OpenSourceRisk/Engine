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

#include <qle/pricingengines/discountingcurrencyswapenginedeltagamma.hpp>
#include <qle/pricingengines/discountingswapenginedeltagamma.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/exchangerate.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/errors.hpp>

#include <boost/unordered_map.hpp>

namespace QuantExt {

DiscountingCurrencySwapEngineDeltaGamma::DiscountingCurrencySwapEngineDeltaGamma(
    const std::vector<Handle<YieldTermStructure>>& discountCurves, const std::vector<Handle<Quote>>& fxQuotes,
    const std::vector<Currency>& currencies, const Currency& npvCurrency, const std::vector<Time>& bucketTimes,
    const bool computeDelta, const bool computeGamma, const bool linearInZero, const bool applySimmExemptions)
    : discountCurves_(discountCurves), fxQuotes_(fxQuotes), currencies_(currencies), npvCurrency_(npvCurrency),
      bucketTimes_(bucketTimes), computeDelta_(computeDelta), computeGamma_(computeGamma), linearInZero_(linearInZero),
      applySimmExemptions_(applySimmExemptions) {

    QL_REQUIRE(discountCurves_.size() == currencies_.size(), "Number of "
                                                             "currencies does not match number of discount curves.");
    QL_REQUIRE(fxQuotes_.size() == currencies_.size(), "Number of "
                                                       "currencies does not match number of FX quotes.");

    for (Size i = 0; i < discountCurves_.size(); i++) {
        registerWith(discountCurves_[i]);
        registerWith(fxQuotes_[i]);
    }

    QL_REQUIRE(!bucketTimes_.empty() || (!computeDelta && !computeGamma),
               "bucket times are empty, although sensitivities have to be calculated");
}

Handle<YieldTermStructure> DiscountingCurrencySwapEngineDeltaGamma::fetchTS(Currency ccy) const {
    std::vector<Currency>::const_iterator i = std::find(currencies_.begin(), currencies_.end(), ccy);
    if (i == currencies_.end())
        return Handle<YieldTermStructure>();
    else
        return discountCurves_[i - currencies_.begin()];
}

Handle<Quote> DiscountingCurrencySwapEngineDeltaGamma::fetchFX(Currency ccy) const {
    std::vector<Currency>::const_iterator i = std::find(currencies_.begin(), currencies_.end(), ccy);
    if (i == currencies_.end())
        return Handle<Quote>();
    else
        return fxQuotes_[i - currencies_.begin()];
}

void DiscountingCurrencySwapEngineDeltaGamma::calculate() const {

    for (Size i = 0; i < arguments_.currency.size(); i++) {
        Currency ccy = arguments_.currency[i];
        Handle<YieldTermStructure> yts = fetchTS(ccy);
        QL_REQUIRE(!yts.empty(), "Discounting term structure is "
                                 "empty for "
                                     << ccy.name());
        Handle<Quote> fxQuote = fetchFX(ccy);
        QL_REQUIRE(!fxQuote.empty(), "FX quote is empty "
                                     "for "
                                         << ccy.name());
    }

    // Prepare the results containers
    Size numLegs = arguments_.legs.size();

    // - Instrument::results
    results_.value = 0.0;
    results_.errorEstimate = Null<Real>();

    // - CurrencySwap::results
    results_.legNPV.resize(numLegs);
    results_.inCcyLegNPV.resize(numLegs);

    // compute npv and raw deltas

    std::map<Currency, std::map<Date, Real>, CurrencyComparator> deltaDiscountRaw, deltaForwardRaw, gammaDiscountRaw;
    std::map<Currency, std::map<std::pair<Date, Date>, Real>, CurrencyComparator> gammaForwardRaw, gammaDscFwdRaw;
    std::map<Currency, Real, CurrencyComparator> fxLinkedForeignNpv; // foreign ccy => npv in foreign ccy
    std::map<Currency, Real, CurrencyComparator> fxSpot, fxSpotDelta;
    std::set<Currency, CurrencyComparator> currencies;
    std::map<Date, Real> empty, fxLinkedDeltaEmpty;

    for (Size i = 0; i < numLegs; ++i) {
        try {
            Currency ccy = arguments_.currency[i];

            // look for second ccy, we need this for FX Linked Coupons - the assumption is then that there are
            // exactly two currencies in the swap; if we don't find a second ccy, we do not collect contributions
            // from fx linked coupons below
            Currency ccy2;
            Real fx2 = 1.0;
            for (Size j = 0; j < numLegs; ++j) {
                if (arguments_.currency[j] != ccy) {
                    ccy2 = arguments_.currency[j];
                    fx2 = fetchFX(ccy2)->value();
                    fxSpot[ccy2] = fx2;
                }
            }

            currencies.insert(ccy);
            Handle<YieldTermStructure> yts = fetchTS(ccy);
            Real npv = 0.0, bps = 0.0, simpleCashFlowNpv = 0.0;
            Real fxLinkedForeignNpvTmp = 0.0;
            detail::NpvDeltaGammaCalculator calc(
                yts, arguments_.payer[i], npv, bps, computeDelta_, computeGamma_, false, deltaDiscountRaw[ccy],
                deltaForwardRaw[ccy], empty, gammaDiscountRaw[ccy], gammaForwardRaw[ccy], gammaDscFwdRaw[ccy], empty,
                fxLinkedForeignNpvTmp,
                applySimmExemptions_ && arguments_.isPhysicallySettled && !arguments_.isResettable, simpleCashFlowNpv);
            Leg& leg = arguments_.legs[i];
            for (Size ii = 0; ii < leg.size(); ++ii) {
                CashFlow& cf = *leg[ii];
                if (cf.date() <= yts->referenceDate()) {
                    continue;
                }
                cf.accept(calc);
            }
            Real fx = fetchFX(ccy)->value();
            fxSpotDelta[ccy] += npv;
            fxSpot[ccy] = fx;
            results_.inCcyLegNPV[i] = npv + simpleCashFlowNpv;
            results_.legNPV[i] = results_.inCcyLegNPV[i] * fx;
            results_.value += results_.legNPV[i];
            // handle contribution from FX Linked Coupon:
            // - these should be subtracted from the fx spot delta in the converted ccy,
            // - and added to the fx spot delta in the original ccy.
            if (!ccy2.empty()) {
                fxSpotDelta[ccy] -= fxLinkedForeignNpvTmp * fx2 / fx;
                fxLinkedForeignNpv[ccy2] += fxLinkedForeignNpvTmp;
            }
        } catch (std::exception& e) {
            QL_FAIL("DiscountingCurrencySwapEngineDeltaGamma, leg " << i << ": " << e.what());
        }
    } // i = 0, ... ,numlegs

    // contributions from FX Linked Coupons to fx spot delta

    for (auto const& f : fxLinkedForeignNpv) {
        fxSpotDelta[f.first] += f.second;
    }

    // set results

    results_.additionalResults["fxSpot"] = fxSpot;
    results_.additionalResults["deltaFxSpot"] = fxSpotDelta;

    // convert raw deltas to given bucketing structure

    results_.additionalResults["bucketTimes"] = bucketTimes_;

    if (computeDelta_) {
        std::map<Currency, std::vector<Real>, CurrencyComparator> deltaDiscount, deltaForward;
        for (std::map<Currency, std::map<Date, Real>, CurrencyComparator>::const_iterator i = deltaDiscountRaw.begin();
             i != deltaDiscountRaw.end(); ++i) {
            Handle<YieldTermStructure> yts = fetchTS(i->first);
            deltaDiscount[i->first] =
                detail::rebucketDeltas(bucketTimes_, i->second, yts->referenceDate(), yts->dayCounter(), linearInZero_);
        }
        results_.additionalResults["deltaDiscount"] = deltaDiscount;

        for (std::map<Currency, std::map<Date, Real>, CurrencyComparator>::const_iterator i = deltaForwardRaw.begin();
             i != deltaForwardRaw.end(); ++i) {
            Handle<YieldTermStructure> yts = fetchTS(i->first);
            deltaForward[i->first] =
                detail::rebucketDeltas(bucketTimes_, i->second, yts->referenceDate(), yts->dayCounter(), linearInZero_);
        }
        results_.additionalResults["deltaForward"] = deltaForward;
    }

    // convert raw gammas to given bucketing structure

    if (computeGamma_) {
        std::map<Currency, Matrix, CurrencyComparator> gamma;
        for (std::set<Currency>::const_iterator i = currencies.begin(); i != currencies.end(); ++i) {
            Handle<YieldTermStructure> yts = fetchTS(*i);
            Matrix tmp =
                detail::rebucketGammas(bucketTimes_, gammaDiscountRaw[*i], gammaForwardRaw[*i], gammaDscFwdRaw[*i],
                                       true, yts->referenceDate(), yts->dayCounter(), linearInZero_);
            gamma[*i] = tmp;
        }
        results_.additionalResults["gamma"] = gamma;
    }

} // calculate

} // namespace QuantExt
