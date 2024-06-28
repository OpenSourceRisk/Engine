/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file portfolio/cashposition.hpp
    \brief Cash position data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradefactory.hpp>

namespace ore {
namespace data {

//! Serializable Cash position
/*!
  \ingroup tradedata
*/
class CashPosition : public Trade {
public:
    //! Default constructor
    CashPosition() : Trade("CashPosition"), amount_(0.0) {}
    //! Constructor
    CashPosition(const Envelope& env, const string& currency, double amount)
        : Trade("CashPosition", env), currency_(currency), amount_(amount) {}

    bool hasCashflows() const override { return false; }

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override { return amount_; }

    //! \name Inspectors
    //@{
    const string& currency() const { return currency_; }
    double amount() const { return amount_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    string currency_;
    double amount_;
};
} // namespace data
} // namespace ore
