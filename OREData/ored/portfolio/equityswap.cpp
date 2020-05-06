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
        } else if (legData[i].legType() == "Fixed" || legData[i].legType() == "Floating") {
            irLegIndex_ = i;
        }
    }
    QL_REQUIRE((legData.size() == 2) && equityLegIndex_ != Null<Size>() && irLegIndex_ != Null<Size>(),
               "An Equity Swap must have 2 legs, an Equity Leg and an IR Leg - Trade: " + id());
}

void EquitySwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("EquitySwap::build() called for " << id());

    checkEquitySwap(legData_);

    /* before we build the equity swap we modify the leg data according to these rules:
       1) if the IR leg has an indexing without an index given, we set this to the EQ index from the equity leg
       2) if the IR leg has an equity indexing, but no notional, we add a notional node with notional = 1
       3) if the IR leg has an equity indexing, but no InitialFixing and the equity leg has an initial price,
          we set the InitialFixing on the IR leg to the initial price from the equity leg
       4) for all indexings on the IR leg, we add an explicit valuation schedule with the valuation dates
          taken from the equity leg, if no valuation schedule is given; in this case we reset the FixingDays,
          FixingCalendar, FixingConvention and IsInArrears flag so that they don't change this schedule */

    QL_REQUIRE(equityLegIndex_ < legData_.size(), "equityLegIndex (" << equityLegIndex_
                                                                     << ") out of range, legData has size "
                                                                     << legData_.size() << ", this is unexpected");
    QL_REQUIRE(irLegIndex_ < legData_.size(), "equityLegIndex (" << irLegIndex_ << ") out of range, legData has size "
                                                                 << legData_.size() << ", this is unexpected");

    // loop over the indexings of the ir leg (if any) and modify them according to the rules above

    auto eqLegData = boost::dynamic_pointer_cast<EquityLegData>(legData_[equityLegIndex_].concreteLegData());
    QL_REQUIRE(eqLegData, "could not cast to EquityLegData for equity leg in equity swap, this is unexpected");
    std::vector<string> valuationDates;

    for (auto& i : legData_[irLegIndex_].indexing()) {

        // apply rule 1 from above
        if (i.hasData() && i.index().empty()) {
            DLOG("adding index from equity leg '" << eqLegData->eqName() << "' to Indexing node on IR leg");
            i.index() = "EQ-" + eqLegData->eqName();
        }

        // apply rule 2 from above
        if (i.hasData() && boost::starts_with(i.index(), "EQ-")) {
            if (i.initialFixing() == Null<Real>() && eqLegData->initialPrice() != Null<Real>()) {
                DLOG("adding initial fixing from equity leg (" << eqLegData->initialPrice()
                                                               << ") to equity Indexing node on IR leg");
                i.initialFixing() = eqLegData->initialPrice();
            }
        }

        // apply rule 3 from above
        if (i.hasData() && boost::starts_with(i.index(), "EQ-") && legData_[irLegIndex_].notionals().empty()) {
            DLOG("adding notional = 1 to equity Indexing node on IR leg");
            legData_[irLegIndex_].notionals() = std::vector<Real>(1, 1.0);
        }

        // apply rule 4 from above
        if (i.hasData() && !i.valuationSchedule().hasData()) {
            DLOG("adding explicit valuation schedule to Indexing node (index = " << i.index() << ") on IR leg");
            // build the equity leg, if we don't have it yet
            if (valuationDates.empty()) {
                auto legBuilder = engineFactory->legBuilder(legData_[equityLegIndex_].legType());
                RequiredFixings dummy;
                Leg tmpEqLeg = legBuilder->buildLeg(legData_[equityLegIndex_], engineFactory, dummy,
                                                    engineFactory->configuration(MarketContext::pricing));
                for (Size i = 0; i < tmpEqLeg.size(); ++i) {
                    auto eqCpn = boost::dynamic_pointer_cast<QuantExt::EquityCoupon>(tmpEqLeg[i]);
                    QL_REQUIRE(eqCpn, "EquitySwap::build(): expected EquityCoupon on equity leg, this is unexpected");
                    valuationDates.push_back(ore::data::to_string(eqCpn->fixingStartDate()));
                    if (i == tmpEqLeg.size() - 1)
                        valuationDates.push_back(ore::data::to_string(eqCpn->fixingEndDate()));
                }
            }
            i.valuationSchedule() = ScheduleData(ScheduleDates("", "", "", valuationDates, ""));
            i.fixingDays() = 0;
            i.fixingCalendar() = "";
            i.fixingConvention() = "U";
            i.inArrearsFixing() = false;
        }
    }

    // now build the swap using the updated leg data
    Swap::build(engineFactory);
}

} // namespace data
} // namespace ore
