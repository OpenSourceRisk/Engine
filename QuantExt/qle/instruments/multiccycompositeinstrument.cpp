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

#include <qle/instruments/multiccycompositeinstrument.hpp>

namespace QuantExt {
using namespace QuantLib;

void MultiCcyCompositeInstrument::add(const ext::shared_ptr<Instrument>& instrument, Real multiplier,
                                      const Handle<Quote>& fx) {
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
    for (const_iterator i = components_.cbegin(); i != components_.end(); ++i) {
        if (!std::get<0>(*i)->isExpired())
            return false;
    }
    return true;
}

void MultiCcyCompositeInstrument::performCalculations() const {
    NPV_ = 0.0;
    additionalResults_.clear();
    Size counter = 0;
    for (const_iterator i = components_.cbegin(); i != components_.end(); ++i, ++counter) {
        // npv += npv * fx * multiplier
        NPV_ += std::get<1>(*i) * std::get<2>(*i)->value() * std::get<0>(*i)->NPV();
        for (auto const& k : std::get<0>(*i)->additionalResults()) {
            additionalResults_[k.first + "_" + std::to_string(counter)] = k.second;
        }
        additionalResults_["__multiplier_" + std::to_string(counter)] = std::get<1>(*i);
        additionalResults_["__fx_conversion_" + std::to_string(counter)] = std::get<2>(*i)->value();
    }
}

void MultiCcyCompositeInstrument::deepUpdate() {
    for (const_iterator i = components_.cbegin(); i != components_.end(); ++i) {
        std::get<0>(*i)->deepUpdate();
    }
    update();
}

} // namespace QuantExt
