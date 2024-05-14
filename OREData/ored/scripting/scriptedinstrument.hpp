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

/*! \file ored/scripting/scriptedinstrument.hpp
    \brief scripted instrument
*/

#pragma once

#include <ql/event.hpp>
#include <ql/instrument.hpp>

namespace QuantExt {
class ScriptedInstrumentPricingEngine;

class ScriptedInstrument : public QuantLib::Instrument {
public:
    explicit ScriptedInstrument(const QuantLib::Date& lastRelevantDate);
    class arguments : public QuantLib::PricingEngine::arguments {
        void validate() const override {}
    };
    using results = QuantLib::Instrument::results;
    using engine = QuantLib::GenericEngine<arguments, results>;
    bool isExpired() const override;
    void setupArguments(QuantLib::PricingEngine::arguments*) const override {}
    bool lastCalculationWasValid() const;
    QuantLib::ext::shared_ptr<PricingEngine> pricingEngine() const { return engine_; }

private:
    const QuantLib::Date lastRelevantDate_;
};

} // namespace QuantExt
