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

/*! \file ored/portfolio/autocallable_01.hpp
    \brief autocallable_01 wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

class Autocallable_01 : public ScriptedTrade {
public:
    Autocallable_01() : ScriptedTrade("Autocallable_01") {}
    Autocallable_01(const Envelope& env, const string& notionalAmount, const string& determinationLevel,
                    const string& triggerLevel, const QuantLib::ext::shared_ptr<Underlying>& underlying, const string& position,
                    const string& payCcy, const ScheduleData& fixingDates, const ScheduleData& settlementDates,
                    const vector<string>& accumulationFactors, const string& cap)
        : ScriptedTrade("Autocallable_01", env), notionalAmount_(notionalAmount),
          determinationLevel_(determinationLevel), triggerLevel_(triggerLevel), position_(position), payCcy_(payCcy),
          underlying_(underlying), fixingDates_(fixingDates), settlementDates_(settlementDates),
          accumulationFactors_(accumulationFactors), cap_(cap) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields() override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    void initIndices();
    string notionalAmount_, determinationLevel_, triggerLevel_, position_, payCcy_;
    QuantLib::ext::shared_ptr<Underlying> underlying_;
    ScheduleData fixingDates_, settlementDates_;
    vector<string> accumulationFactors_;
    string cap_;
};

} // namespace data
} // namespace ore
