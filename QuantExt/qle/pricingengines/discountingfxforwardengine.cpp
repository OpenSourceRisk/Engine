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
        const Handle<Quote>& spotFX,
        boost::optional<bool> includeSettlementDateFlows,
        const Date& settlementDate,
        const Date& npvDate)
    : ccy1_(ccy1),
      currency1Discountcurve_(currency1Discountcurve),
      ccy2_(ccy2),
      currency2Discountcurve_(currency2Discountcurve),
      spotFX_(spotFX),
      includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDate_(settlementDate),
      npvDate_(npvDate) {
        registerWith(currency1Discountcurve_);
        registerWith(currency2Discountcurve_);
        registerWith(spotFX_);

        if(npvDate_ == Null<Date>()) {
            npvDate_ = currency1Discountcurve_->referenceDate();
        }
        if(settlementDate_ == Null<Date>()) {
            settlementDate_ = npvDate_;
        }
    }

    void DiscountingFxForwardEngine::calculate() const {

        Real tmpNominal1, tmpNominal2;
        if(ccy1_ == arguments_.currency1) {
            QL_REQUIRE(ccy2_ == arguments_.currency2,
                       "mismatched currency pairs ("
                           << ccy1_ << "," << ccy2_ << ") in the egine and ("
                           << arguments_.currency1 << ","
                           << arguments_.currency2 << ") in the instrument");
            tmpNominal1 = arguments_.nominal1;
            tmpNominal2 = arguments_.nominal2;
        }
        else {
            QL_REQUIRE(ccy1_ == arguments_.currency2 &&
                           ccy2_ == arguments_.currency1,
                       "mismatched currency pairs ("
                           << ccy1_ << "," << ccy2_ << ") in the egine and ("
                           << arguments_.currency1 << ","
                           << arguments_.currency2 << ") in the instrument");
            tmpNominal1 = arguments_.nominal2;
            tmpNominal2 = arguments_.nominal1;
        }

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

        results_.value = 0.0;
        if (!detail::simple_event(arguments_.maturityDate).hasOccurred(
                settlementDate_, includeSettlementDateFlows_)) {
            results_.value = (arguments_.payCurrency1 ? -1.0 : +1.0) * (
                tmpNominal1 *
                currency1Discountcurve_->discount(arguments_.maturityDate) /
                currency1Discountcurve_->discount(npvDate_) -
                tmpNominal2 *
                currency2Discountcurve_->discount(arguments_.maturityDate) /
                currency2Discountcurve_->discount(npvDate_)*spotFX_->value());
        }
        results_.npv = Money(ccy1_, results_.value);
        results_.fairForwardRate = ExchangeRate(ccy2_, ccy1_, tmpNominal1/tmpNominal2);

    } // calculate

} // namespace QuantExt
