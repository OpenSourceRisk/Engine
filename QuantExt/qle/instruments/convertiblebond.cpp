/*
 Copyright (C) 2005, 2006 Theo Boafo
 Copyright (C) 2006, 2007 StatPro Italia srl

 Copyright (C) 2020 Quaternion Risk Managment Ltd

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

#include <qle/instruments/convertiblebond.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>

#include <ql/utilities/null_deleter.hpp>

#include <iostream>

namespace QuantExt {

ConvertibleBond::ConvertibleBond(Natural settlementDays, const Calendar& calendar, const Date& issueDate,
                                 const Leg& coupons, const QuantLib::ext::shared_ptr<Exercise>& exercise,
                                 const Real conversionRatio, const DividendSchedule& dividends,
                                 const CallabilitySchedule& callability)
    : Bond(settlementDays, calendar, issueDate, coupons), exercise_(exercise), conversionRatio_(conversionRatio),
      dividends_(dividends), callability_(callability) {

    if (!callability.empty()) {
        QL_REQUIRE(callability.back()->date() <= maturityDate_, "last callability date (" << callability.back()->date()
                                                                                          << ") later than maturity ("
                                                                                          << maturityDate_ << ")");
    }

    QL_REQUIRE(exercise_, "no exercise for conversion given");
    QL_REQUIRE(!exercise_->dates().empty(), "exercise does not contain any conversion dates");

    option_ = QuantLib::ext::make_shared<option>(this);
}

void ConvertibleBond::performCalculations() const {
    option_->setPricingEngine(engine_);
    NPV_ = settlementValue_ = option_->NPV();
    additionalResults_ = option_->additionalResults();
    errorEstimate_ = Null<Real>();
}

ConvertibleBond::option::option(const ConvertibleBond* bond)
    : OneAssetOption(QuantLib::ext::shared_ptr<StrikedTypePayoff>(
                         new PlainVanillaPayoff(Option::Call, bond->notionals().front() / bond->conversionRatio())),
                     bond->exercise()),
      bond_(bond) {
    registerWith(QuantLib::ext::shared_ptr<ConvertibleBond>(const_cast<ConvertibleBond*>(bond), null_deleter()));
}

bool ConvertibleBond::option::isExpired() const { return bond_->isExpired(); }

void ConvertibleBond::option::setupArguments(PricingEngine::arguments* args) const {

    OneAssetOption::setupArguments(args);

    ConvertibleBond::option::arguments* moreArgs = dynamic_cast<ConvertibleBond::option::arguments*>(args);
    QL_REQUIRE(moreArgs != 0, "wrong argument type");

    moreArgs->conversionRatio = bond_->conversionRatio();
    moreArgs->conversionValue = close_enough(bond_->conversionRatio(), 0.0)
                                    ? Null<Real>()
                                    : bond_->notionals().front() / bond_->conversionRatio();

    Date settlement = bond_->settlementDate();

    Size n = bond_->callability().size();
    moreArgs->callabilityDates.clear();
    moreArgs->callabilityTypes.clear();
    moreArgs->callabilityPrices.clear();
    moreArgs->callabilityTriggers.clear();
    moreArgs->callabilityDates.reserve(n);
    moreArgs->callabilityTypes.reserve(n);
    moreArgs->callabilityPrices.reserve(n);
    moreArgs->callabilityTriggers.reserve(n);
    for (Size i = 0; i < n; i++) {
        if (!bond_->callability()[i]->hasOccurred(settlement, false)) {
            moreArgs->callabilityTypes.push_back(bond_->callability()[i]->type());
            moreArgs->callabilityDates.push_back(bond_->callability()[i]->date());
            moreArgs->callabilityPrices.push_back(bond_->callability()[i]->price().amount() *
                                                  bond_->notional(bond_->callability()[i]->date()));
            if (bond_->callability()[i]->price().type() == Bond::Price::Clean)
                moreArgs->callabilityPrices.back() += bond_->accruedAmount(bond_->callability()[i]->date()) / 100.0 *
                                                      bond_->notional(bond_->callability()[i]->date());
            QuantLib::ext::shared_ptr<SoftCallability> softCall =
                QuantLib::ext::dynamic_pointer_cast<SoftCallability>(bond_->callability()[i]);
            if (softCall)
                moreArgs->callabilityTriggers.push_back(softCall->trigger());
            else
                moreArgs->callabilityTriggers.push_back(Null<Real>());
        }
    }

    const Leg& cashflows = bond_->cashflows();

    moreArgs->cashflowDates.clear();
    moreArgs->cashflowAmounts.clear();
    for (Size i = 0; i < cashflows.size(); i++) {
        if (!cashflows[i]->hasOccurred(settlement, false)) {
            moreArgs->cashflowDates.push_back(cashflows[i]->date());
            moreArgs->cashflowAmounts.push_back(cashflows[i]->amount());
        }
    }

    moreArgs->notionals.clear();
    moreArgs->notionalDates.clear();
    Real currentNotional = 0.0;
    for (std::vector<QuantLib::ext::shared_ptr<CashFlow>>::const_reverse_iterator r = bond_->redemptions().rbegin();
         r != bond_->redemptions().rend(); ++r) {
        moreArgs->notionals.insert(moreArgs->notionals.begin(), currentNotional);
        moreArgs->notionalDates.insert(moreArgs->notionalDates.begin(), (*r)->date());
        currentNotional += (*r)->amount();
    }
    moreArgs->notionals.insert(moreArgs->notionals.begin(), currentNotional);
    moreArgs->notionalDates.insert(moreArgs->notionalDates.begin(), settlement);

    moreArgs->dividends.clear();
    moreArgs->dividendDates.clear();
    for (Size i = 0; i < bond_->dividends().size(); i++) {
        if (!bond_->dividends()[i]->hasOccurred(settlement, false)) {
            moreArgs->dividends.push_back(bond_->dividends()[i]);
            moreArgs->dividendDates.push_back(bond_->dividends()[i]->date());
        }
    }

    moreArgs->issueDate = bond_->issueDate();
    moreArgs->settlementDate = bond_->settlementDate();
    moreArgs->settlementDays = bond_->settlementDays();
    moreArgs->maturityDate = bond_->maturityDate();
}

void ConvertibleBond::option::arguments::validate() const {

    OneAssetOption::arguments::validate();

    QL_REQUIRE(conversionRatio != Null<Real>(), "null conversion ratio");
    QL_REQUIRE(conversionRatio > 0.0 || close_enough(conversionRatio, 0.0),
               "non-negative conversion ratio required: " << conversionRatio << " not allowed");

    QL_REQUIRE(settlementDate != Date(), "null settlement date");

    QL_REQUIRE(settlementDays != Null<Natural>(), "null settlement days");

    QL_REQUIRE(callabilityDates.size() == callabilityTypes.size(), "different number of callability dates and types");
    QL_REQUIRE(callabilityDates.size() == callabilityPrices.size(), "different number of callability dates and prices");
    QL_REQUIRE(callabilityDates.size() == callabilityTriggers.size(),
               "different number of callability dates and triggers");

    QL_REQUIRE(cashflowDates.size() == cashflowAmounts.size(), "different number of coupon dates and amounts");

    QL_REQUIRE(exercise->lastDate() <= maturityDate, "last conversion date (" << exercise->lastDate()
                                                                              << ") must not be after bond maturity ("
                                                                              << maturityDate << ")");
}

} // namespace QuantExt
