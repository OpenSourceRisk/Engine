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

/*! \file ored/portfolio/europeanoptionbarrier.hpp
    \brief European option with barrier wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/scriptedtrade.hpp>

namespace ore {
namespace data {

using namespace ore::data;

class EuropeanOptionBarrier : public ScriptedTrade {
public:
    explicit EuropeanOptionBarrier(const QuantLib::ext::shared_ptr<Conventions>& conventions = nullptr)
        : ScriptedTrade("EuropeanOptionBarrier") {}
    EuropeanOptionBarrier(const Envelope& env, const string& quantity, const string& putCall, const string& longShort,
                          const string& strike, const string& premiumAmount, const string& premiumCurrency,
                          const string& premiumDate, const string& optionExpiry,
                          const QuantLib::ext::shared_ptr<Underlying>& optionUnderlying,
                          const QuantLib::ext::shared_ptr<Underlying>& barrierUnderlying, const string& barrierLevel,
                          const string& barrierType, const string& barrierStyle, const string& settlementDate,
                          const string& payCcy, const ScheduleData& barrierSchedule,
                          const QuantLib::ext::shared_ptr<Conventions>& conventions = nullptr)
        : ScriptedTrade("EuropeanOptionBarrier", env), quantity_(quantity), putCall_(putCall), longShort_(longShort),
          strike_(strike), premiumAmount_(premiumAmount), premiumCurrency_(premiumCurrency), premiumDate_(premiumDate),
          optionExpiry_(optionExpiry), optionUnderlying_(optionUnderlying), barrierUnderlying_(barrierUnderlying),
          barrierLevel_(barrierLevel), barrierType_(barrierType), barrierStyle_(barrierStyle),
          barrierSchedule_(barrierSchedule), settlementDate_(settlementDate), payCcy_(payCcy) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    void initIndices();
    string quantity_, putCall_, longShort_, strike_, premiumAmount_, premiumCurrency_, premiumDate_, optionExpiry_;
    QuantLib::ext::shared_ptr<Underlying> optionUnderlying_, barrierUnderlying_;
    string barrierLevel_, barrierType_, barrierStyle_;
    ScheduleData barrierSchedule_;
    string settlementDate_, payCcy_;
};

} // namespace data
} // namespace ore
