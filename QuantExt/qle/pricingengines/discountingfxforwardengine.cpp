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

#include <ql/event.hpp>

#include <qle/pricingengines/discountingfxforwardengine.hpp>

namespace QuantExt {

DiscountingFxForwardEngine::DiscountingFxForwardEngine(
    const Currency& ccy1, const Handle<YieldTermStructure>& currency1Discountcurve, const Currency& ccy2,
    const Handle<YieldTermStructure>& currency2Discountcurve, const Handle<Quote>& spotFX,
    boost::optional<bool> includeSettlementDateFlows, const Date& settlementDate, const Date& npvDate)
    : ccy1_(ccy1), currency1Discountcurve_(currency1Discountcurve), ccy2_(ccy2),
      currency2Discountcurve_(currency2Discountcurve), spotFX_(spotFX),
      includeSettlementDateFlows_(includeSettlementDateFlows), settlementDate_(settlementDate), npvDate_(npvDate) {
    registerWith(currency1Discountcurve_);
    registerWith(currency2Discountcurve_);
    registerWith(spotFX_);
}

void DiscountingFxForwardEngine::calculate() const {

    Date npvDate = npvDate_;
    if (npvDate == Null<Date>()) {
        npvDate = currency1Discountcurve_->referenceDate();
    }
    Date settlementDate = settlementDate_;
    if (settlementDate == Null<Date>()) {
        settlementDate = npvDate;
    }

    Real tmpNominal1, tmpNominal2;
    bool tmpPayCurrency1;
    if (ccy1_ == arguments_.currency1) {
        QL_REQUIRE(ccy2_ == arguments_.currency2, "mismatched currency pairs ("
                                                      << ccy1_ << "," << ccy2_ << ") in the egine and ("
                                                      << arguments_.currency1 << "," << arguments_.currency2
                                                      << ") in the instrument");
        tmpNominal1 = arguments_.nominal1;
        tmpNominal2 = arguments_.nominal2;
        tmpPayCurrency1 = arguments_.payCurrency1;
    } else {
        QL_REQUIRE(ccy1_ == arguments_.currency2 && ccy2_ == arguments_.currency1,
                   "mismatched currency pairs (" << ccy1_ << "," << ccy2_ << ") in the egine and ("
                                                 << arguments_.currency1 << "," << arguments_.currency2
                                                 << ") in the instrument");
        tmpNominal1 = arguments_.nominal2;
        tmpNominal2 = arguments_.nominal1;
        tmpPayCurrency1 = !arguments_.payCurrency1;
    }

    QL_REQUIRE(!currency1Discountcurve_.empty() && !currency2Discountcurve_.empty(),
               "Discounting term structure handle is empty.");

    QL_REQUIRE(currency1Discountcurve_->referenceDate() == currency2Discountcurve_->referenceDate(),
               "Term structures should have the same reference date.");

    QL_REQUIRE(arguments_.maturityDate >= currency1Discountcurve_->referenceDate(),
               "FX forward maturity should exceed or equal the "
               "discount curve reference date.");

    results_.value = 0.0;
    results_.fairForwardRate = ExchangeRate(ccy2_, ccy1_, tmpNominal1 / tmpNominal2); // strike rate

    if (!detail::simple_event(arguments_.maturityDate).hasOccurred(settlementDate, includeSettlementDateFlows_)) {
        Real disc1near = currency1Discountcurve_->discount(npvDate);
        Real disc1far = currency1Discountcurve_->discount(arguments_.maturityDate);
        Real disc2near = currency2Discountcurve_->discount(npvDate);
        Real disc2far = currency2Discountcurve_->discount(arguments_.maturityDate);
        Real fxfwd = disc1near / disc1far * disc2far / disc2near * spotFX_->value();
        // results_.value =
        //     (tmpPayCurrency1 ? -1.0 : 1.0) * (tmpNominal1 * disc1far / disc1near -
        //                                       tmpNominal2 * disc2far / disc2near * spotFX_->value());
        results_.value = (tmpPayCurrency1 ? -1.0 : 1.0) * disc1far / disc1near * (tmpNominal1 - tmpNominal2 * fxfwd);
        results_.fairForwardRate = ExchangeRate(ccy2_, ccy1_, fxfwd);
    }
    results_.npv = Money(ccy1_, results_.value);

} // calculate

} // namespace QuantExt
