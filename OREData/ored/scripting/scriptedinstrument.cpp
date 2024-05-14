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

#include <ored/scripting/engines/scriptedinstrumentpricingengine.hpp>
#include <ored/scripting/engines/scriptedinstrumentpricingenginecg.hpp>
#include <ored/scripting/scriptedinstrument.hpp>

namespace QuantExt {

ScriptedInstrument::ScriptedInstrument(const QuantLib::Date& lastRelevantDate) : lastRelevantDate_(lastRelevantDate) {
}

bool ScriptedInstrument::isExpired() const { return QuantLib::detail::simple_event(lastRelevantDate_).hasOccurred(); }

bool ScriptedInstrument::lastCalculationWasValid() const {
    if (auto res = QuantLib::ext::dynamic_pointer_cast<ore::data::ScriptedInstrumentPricingEngine>(engine_)) {
        return res->lastCalculationWasValid();
    } else if (auto res = QuantLib::ext::dynamic_pointer_cast<ore::data::ScriptedInstrumentPricingEngineCG>(engine_)) {
        return res->lastCalculationWasValid();
    } else {
        QL_FAIL(
            "internal error: could not cast to ScriptedInstrumentPricingEngine or ScriptedInstrumentPricingEngineCG");
    }
}

} // namespace QuantExt
