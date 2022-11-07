/*
  Copyright (C) 2019 Quaternion Risk Manaement Ltd
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

#include <qle/instruments/genericswaption.hpp>

#include <ql/exercise.hpp>
#include <ql/shared_ptr.hpp>

namespace QuantExt {

GenericSwaption::GenericSwaption(const ext::shared_ptr<QuantLib::Swap>& swap, const ext::shared_ptr<Exercise>& exercise,
                                 Settlement::Type delivery, Settlement::Method settlementMethod)
    : Option(ext::shared_ptr<Payoff>(), exercise), swap_(swap), settlementType_(delivery),
      settlementMethod_(settlementMethod) {
    registerWith(swap_);
    swap_->alwaysForwardNotifications();
}

bool GenericSwaption::isExpired() const { return detail::simple_event(exercise_->dates().back()).hasOccurred(); }

void GenericSwaption::setupArguments(PricingEngine::arguments* args) const {
    swap_->setupArguments(args);
    Option::setupArguments(args);

    GenericSwaption::arguments* arguments = dynamic_cast<GenericSwaption::arguments*>(args);

    QL_REQUIRE(arguments != 0, "wrong argument type");

    arguments->swap = swap_;
    arguments->settlementType = settlementType_;
    arguments->settlementMethod = settlementMethod_;
    arguments->exercise = exercise_;
}

void GenericSwaption::fetchResults(const PricingEngine::results* r) const {
    Option::fetchResults(r);
    const GenericSwaption::results* results = dynamic_cast<const GenericSwaption::results*>(r);
    QL_ENSURE(results != 0, "wrong results type");
    underlyingValue_ = results->underlyingValue;
}

void GenericSwaption::results::reset() {
    Option::results::reset();
    underlyingValue = Null<Real>();
}

void GenericSwaption::arguments::validate() const {
    Swap::arguments::validate();
    QL_REQUIRE(swap, "underlying swap not set");
    QL_REQUIRE(exercise, "exercise not set");
    QuantLib::Settlement::checkTypeAndMethodConsistency(settlementType, settlementMethod);
}

} // namespace QuantExt
