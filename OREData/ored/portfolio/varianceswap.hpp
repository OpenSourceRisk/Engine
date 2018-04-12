/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/varianceswap.hpp
    \brief variance swap representation
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/trade.hpp>

using std::string;

namespace ore {
namespace data {

class VarSwap : public Trade {
public:
    VarSwap() : Trade("VarSwap") {}
    VarSwap(Envelope& env,
        string longShort,
        string eqName,
        string currency,
        double strike,
        double notional,
        string startDate,
        string endDate)
        : Trade("VarSwap", env),
        longShort_(longShort),
        eqName_(eqName),
        currency_(currency),
        strike_(strike),
        notional_(notional),
        startDate_(startDate),
        endDate_(endDate) {}

    void build(const boost::shared_ptr<EngineFactory>&);

    string longShort() { return longShort_; }
    string eqName() { return eqName_; }
    string currency() { return currency_; }
    double strike() { return strike_; }
    double notional() { return notional_; }
    string startDate() { return startDate_; }
    string endDate() { return endDate_; }
    string calendar() { return calendar_; }

    virtual void fromXML(XMLNode *node);
    virtual XMLNode* toXML(XMLDocument& doc);
private:
    string longShort_;
    string eqName_;
    string currency_;
    double strike_;
    double notional_;
    string startDate_;
    string endDate_;
    string calendar_;
};
}   // namespace data
}   // namespace ore
