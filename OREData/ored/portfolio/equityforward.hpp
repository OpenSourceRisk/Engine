/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/equityoption.hpp
\brief Equity Option data model and serialization
\ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable Equity Forward contract
/*!
\ingroup tradedata
*/
class EquityForward : public Trade {
public:
    EquityForward() : Trade("EquityForward") {}
    EquityForward(Envelope& env, string longShort, EquityUnderlying equityUnderlying, string currency, double quantity,
                  string maturityDate, double strike)
        : Trade("EquityForward", env), longShort_(longShort), equityUnderlying_(equityUnderlying), currency_(currency),
          quantity_(quantity), maturityDate_(maturityDate), strike_(strike) {}

    void build(const boost::shared_ptr<EngineFactory>&) override;

    //! Add underlying Equity names
    std::map<AssetClass, std::set<std::string>> underlyingIndices() const override;

    string longShort() { return longShort_; }
    const string& eqName() const { return equityUnderlying_.name(); }
    string currency() { return currency_; }
    double quantity() { return quantity_; }
    string maturityDate() { return maturityDate_; }
    double strike() { return strike_; }

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;

private:
    string longShort_;
    EquityUnderlying equityUnderlying_;
    string currency_;
    double quantity_;
    string maturityDate_;
    double strike_;
};
} // namespace data
} // namespace ore
