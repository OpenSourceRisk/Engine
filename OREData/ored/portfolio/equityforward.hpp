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

using std::string;

namespace ore {
namespace data {

//! Serializable Equity Forward contract
/*!
\ingroup tradedata
*/
class EquityForward : public Trade {
public:
    EquityForward() : Trade("EquityForward") {}
    EquityForward(Envelope& env, string longShort, string name, string currency, double quantity, string maturityDate,
                  double strike)
        : Trade("EquityForward", env), longShort_(longShort), eqName_(name), currency_(currency), quantity_(quantity),
          maturityDate_(maturityDate), strike_(strike) {}

    void build(const boost::shared_ptr<EngineFactory>&);

    string longShort() { return longShort_; }
    string eqName() { return eqName_; }
    string currency() { return currency_; }
    double quantity() { return quantity_; }
    string maturityDate() { return maturityDate_; }
    double strike() { return strike_; }

    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);

private:
    string longShort_;
    string eqName_;
    string currency_;
    double quantity_;
    string maturityDate_;
    double strike_;
};
} // namespace data
} // namespace ore
