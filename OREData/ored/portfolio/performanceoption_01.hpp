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

/*! \file ored/portfolio/performanceoption_01.hpp
    \brief performance option wrapper for scripted trade
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/scriptedtrade.hpp>

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

using namespace ore::data;

class PerformanceOption_01 : public ScriptedTrade {
public:
    explicit PerformanceOption_01(const QuantLib::ext::shared_ptr<Conventions>& conventions = nullptr)
        : ScriptedTrade("PerformanceOption_01") {}
    PerformanceOption_01(const Envelope& env, const string& notionalAmount, const string& participationRate,
                         const string& valuationDate, const string& settlementDate,
                         const vector<QuantLib::ext::shared_ptr<Underlying>>& underlyings, const vector<string>& strikePrices,
                         const string& strike, const bool strikeIncluded, const string& position, const string& payCcy,
                         const QuantLib::ext::shared_ptr<Conventions>& conventions = nullptr)
        : ScriptedTrade("PerformanceOption_01", env), notionalAmount_(notionalAmount),
          participationRate_(participationRate), valuationDate_(valuationDate), settlementDate_(settlementDate),
          underlyings_(underlyings), strikePrices_(strikePrices), strike_(strike), strikeIncluded_(strikeIncluded),
          position_(position), payCcy_(payCcy) {
        initIndices();
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields() override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    void initIndices();
    string notionalAmount_, participationRate_, valuationDate_, settlementDate_;
    vector<QuantLib::ext::shared_ptr<Underlying>> underlyings_;
    vector<string> strikePrices_;
    string strike_;
    bool strikeIncluded_ = true;
    string position_, payCcy_;
};

} // namespace data
} // namespace ore
