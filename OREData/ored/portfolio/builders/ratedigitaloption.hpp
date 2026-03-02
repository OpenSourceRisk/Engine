/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/ratedigitaloption.hpp
    \brief Engine builder for European digital (cash-or-nothing) option on interest rates
    \ingroup builders

    Prices the digital as a call spread on vanilla caplet/floorlet prices:
        Digital(K) ≈ [C(K − ε/2) − C(K + ε/2)] / ε
    where C(K, σ(K)) is the vanilla call priced with the Black (or Bachelier)
    formula using market optionlet vol σ(K) from the capFloor surface.

    This is the standard model-free replication of a digital option and
    naturally incorporates the market smile.
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

// ---------------------------------------------------------------------------
//  RateDigitalCallSpreadEngine
// ---------------------------------------------------------------------------
//! Prices a CashOrNothingPayoff via call-spread replication on market vols.
/*! For a digital call paying P if L > K:
        PV = P × [C(K − ε/2, σ−) − C(K + ε/2, σ+)] / ε
    For a digital put paying P if L < K:
        PV = P × [P(K + ε/2, σ+) − P(K − ε/2, σ−)] / ε

    The vanilla prices C / P are computed with Black (shifted-lognormal) or
    Bachelier (normal) formula depending on the optionlet surface type.

    \ingroup engines
*/
class RateDigitalCallSpreadEngine
    : public GenericEngine<VanillaOption::arguments, VanillaOption::results> {
public:
    /*! \param forward     Forward rate F at the fixing date
        \param dfPayment   Discount factor to the payment date
        \param ovs         Optionlet (capFloor) volatility surface
        \param fixingDate  Fixing / observation date
        \param eps         Width of the call-spread (in rate terms)
    */
    RateDigitalCallSpreadEngine(Real forward,
                                DiscountFactor dfPayment,
                                Handle<OptionletVolatilityStructure> ovs,
                                Date fixingDate,
                                Real eps = 1.0e-4)
        : forward_(forward), df_(dfPayment), ovs_(std::move(ovs)),
          fixingDate_(fixingDate), eps_(eps) {
        registerWith(ovs_);
    }

    void calculate() const override {
        // Extract payoff information
        auto payoff = ext::dynamic_pointer_cast<CashOrNothingPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "RateDigitalCallSpreadEngine requires a CashOrNothingPayoff");

        const Real K = payoff->strike();
        const Real cash = payoff->cashPayoff();
        const Option::Type type = payoff->optionType();

        const Real K_lo = K - eps_ / 2.0;
        const Real K_hi = K + eps_ / 2.0;

        Real ttExpiry = ovs_->timeFromReference(fixingDate_);
        QL_REQUIRE(ttExpiry > 0.0, "RateDigitalCallSpreadEngine: fixing date "
                                       << fixingDate_ << " is not in the future");

        Real price;
        if (ovs_->volatilityType() == Normal) {
            // Bachelier (normal) formula
            Real volLo = ovs_->volatility(fixingDate_, K_lo);
            Real volHi = ovs_->volatility(fixingDate_, K_hi);
            Real stdLo = volLo * std::sqrt(ttExpiry);
            Real stdHi = volHi * std::sqrt(ttExpiry);

            if (type == Option::Call) {
                Real cLo = bachelierBlackFormula(Option::Call, K_lo, forward_, stdLo, df_);
                Real cHi = bachelierBlackFormula(Option::Call, K_hi, forward_, stdHi, df_);
                price = cash * (cLo - cHi) / eps_;
            } else {
                Real pLo = bachelierBlackFormula(Option::Put, K_lo, forward_, stdLo, df_);
                Real pHi = bachelierBlackFormula(Option::Put, K_hi, forward_, stdHi, df_);
                price = cash * (pHi - pLo) / eps_;
            }
        } else {
            // Black (shifted-lognormal) formula
            Real displacement = ovs_->displacement();
            Real volLo = ovs_->volatility(fixingDate_, K_lo);
            Real volHi = ovs_->volatility(fixingDate_, K_hi);
            Real stdLo = volLo * std::sqrt(ttExpiry);
            Real stdHi = volHi * std::sqrt(ttExpiry);

            if (type == Option::Call) {
                Real cLo = blackFormula(Option::Call, K_lo, forward_, stdLo, df_, displacement);
                Real cHi = blackFormula(Option::Call, K_hi, forward_, stdHi, df_, displacement);
                price = cash * (cLo - cHi) / eps_;
            } else {
                Real pLo = blackFormula(Option::Put, K_lo, forward_, stdLo, df_, displacement);
                Real pHi = blackFormula(Option::Put, K_hi, forward_, stdHi, df_, displacement);
                price = cash * (pHi - pLo) / eps_;
            }
        }

        results_.value = price;
    }

private:
    Real forward_;
    DiscountFactor df_;
    Handle<OptionletVolatilityStructure> ovs_;
    Date fixingDate_;
    Real eps_;
};

// ---------------------------------------------------------------------------
//  RateDigitalOptionEngineBuilder
// ---------------------------------------------------------------------------
//! Engine Builder for European rate digital options (call-spread replication)
/*! Retrieves the optionlet (capFloor) vol surface, forward rate and discount
    factor, then constructs a RateDigitalCallSpreadEngine.

    Engine parameters:
    - CallSpreadEps (optional, default 1e-4): width of the call spread in rate
      terms.  Smaller values approach the analytical digital; larger values
      smooth the replication.

    \ingroup builders
*/
class RateDigitalOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<std::string, const std::string&, QuantLib::Real,
                                                    const QuantLib::Date&, const QuantLib::Date&> {
public:
    RateDigitalOptionEngineBuilder()
        : CachingEngineBuilder("Black", "CallSpreadEngine", {"RateDigitalOption"}) {}

protected:
    std::string keyImpl(const std::string& index, QuantLib::Real strike, const QuantLib::Date& fixingDate,
                        const QuantLib::Date& paymentDate) override {
        return index + "/" + std::to_string(strike) + "/" + ore::data::to_string(fixingDate) + "/" +
               ore::data::to_string(paymentDate);
    }

    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    engineImpl(const std::string& index, QuantLib::Real strike, const QuantLib::Date& fixingDate,
               const QuantLib::Date& paymentDate) override {

        auto config = configuration(MarketContext::pricing);

        Handle<IborIndex> hIndex = market_->iborIndex(index, config);
        QL_REQUIRE(!hIndex.empty(), "RateDigitalOptionEngineBuilder: could not find index " << index);
        ext::shared_ptr<IborIndex> iborIndex = hIndex.currentLink();

        std::string ccy = iborIndex->currency().code();
        Handle<YieldTermStructure> discountCurve = market_->discountCurve(ccy, config);

        Real forward = iborIndex->forecastFixing(fixingDate);

        DiscountFactor dfPayment = discountCurve->discount(paymentDate);

        Handle<OptionletVolatilityStructure> ovs = market_->capFloorVol(index, config);
        QL_REQUIRE(!ovs.empty(), "RateDigitalOptionEngineBuilder: no capFloor vol for " << index);

        // Call-spread eps (configurable, default 1e-4 = 1 bp)
        Real eps = 1.0e-4;
        if (engineParameter("CallSpreadEps", {}, false, "") != "")
            eps = parseReal(engineParameter("CallSpreadEps"));

        return ext::make_shared<RateDigitalCallSpreadEngine>(
            forward, dfPayment, ovs, fixingDate, eps);
    }
};

} // namespace data
} // namespace ore
