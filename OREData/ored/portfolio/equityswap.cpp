/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/equityswap.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/equitycoupon.hpp>

namespace ore {
namespace data {

EquitySwap::EquitySwap(const Envelope& env, const vector<LegData>& legData) : Swap(env, legData, "EquitySwap") {}

EquitySwap::EquitySwap(const Envelope& env, const LegData& leg0, const LegData& leg1)
    : Swap(env, leg0, leg1, "EquitySwap") {}

void EquitySwap::checkEquitySwap(const vector<LegData>& legData) {
    // An Equity Swap must have 2 legs - 1 an Equity Leg and the other an IR Leg either Fixed or Floating
    equityLegIndex_ = Null<Size>();
    irLegIndex_ = Null<Size>();
    for (Size i = 0; i < legData.size(); ++i) {
        if (legData[i].legType() == "Equity") {
            equityLegIndex_ = i;
        } else {
            irLegIndex_ = i;
        }
    }
    QL_REQUIRE((legData.size() == 2) && equityLegIndex_ != Null<Size>() && irLegIndex_ != Null<Size>(),
               "An Equity Swap must have 2 legs, an Equity Leg and an IR Leg - Trade: " + id());
}

void EquitySwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("EquitySwap::build() called for " << id());

    checkEquitySwap(legData_);

    QL_REQUIRE(equityLegIndex_ < legData_.size(), "equityLegIndex (" << equityLegIndex_
                                                                     << ") out of range, legData has size "
                                                                     << legData_.size() << ", this is unexpected");
    QL_REQUIRE(irLegIndex_ < legData_.size(), "equityLegIndex (" << irLegIndex_ << ") out of range, legData has size "
                                                                 << legData_.size() << ", this is unexpected");

    // 1 add indexing data from the equity leg, if this is desired

    auto eqLegData = QuantLib::ext::dynamic_pointer_cast<EquityLegData>(legData_[equityLegIndex_].concreteLegData());
    QL_REQUIRE(eqLegData, "could not cast to EquityLegData for equity leg in equity swap, this is unexpected");

    if (legData_[irLegIndex_].indexingFromAssetLeg() && eqLegData->notionalReset()) {
        DLOG("adding indexing information from equity leg to funding leg");

        // build valuation schedule
        std::vector<string> valuationDates;
        auto legBuilder = engineFactory->legBuilder(legData_[equityLegIndex_].legType());
        RequiredFixings dummy;
        Leg tmpEqLeg = legBuilder->buildLeg(legData_[equityLegIndex_], engineFactory, dummy,
                                            engineFactory->configuration(MarketContext::pricing));
        for (Size i = 0; i < tmpEqLeg.size(); ++i) {
            auto eqCpn = QuantLib::ext::dynamic_pointer_cast<QuantExt::EquityCoupon>(tmpEqLeg[i]);
            QL_REQUIRE(eqCpn, "EquitySwap::build(): expected EquityCoupon on equity leg, this is unexpected");
            valuationDates.push_back(ore::data::to_string(eqCpn->fixingStartDate()));
            if (i == tmpEqLeg.size() - 1)
                valuationDates.push_back(ore::data::to_string(eqCpn->fixingEndDate()));
        }
        ScheduleData valuationSchedule(ScheduleDates("", "", "", valuationDates, ""));

        // add equity indexing
        QL_REQUIRE(eqLegData->quantity() != Null<Real>(),
                   "indexing can only be added to funding leg, if quantity is given on equity leg");
        Indexing eqIndexing("EQ-" + eqLegData->eqName(), "", false, false, false, eqLegData->quantity(),
                            eqLegData->initialPrice(), Null<Real>(), valuationSchedule, 0, "", "U", false);
        legData_[irLegIndex_].indexing().push_back(eqIndexing);

        // add fx indexing, if applicable
        if (!eqLegData->fxIndex().empty()) {
            Real initialFxFixing = Null<Real>();
            // if the eq leg has an initial price and this is in the eq leg currency, we do not want an FX conversion
            // for this price
            if (!eqLegData->initialPriceCurrency().empty() &&
                eqLegData->initialPriceCurrency() == legData_[equityLegIndex_].currency() &&
                eqLegData->initialPrice() != Null<Real>())
                initialFxFixing = 1.0;
            Indexing fxIndexing(eqLegData->fxIndex(), "", false, false, false, 1.0, initialFxFixing, Null<Real>(),
                                valuationSchedule, 0, "", "U", false);
            legData_[irLegIndex_].indexing().push_back(fxIndexing);
        }

        // set notional node to 1.0
        legData_[irLegIndex_].notionals() = std::vector<Real>(1, 1.0);
        legData_[irLegIndex_].notionalDates() = std::vector<std::string>();

        // reset flag that told us to pull the indexing information from the equity leg
        legData_[irLegIndex_].indexingFromAssetLeg() = false;
    }

    // just underlying security; notionals and currencies are covered by the Swap class already
    additionalData_["underlyingSecurityId"] = eqLegData->eqName();

    // 2 now build the swap using the updated leg data

    Swap::build(engineFactory);
}

void EquitySwap::setIsdaTaxonomyFields() {
    Swap::setIsdaTaxonomyFields();

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("Swap");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");
}

QuantLib::Real EquitySwap::notional() const {
    Date asof = Settings::instance().evaluationDate();
    for (auto const& c : legs_[equityLegIndex_]) {
        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<QuantExt::EquityCoupon>(c)) {
            if (c->date() > asof)
                return cpn->nominal();
        }
    }
    ALOG("Error retrieving current notional for equity swap " << id() << " as of " << io::iso_date(asof));
    return Null<Real>();
}

std::string EquitySwap::notionalCurrency() const {
    // try to get the notional ccy from the additional results of the instrument
    if (legCurrencies_.size() > equityLegIndex_)
        return legCurrencies_[equityLegIndex_];
    else
        return Swap::notionalCurrency();
}

} // namespace data
} // namespace ore
