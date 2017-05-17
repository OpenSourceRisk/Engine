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

#include <ql/cashflows/cashflows.hpp>
#include <ql/exchangerate.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <qle/pricingengines/crossccyswapengine.hpp>

namespace QuantExt {

CrossCcySwapEngine::CrossCcySwapEngine(const Currency& ccy1, const Handle<YieldTermStructure>& currency1Discountcurve,
                                       const Currency& ccy2, const Handle<YieldTermStructure>& currency2Discountcurve,
                                       const Handle<Quote>& spotFX, boost::optional<bool> includeSettlementDateFlows,
                                       const Date& settlementDate, const Date& npvDate)
    : ccy1_(ccy1), currency1Discountcurve_(currency1Discountcurve), ccy2_(ccy2),
      currency2Discountcurve_(currency2Discountcurve), spotFX_(spotFX),
      includeSettlementDateFlows_(includeSettlementDateFlows), settlementDate_(settlementDate), npvDate_(npvDate) {

    registerWith(currency1Discountcurve_);
    registerWith(currency2Discountcurve_);
    registerWith(spotFX_);
}

void CrossCcySwapEngine::calculate() const {

    QL_REQUIRE(!currency1Discountcurve_.empty() && !currency2Discountcurve_.empty(),
               "Discounting term structure handle is empty.");

    QL_REQUIRE(!spotFX_.empty(), "FX spot quote handle is empty.");

    QL_REQUIRE(currency1Discountcurve_->referenceDate() == currency2Discountcurve_->referenceDate(),
               "Term structures should have the same reference date.");
    Date referenceDate = currency1Discountcurve_->referenceDate();
    Date settlementDate = settlementDate_;
    if (settlementDate_ == Date()) {
        settlementDate = referenceDate;
    } else {
        QL_REQUIRE(settlementDate >= referenceDate, "Settlement date (" << settlementDate
                                                                        << ") cannot be before discount curve "
                                                                           "reference date ("
                                                                        << referenceDate << ")");
    }

    Size numLegs = arguments_.legs.size();
    // - Instrument::Results
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
    // - Swap::Results
    results_.legNPV.resize(numLegs);
    results_.legBPS.resize(numLegs);
    results_.startDiscounts.resize(numLegs);
    results_.endDiscounts.resize(numLegs);
    // - CrossCcySwap::Results
    results_.inCcyLegNPV.resize(numLegs);
    results_.inCcyLegBPS.resize(numLegs);
    results_.npvDateDiscounts.resize(numLegs);

    bool includeReferenceDateFlows =
        includeSettlementDateFlows_ ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();

    for (Size legNo = 0; legNo < numLegs; legNo++) {
        try {
            // Choose the correct discount curve for the leg.
            Handle<YieldTermStructure> legDiscountCurve;
            if (arguments_.currencies[legNo] == ccy1_) {
                legDiscountCurve = currency1Discountcurve_;
            } else {
                QL_REQUIRE(arguments_.currencies[legNo] == ccy2_, "leg ccy (" << arguments_.currencies[legNo]
                                                                              << ") must be ccy1 (" << ccy1_
                                                                              << ") or ccy2 (" << ccy2_ << ")");
                legDiscountCurve = currency2Discountcurve_;
            }
            results_.npvDateDiscounts[legNo] = legDiscountCurve->discount(results_.valuationDate);

            // Calculate the NPV and BPS of each leg in its currency.
            CashFlows::npvbps(arguments_.legs[legNo], **legDiscountCurve, includeReferenceDateFlows, settlementDate,
                              results_.valuationDate, results_.inCcyLegNPV[legNo], results_.inCcyLegBPS[legNo]);
            results_.inCcyLegNPV[legNo] *= arguments_.payer[legNo];
            results_.inCcyLegBPS[legNo] *= arguments_.payer[legNo];

            results_.legNPV[legNo] = results_.inCcyLegNPV[legNo];
            results_.legBPS[legNo] = results_.inCcyLegBPS[legNo];

            // Convert to NPV currency if necessary.
            if (arguments_.currencies[legNo] != ccy1_) {
                results_.legNPV[legNo] *= spotFX_->value();
                results_.legBPS[legNo] *= spotFX_->value();
            }

            // Get start date and end date discount for the leg
            Date startDate = CashFlows::startDate(arguments_.legs[legNo]);
            if (startDate >= currency1Discountcurve_->referenceDate()) {
                results_.startDiscounts[legNo] = legDiscountCurve->discount(startDate);
            } else {
                results_.startDiscounts[legNo] = Null<DiscountFactor>();
            }

            Date maturityDate = CashFlows::maturityDate(arguments_.legs[legNo]);
            if (maturityDate >= currency1Discountcurve_->referenceDate()) {
                results_.endDiscounts[legNo] = legDiscountCurve->discount(maturityDate);
            } else {
                results_.endDiscounts[legNo] = Null<DiscountFactor>();
            }

        } catch (std::exception& e) {
            QL_FAIL(io::ordinal(legNo + 1) << " leg: " << e.what());
        }

        results_.value += results_.legNPV[legNo];
    }
}
} // namespace QuantExt
