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

#include <qle/instruments/cashflowresults.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
namespace QuantExt {

using namespace QuantLib;

std::ostream& operator<<(std::ostream& out, const CashFlowResults& t) {
    if (t.amount != Null<Real>())
        out << t.amount;
    else
        out << "?";
    out << " " << t.currency;
    if (t.payDate != Null<Date>())
        out << " @ " << QuantLib::io::iso_date(t.payDate);
    return out;
}

CashFlowResults standardCashFlowResults(const QuantLib::ext::shared_ptr<CashFlow>& c, const Real multiplier,
                                        const std::string& type, const Size legNo, const Currency& currency,
                                        const Handle<YieldTermStructure>& discountCurve) {

    CashFlowResults cfResults = populateCashFlowResultsFromCashflow(c, multiplier, legNo, currency);

    if (!type.empty()) {
        cfResults.type = type;
    }

    if (!discountCurve.empty()) {
        cfResults.discountFactor = discountCurve->discount(cfResults.payDate);
        cfResults.presentValue = cfResults.amount * cfResults.discountFactor;
    }
    return cfResults;
}

CashFlowResults populateCashFlowResultsFromCashflow(const QuantLib::ext::shared_ptr<QuantLib::CashFlow>& c,
                                                    const QuantLib::Real multiplier, const QuantLib::Size legNo,
                                                    const QuantLib::Currency& currency) {

    Date today = Settings::instance().evaluationDate();

    CashFlowResults cfResults;

    cfResults.amount = c->amount() * multiplier;
    cfResults.payDate = c->date();

    if (!currency.empty())
        cfResults.currency = currency.code();

    cfResults.legNumber = legNo;

    if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
        cfResults.rate = cpn->rate();
        cfResults.accrualStartDate = cpn->accrualStartDate();
        cfResults.accrualEndDate = cpn->accrualEndDate();
        cfResults.accrualPeriod = cpn->accrualPeriod();
        cfResults.accruedAmount = cpn->accruedAmount(today);
        cfResults.notional = cpn->nominal();
        cfResults.type = "Interest";
        if (auto ptrFloat = QuantLib::ext::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(cpn)) {
            cfResults.fixingDate = ptrFloat->fixingDate();
            cfResults.fixingValue = ptrFloat->index()->fixing(cfResults.fixingDate);
            if (cfResults.fixingDate > today)
                cfResults.type = "InterestProjected";
        } else if (auto ptrInfl = QuantLib::ext::dynamic_pointer_cast<QuantLib::InflationCoupon>(cpn)) {
            // We return the last fixing inside the coupon period
            cfResults.fixingDate = ptrInfl->fixingDate();
            cfResults.fixingValue = ptrInfl->indexFixing();
            cfResults.type = "Inflation";
        } else if (auto ptrBMA = QuantLib::ext::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(cpn)) {
            // We return the last fixing inside the coupon period
            cfResults.fixingDate = ptrBMA->fixingDates().end()[-2];
            cfResults.fixingValue = ptrBMA->pricer()->swapletRate();
            if (cfResults.fixingDate > today)
                cfResults.type = "BMAaverage";
        }
    } else {
        cfResults.type = "Notional";
        if (auto ptrIndCf = QuantLib::ext::dynamic_pointer_cast<QuantLib::IndexedCashFlow>(c)) {
            cfResults.fixingDate = ptrIndCf->fixingDate();
            cfResults.fixingValue = ptrIndCf->index()->fixing(cfResults.fixingDate);
            cfResults.type = "Index";
        } else if (auto ptrFxlCf = QuantLib::ext::dynamic_pointer_cast<QuantExt::FXLinkedCashFlow>(c)) {
            // We return the last fixing inside the coupon period
            cfResults.fixingDate = ptrFxlCf->fxFixingDate();
            cfResults.fixingValue = ptrFxlCf->fxRate();
        }
    }
    return cfResults;
}
} // namespace QuantExt
