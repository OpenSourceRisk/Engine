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
#include <qle/instruments/cashflowresults.hpp>

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
                                                      << ccy1_ << "," << ccy2_ << ") in the engine and ("
                                                      << arguments_.currency1 << "," << arguments_.currency2
                                                      << ") in the instrument");
        tmpNominal1 = arguments_.nominal1;
        tmpNominal2 = arguments_.nominal2;
        tmpPayCurrency1 = arguments_.payCurrency1;
    } else {
        QL_REQUIRE(ccy1_ == arguments_.currency2 && ccy2_ == arguments_.currency1,
                   "mismatched currency pairs (" << ccy1_ << "," << ccy2_ << ") in the engine and ("
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

    QL_REQUIRE(arguments_.payDate >= currency1Discountcurve_->referenceDate(),
               "FX forward maturity should exceed or equal the "
               "discount curve reference date.");

    results_.value = 0.0;
    results_.fairForwardRate = ExchangeRate(ccy2_, ccy1_, tmpNominal1 / tmpNominal2); // strike rate
    results_.additionalResults["fairForwardRate"] = tmpNominal1 / tmpNominal2;
    results_.additionalResults["currency[1]"] = ccy1_.code();
    results_.additionalResults["currency[2]"] = ccy2_.code();

    // The instrument flag overrides what is passed to the engine c'tor
    bool includeSettlementDateFlows = arguments_.includeSettlementDateFlows;
    
    if (!detail::simple_event(arguments_.payDate).hasOccurred(settlementDate, includeSettlementDateFlows)) {
        Real disc1near = currency1Discountcurve_->discount(npvDate);
        Real disc1far = currency1Discountcurve_->discount(arguments_.payDate);
        Real disc2near = currency2Discountcurve_->discount(npvDate);
        Real disc2far = currency2Discountcurve_->discount(arguments_.payDate);
        Real fxfwd = disc1near / disc1far * disc2far / disc2near * spotFX_->value();

        // settle ccy is ccy1 if no pay ccy provided
        Currency settleCcy = arguments_.payCcy.empty() ? ccy1_ : arguments_.payCcy;
        bool settleCcy1 = ccy1_ == settleCcy;

        Real discNear = settleCcy1 ? disc1near : disc2near;
        Real discFar = settleCcy1 ? disc1far : disc2far;
        Real fx1 = settleCcy1 ? 1.0 : fxfwd;
        Real fx2 = settleCcy1 ? 1 / fxfwd : 1.0;

        QL_REQUIRE(arguments_.isPhysicallySettled || arguments_.payDate <= arguments_.fixingDate ||
                       arguments_.fxIndex != nullptr,
                   "If pay date (" << arguments_.payDate << ") is strictly after fixing date (" << arguments_.fixingDate
                                   << "), an FX Index must be given for a cash-settled FX Forward.");
        if (!arguments_.isPhysicallySettled && arguments_.payDate >= arguments_.fixingDate &&
            arguments_.fxIndex != nullptr) {
            fx1 = settleCcy1 ? 1.0 : arguments_.fxIndex->fixing(arguments_.fixingDate);
            fx2 = settleCcy1 ? arguments_.fxIndex->fixing(arguments_.fixingDate) : 1.0;
            fxfwd = arguments_.fxIndex->fixing(arguments_.fixingDate);
        }

        // populate cashflow results
        std::vector<CashFlowResults> cashFlowResults;
        CashFlowResults cf1, cf2;
        cf1.payDate = arguments_.payDate;
        cf1.type = "Notional";
        cf2.payDate = arguments_.payDate;
        cf2.type = "Notional";
        if (!arguments_.isPhysicallySettled) {
            if (arguments_.payDate >= arguments_.fixingDate) {
                cf1.fixingDate = arguments_.fixingDate;
                cf2.fixingDate = arguments_.fixingDate;
            }
            cf1.fixingValue = 1.0 / fx1;
            cf2.fixingValue = 1.0 / fx2;
            cf1.amount = (tmpPayCurrency1 ? -1.0 : 1.0) * tmpNominal1 / fx1;
            cf2.amount = (tmpPayCurrency1 ? -1.0 : 1.0) * (-tmpNominal2 / fx2);
            cf1.currency = settleCcy.code();
            cf2.currency = settleCcy.code();
        } else {
            cf1.amount = (tmpPayCurrency1 ? -1.0 : 1.0) * tmpNominal1;
            cf2.amount = (tmpPayCurrency1 ? -1.0 : 1.0) * (-tmpNominal2);
            cf1.currency = ccy1_.code();
            cf2.currency = ccy2_.code();
        }
        if (ccy1_ == arguments_.currency1) {
            cf1.legNumber = 0;
            cf2.legNumber = 1;
            cashFlowResults.push_back(cf1);
            cashFlowResults.push_back(cf2);
        } else {
            cf1.legNumber = 1;
            cf2.legNumber = 0;
            cashFlowResults.push_back(cf2);
            cashFlowResults.push_back(cf1);
        }
        results_.additionalResults["cashFlowResults"] = cashFlowResults;

        results_.value = (tmpPayCurrency1 ? -1.0 : 1.0) * discFar / discNear * (tmpNominal1 / fx1 - tmpNominal2 / fx2);

        results_.npv = Money(settleCcy, results_.value);

        results_.fairForwardRate = ExchangeRate(ccy2_, ccy1_, fxfwd);
        results_.additionalResults["fairForwardRate"] = fxfwd;
        results_.additionalResults["fxSpot"] = spotFX_->value();
        results_.additionalResults["discountFactor[1]"] = disc1far;
        results_.additionalResults["discountFactor[2]"] = disc2far;
        results_.additionalResults["legNPV[1]"] = (tmpPayCurrency1 ? -1.0 : 1.0) * discFar / discNear * tmpNominal1 / fx1;
        results_.additionalResults["legNPV[2]"] = (tmpPayCurrency1 ? -1.0 : 1.0) * discFar / discNear * (-tmpNominal2 / fx2);

        // set notional
        if (arguments_.isPhysicallySettled) {
            // Align notional with ISDA AANA/GRID guidance as of November 2020 for deliverable forwards
            if (tmpNominal1 > tmpNominal2 * fxfwd) {
                results_.additionalResults["currentNotional"] = tmpNominal1;
                results_.additionalResults["notionalCurrency"] = ccy1_.code();
            } else {
                results_.additionalResults["currentNotional"] = tmpNominal2;
                results_.additionalResults["notionalCurrency"] = ccy2_.code();
            }
        } else {
            // for cash settled forwards we take the notional from the settlement ccy leg
            results_.additionalResults["currentNotional"] =
                arguments_.currency1 == arguments_.payCcy ? arguments_.nominal1 : arguments_.nominal2;
            results_.additionalResults["notionalCurrency"] = arguments_.payCcy.code();
        }
    }

} // calculate

} // namespace QuantExt
