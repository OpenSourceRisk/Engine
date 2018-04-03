/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! 
  \file qlw/trades/varswap.hpp  
  \brief Variance Swap data model and serialization
  \ingroup Wrap 
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
