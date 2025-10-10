/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <algorithm>
#include <qle/instruments/multiccycompositeinstrument.hpp>

namespace QuantExt {
using namespace QuantLib;

void MultiCcyCompositeInstrument::add(const ext::shared_ptr<Instrument>& instrument, Real multiplier,
                                      const Handle<Quote>& fx) {
    QL_REQUIRE(instrument, "null instrument provided");
    components_.push_back(std::make_tuple(instrument, multiplier, fx));
    registerWith(instrument);
    registerWith(fx);
    update();
    // When we ask for the NPV of an expired composite, the
    // components are not recalculated and thus wouldn't forward
    // later notifications according to the default behavior of
    // LazyObject instances.  This means that even if the
    // evaluation date changes so that the composite is no longer
    // expired, the instrument wouldn't be notified and thus it
    // wouldn't recalculate.  To avoid this, we override the
    // default behavior of the components.
    instrument->alwaysForwardNotifications();
}

void MultiCcyCompositeInstrument::subtract(const ext::shared_ptr<Instrument>& instrument, Real multiplier,
                                           const Handle<Quote>& fx) {
    add(instrument, -multiplier, fx);
}

bool MultiCcyCompositeInstrument::isExpired() const {
    return !std::any_of(components_.begin(), components_.end(),
                        [](auto const& c) { return !std::get<0>(c)->isExpired(); });
}

void MultiCcyCompositeInstrument::performCalculations() const {
    NPV_ = 0.0;
    for (auto const& [inst, mult, fx] : components_) {
        NPV_ += mult * fx->value() * inst->NPV();
    }
    updateAdditionalResults();
}

void MultiCcyCompositeInstrument::deepUpdate() {
    std::for_each(components_.begin(), components_.end(), [](component& c) { std::get<0>(c)->deepUpdate(); });
    update();
}

void MultiCcyCompositeInstrument::updateAdditionalResults() const {
    additionalResults_.clear();
    Size counter = 0;
    for (auto const& [inst, mult, fx] : components_) {
        std::string postFix = "_" + std::to_string(counter++);
        const auto& cmpResults = inst->additionalResults();
        for (auto const& r : cmpResults) {
            additionalResults_[r.first + postFix] = r.second;
        }
        additionalResults_["__multiplier" + postFix] = mult;
        additionalResults_["__fx_conversion" + postFix] = fx->value();
        additionalResults_["__npv" + postFix] = inst->NPV();
    }
}

} // namespace QuantExt
