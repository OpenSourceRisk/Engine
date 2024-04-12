/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/models/representativefxoption.hpp
    \brief representative fx option matcher
    \ingroup models
*/

#include <qle/models/representativefxoption.hpp>

#include <qle/cashflows/scaledcoupon.hpp>

#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/math/functional.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/quotes/simplequote.hpp>

namespace QuantExt {

RepresentativeFxOptionMatcher::RepresentativeFxOptionMatcher(
    const std::vector<Leg>& underlying, const std::vector<bool>& isPayer, const std::vector<std::string>& currencies,
    const Date& referenceDate, const std::string& forCcy, const std::string& domCcy,
    const Handle<YieldTermStructure>& forCurve, const Handle<YieldTermStructure>& domCurve, const Handle<Quote>& fxSpot,
    const bool includeRefDateFlows) {

    Date today = Settings::instance().evaluationDate();

    // 1 check inputs

    QL_REQUIRE(referenceDate >= today, "RepresentativeFxOptionMatcher: referenceDate ("
                                           << referenceDate << ") must be < today (" << today << ")");
    QL_REQUIRE(isPayer.size() == underlying.size(), "RepresentativeFxOptionMatcher: isPayer ("
                                                        << isPayer.size() << ") does not match underlying ("
                                                        << underlying.size() << ")");
    QL_REQUIRE(currencies.size() == underlying.size(), "RepresentativeFxOptionMatcher: currencies ("
                                                           << currencies.size() << ") does not match underlying ("
                                                           << underlying.size() << ")");

    // 2a  make a copy of the input fx spot that we can shift later

    auto fxScenarioValue = QuantLib::ext::make_shared<SimpleQuote>(fxSpot->value());
    Handle<Quote> fxSpotScen(fxScenarioValue);

    // 2b create the inverse spot, this is convenient when we clone fx linked cashflows below

    auto m = [](Real x) { return 1.0 / x; };
    Handle<Quote> fxSpotScenInv(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(fxSpotScen, m));

    // 3 collect cashflows by their pay currencies and link them to the fx spot copy (if applicable)

    Leg forCfs, domCfs;

    for (Size i = 0; i < underlying.size(); ++i) {
        // 3a check the pay ccy is one of the two currencies to be matched

        QL_REQUIRE(currencies[i] == forCcy || currencies[i] == domCcy,
                   "RepresentativeFxOptionMatcher: currency at index "
                       << i << " (" << currencies[i] << ") does not match forCcy (" << forCcy << ") or domCcy ("
                       << domCcy << ")");

        for (auto const& c : underlying[i]) {

            // 3b skip cashflows with pay date on or

            if (c->date() < referenceDate || (c->date() == referenceDate && !includeRefDateFlows))
                continue;

            // 3b check for non-supported coupon types that are linked to fx

            QL_REQUIRE(!QuantLib::ext::dynamic_pointer_cast<IndexedCoupon>(c),
                       "RepresentativeFxOptionMatcher: Indexed Coupons are not supported");

            QuantLib::ext::shared_ptr<CashFlow> res;

            if (auto fxlinked = QuantLib::ext::dynamic_pointer_cast<FXLinked>(c)) {

                // 3c clone fx linked coupons and link them to our scenario fx spot quote

                std::set<std::string> ccys = {fxlinked->fxIndex()->sourceCurrency().code(),
                                              fxlinked->fxIndex()->targetCurrency().code()};
                QL_REQUIRE((ccys == std::set<std::string>{forCcy, domCcy}),
                           "RepresentativeFxOptionMatcher: FXLinked coupon ccys "
                               << fxlinked->fxIndex()->sourceCurrency().code() << ", "
                               << fxlinked->fxIndex()->targetCurrency().code()
                               << " do noth match currencies to be matched (" << forCcy << ", " << domCcy << ")");

                res = QuantLib::ext::dynamic_pointer_cast<CashFlow>(fxlinked->clone(fxlinked->fxIndex()->clone(
                    fxlinked->fxIndex()->sourceCurrency().code() == forCcy ? fxSpotScen : fxSpotScenInv,
                    fxlinked->fxIndex()->sourceCurve(), fxlinked->fxIndex()->targetCurve())));
                QL_REQUIRE(res, "RepresentativeFxOptionMatcher: internal error, cloned fx linked cashflow could not be "
                                "cast to CashFlow");

            } else {

                // 3d for all other coupons, just push a fixed cashflow with the amount

                res = QuantLib::ext::make_shared<SimpleCashFlow>(c->amount(), c->date());
            }

            // 3e push the cashflow with the correct sign to the vector holding the flows in the its pay ccy

            res = QuantLib::ext::make_shared<ScaledCashFlow>(isPayer[i] ? -1.0 : 1.0, res);

            if (currencies[i] == forCcy)
                forCfs.push_back(res);
            else
                domCfs.push_back(res);
        }
    }

    // 4a determine the NPV of the collected cashflows in dom ccy and as seen from the global evaluation date

    Real npv =
        CashFlows::npv(forCfs, **forCurve, false) * fxSpotScen->value() + CashFlows::npv(domCfs, **domCurve, false);

    // 4b determine the FX Delta of the collected cashflows as seen from the global evaluation date

    constexpr Real relShift = 0.01;

    Real baseFx = fxScenarioValue->value();

    fxScenarioValue->setValue(baseFx * (1.0 + relShift));
    Real npv_up =
        CashFlows::npv(forCfs, **forCurve, false) * fxSpotScen->value() + CashFlows::npv(domCfs, **domCurve, false);

    fxScenarioValue->setValue(baseFx * (1.0 - relShift));
    Real npv_down =
        CashFlows::npv(forCfs, **forCurve, false) * fxSpotScen->value() + CashFlows::npv(domCfs, **domCurve, false);

    Real fxDelta = (npv_up - npv_down) / (2.0 * baseFx * relShift);

    // 4c determine the amounts in for and dom ccy matching the fx delta and the npv, as seen from the global evaluation
    // date, then compound the resulting amounts to the reference date

    amount1_ = fxDelta / forCurve->discount(referenceDate);
    amount2_ = (npv - fxDelta * baseFx) / domCurve->discount(referenceDate);
    ccy1_ = forCcy;
    ccy2_ = domCcy;
}

} // namespace QuantExt
