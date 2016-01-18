/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ql/cashflows/cashflows.hpp>
#include <ql/exchangerate.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <qle/pricingengines/crossccyswapengine.hpp>

namespace QuantExt {

    CrossCcySwapEngine::CrossCcySwapEngine(
        const Currency &ccy1,
        const Handle<YieldTermStructure> &currency1Discountcurve,
        const Currency &ccy2,
        const Handle<YieldTermStructure> &currency2Discountcurve,
        const Handle<Quote> &spotFX,
        boost::optional<bool> includeSettlementDateFlows,
        const Date &settlementDate, const Date &npvDate)
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

        if (npvDate_ == Null<Date>()) {
            npvDate_ = currency1Discountcurve_->referenceDate();
        }
        if (settlementDate_ == Null<Date>()) {
            settlementDate_ = npvDate_;
        }

    }

    void CrossCcySwapEngine::calculate() const {

        QL_REQUIRE(!currency1Discountcurve_.empty() &&
            !currency1Discountcurve_.empty(),
            "Discounting term structure handle is empty.");

        QL_REQUIRE(!spotFX_.empty(), "FX spot quote handle is empty.");

        QL_REQUIRE(currency1Discountcurve_->referenceDate() ==
                       currency2Discountcurve_->referenceDate(),
                   "Term structures should have the same reference date.");

        Size numLegs = arguments_.legs.size();

        results_.value = 0.0;

        // - Swap::Results
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

        for (Size legNo = 0; legNo < numLegs; legNo++) {
            try {
                // Choose the correct discount curve for the leg.
                Handle<YieldTermStructure> legDiscountCurve;
                if (arguments_.currencies[legNo] == ccy1_) {
                    legDiscountCurve = currency1Discountcurve_;
                } else {
                    QL_REQUIRE(arguments_.currencies[legNo] == ccy2_,
                               "leg ccy (" << arguments_.currencies[legNo]
                                           << ") must be ccy1 (" << ccy1_
                                           << ") or ccy2 (" << ccy2_ << ")");
                    legDiscountCurve = currency2Discountcurve_;
                }
                results_.npvDateDiscounts[legNo] = legDiscountCurve->
                    discount(npvDate_);

                // Calculate the NPV and BPS of each leg in its currency.
                CashFlows::npvbps(arguments_.legs[legNo],
                    **legDiscountCurve,
                    includeReferenceDateFlows,
                    settlementDate_,
                    npvDate_,
                    results_.inCcyLegNPV[legNo],
                    results_.inCcyLegBPS[legNo]);
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
                    results_.startDiscounts[legNo] =
                        legDiscountCurve->discount(startDate);
                } else {
                   results_.startDiscounts[legNo] = Null<DiscountFactor>();
                }

                Date maturityDate = CashFlows::maturityDate(
                    arguments_.legs[legNo]);
                if (maturityDate >= currency1Discountcurve_->referenceDate()) {
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
