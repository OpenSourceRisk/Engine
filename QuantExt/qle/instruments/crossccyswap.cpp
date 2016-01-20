/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

#include <qle/instruments/crossccyswap.hpp>

namespace QuantExt {

    CrossCcySwap::CrossCcySwap(const Leg& firstLeg,
        const Currency& firstLegCcy,
        const Leg& secondLeg,
        const Currency& secondLegCcy)
    : Swap(firstLeg, secondLeg) {
        currencies_.resize(2);
        currencies_[0] = firstLegCcy;
        currencies_[1] = secondLegCcy;
    }

    CrossCcySwap::CrossCcySwap(const std::vector<Leg>& legs,
        const std::vector<bool>& payer,
        const std::vector<Currency>& currencies) 
    : Swap(legs, payer), currencies_(currencies) {
        QL_REQUIRE(payer.size() == currencies_.size(), "Size mismatch "
            "between payer (" << payer.size() << ") and currencies (" << 
            currencies_.size() << ")");
    }

    CrossCcySwap::CrossCcySwap(Size legs)
    : Swap(legs),
      currencies_(legs),
      inCcyLegNPV_(legs, 0.0),
      inCcyLegBPS_(legs, 0.0),
      npvDateDiscounts_(legs, 0.0) {}

    void CrossCcySwap::setupArguments(PricingEngine::arguments* args) const {
        
        Swap::setupArguments(args);

        CrossCcySwap::arguments* arguments = dynamic_cast
            <CrossCcySwap::arguments*>(args);
        QL_REQUIRE(arguments, "The arguments are not of type "
            "cross currency swap");

        arguments->currencies = currencies_;
    }

    void CrossCcySwap::fetchResults(const PricingEngine::results* r) const {
        
        Swap::fetchResults(r);

        const CrossCcySwap::results* results = dynamic_cast
            <const CrossCcySwap::results*>(r);
        QL_REQUIRE(results, "The results are not of type "
            "cross currency swap");

        if (!results->inCcyLegNPV.empty()) {
            QL_REQUIRE(results->inCcyLegNPV.size() == inCcyLegNPV_.size(),
                "Wrong number of in currency leg NPVs returned by engine");
            inCcyLegNPV_ = results->inCcyLegNPV;
        } else {
            std::fill(inCcyLegNPV_.begin(), inCcyLegNPV_.end(), Null<Real>());
        }

        if (!results->inCcyLegBPS.empty()) {
            QL_REQUIRE(results->inCcyLegBPS.size() == inCcyLegBPS_.size(),
                "Wrong number of in currency leg BPSs returned by engine");
            legBPS_ = results->legBPS;
        } else {
            std::fill(inCcyLegBPS_.begin(), inCcyLegBPS_.end(), Null<Real>());
        }

        if (!results->npvDateDiscounts.empty()) {
            QL_REQUIRE(results->npvDateDiscounts.size() == npvDateDiscounts_.
                size(), "Wrong number of npv date discounts returned by engine");
            npvDateDiscounts_ = results->npvDateDiscounts;
        } else {
            std::fill(npvDateDiscounts_.begin(), npvDateDiscounts_.end(),
                Null<DiscountFactor>());
        }
    }

    void CrossCcySwap::setupExpired() const {
        Swap::setupExpired();
        std::fill(inCcyLegBPS_.begin(), inCcyLegBPS_.end(), 0.0);
        std::fill(inCcyLegNPV_.begin(), inCcyLegNPV_.end(), 0.0);
        std::fill(npvDateDiscounts_.begin(), npvDateDiscounts_.end(), 0.0);
    }

    void CrossCcySwap::arguments::validate() const {
        Swap::arguments::validate();
        QL_REQUIRE(legs.size() == currencies.size(),
            "Number of legs is not equal to number of currencies");
    }

    void CrossCcySwap::results::reset() {
        Swap::results::reset();
        inCcyLegNPV.clear();
        inCcyLegBPS.clear();
        npvDateDiscounts.clear();
    }

}
