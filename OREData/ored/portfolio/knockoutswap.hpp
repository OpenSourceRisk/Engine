/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/knockoutswap.hpp
    \brief knock out swap wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>

#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

using namespace ore::data;

class KnockOutSwap : public ScriptedTrade {
public:
    explicit KnockOutSwap(const std::string& tradeType = "KnockOutSwap") : ScriptedTrade(tradeType) {}
    KnockOutSwap(const std::vector<LegData>& legData, const BarrierData& barrierData,
                 const std::string& barrierStartDate)
        : legData_(legData), barrierData_(barrierData), barrierStartDate_(barrierStartDate) {}
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    std::vector<LegData> legData_;
    BarrierData barrierData_;
    std::string barrierStartDate_;
};

} // namespace data
} // namespace ore
