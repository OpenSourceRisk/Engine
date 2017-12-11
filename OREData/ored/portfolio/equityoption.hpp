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

//! Serializable Equity Option
/*!
  \ingroup tradedata
*/
class EquityOption : public Trade {
public:
    //! Default constructor
    EquityOption() : Trade("EquityOption"), strike_(0.0), quantity_(0.0) {}
    //! Constructor
    EquityOption(Envelope& env, OptionData option, string equityName, string currency, double strike, double quantity)
        : Trade("EquityOption", env), option_(option), eqName_(equityName), currency_(currency), strike_(strike),
          quantity_(quantity) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&);

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    const string& equityName() const { return eqName_; }
    const string& currency() const { return currency_; }
    double strike() const { return strike_; }
    double quantity() const { return quantity_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    OptionData option_;
    string eqName_;
    string currency_;
    double strike_;
    double quantity_;
};
} // namespace data
} // namespace ore
