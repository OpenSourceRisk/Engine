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

#include <qle/pricingengines/discountingcurrencyswapengine.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/exchangerate.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/errors.hpp>

namespace QuantExt {

DiscountingCurrencySwapEngine::DiscountingCurrencySwapEngine(
    const std::vector<Handle<YieldTermStructure> >& discountCurves, const std::vector<Handle<Quote> >& fxQuotes,
    const std::vector<Currency>& currencies, const Currency& npvCurrency,
    boost::optional<bool> includeSettlementDateFlows, Date settlementDate, Date npvDate)
    : discountCurves_(discountCurves), fxQuotes_(fxQuotes), currencies_(currencies), npvCurrency_(npvCurrency),
      includeSettlementDateFlows_(includeSettlementDateFlows), settlementDate_(settlementDate), npvDate_(npvDate) {

    QL_REQUIRE(discountCurves_.size() == currencies_.size(), "Number of "
                                                             "currencies does not match number of discount curves.");
    QL_REQUIRE(fxQuotes_.size() == currencies_.size(), "Number of "
                                                       "currencies does not match number of FX quotes.");

    for (Size i = 0; i < discountCurves_.size(); i++) {
        registerWith(discountCurves_[i]);
        registerWith(fxQuotes_[i]);
    }
}

Handle<YieldTermStructure> DiscountingCurrencySwapEngine::fetchTS(Currency ccy) const {
    std::vector<Currency>::const_iterator i = std::find(currencies_.begin(), currencies_.end(), ccy);
    if (i == currencies_.end())
        return Handle<YieldTermStructure>();
    else
        return discountCurves_[i - currencies_.begin()];
}

Handle<Quote> DiscountingCurrencySwapEngine::fetchFX(Currency ccy) const {
    std::vector<Currency>::const_iterator i = std::find(currencies_.begin(), currencies_.end(), ccy);
    if (i == currencies_.end())
        return Handle<Quote>();
    else
        return fxQuotes_[i - currencies_.begin()];
}

void DiscountingCurrencySwapEngine::calculate() const {

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

    Handle<YieldTermStructure> npvCcyYts = fetchTS(npvCurrency_);

    // Instrument settlement date
    Date referenceDate = npvCcyYts->referenceDate();
    Date settlementDate = settlementDate_;
    if (settlementDate_ == Date()) {
        settlementDate = referenceDate;
    } else {
        QL_REQUIRE(settlementDate >= referenceDate, "Settlement date (" << settlementDate
                                                                        << ") cannot be before discount curve "
                                                                           "reference date ("
                                                                        << referenceDate << ")");
    }

    // Prepare the results containers
    Size numLegs = arguments_.legs.size();

    // - Instrument::results
    if (npvDate_ == Date()) {
        results_.valuationDate = referenceDate;
    } else {
        QL_REQUIRE(npvDate_ >= referenceDate, "NPV date (" << npvDate_
                                                           << ") cannot be before "
                                                              "discount curve reference date ("
                                                           << referenceDate << ")");
        results_.valuationDate = npvDate_;
    }
    results_.value = 0.0;
    results_.errorEstimate = Null<Real>();

    // - CurrencySwap::results
    results_.legNPV.resize(numLegs);
    results_.legBPS.resize(numLegs);
    results_.inCcyLegNPV.resize(numLegs);
    results_.inCcyLegBPS.resize(numLegs);
    results_.startDiscounts.resize(numLegs);
    results_.endDiscounts.resize(numLegs);

    bool includeRefDateFlows =
        includeSettlementDateFlows_ ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();

    results_.npvDateDiscount = npvCcyYts->discount(results_.valuationDate);

    for (Size i = 0; i < numLegs; ++i) {
        try {
            Currency ccy = arguments_.currency[i];
            Handle<YieldTermStructure> yts = fetchTS(ccy);

            QuantLib::CashFlows::npvbps(arguments_.legs[i], **yts, includeRefDateFlows, settlementDate,
                                        results_.valuationDate, results_.inCcyLegNPV[i], results_.inCcyLegBPS[i]);

            results_.inCcyLegNPV[i] *= arguments_.payer[i];
            if (results_.inCcyLegBPS[i] != Null<Real>()) {
                results_.inCcyLegBPS[i] *= arguments_.payer[i];
            }

            // Converts into base currency and adds.
            Handle<Quote> fx = fetchFX(ccy);
            results_.legNPV[i] = results_.inCcyLegNPV[i] * fx->value();
            if (results_.inCcyLegBPS[i] != Null<Real>()) {
                results_.legBPS[i] = results_.inCcyLegBPS[i] * fx->value();
            } else {
                results_.legBPS[i] = Null<Real>();
            }

            results_.value += results_.legNPV[i];

            if (!arguments_.legs[i].empty()) {
                Date d1 = CashFlows::startDate(arguments_.legs[i]);
                if (d1 >= referenceDate)
                    results_.startDiscounts[i] = yts->discount(d1);
                else
                    results_.startDiscounts[i] = Null<DiscountFactor>();

                Date d2 = CashFlows::maturityDate(arguments_.legs[i]);
                if (d2 >= referenceDate)
                    results_.endDiscounts[i] = yts->discount(d2);
                else
                    results_.endDiscounts[i] = Null<DiscountFactor>();
            } else {
                results_.startDiscounts[i] = Null<DiscountFactor>();
                results_.endDiscounts[i] = Null<DiscountFactor>();
            }

        } catch (std::exception& e) {
            QL_FAIL("leg " << i << ": " << e.what());
        }
    }
}
} // namespace QuantExt
