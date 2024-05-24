/*
  Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/equitydigitaloption.hpp
    \brief EQ Digital Option data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/equityderivative.hpp>
#include <ored/portfolio/optiondata.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable EQ Digital Option
/*!
  \ingroup tradedata
*/
class EquityDigitalOption : public EquitySingleAssetDerivative {
public:
    //! Default constructor
    EquityDigitalOption()
        : ore::data::Trade("EquityDigitalOption"), EquitySingleAssetDerivative("") {}
    //! Constructor
    EquityDigitalOption(Envelope& env, OptionData option, double strike, const string& payoffCurrency, double payoffAmount,
                    const EquityUnderlying& equityUnderlying, double quantity)
        : ore::data::Trade("EquityDigitalOption", env),
          EquitySingleAssetDerivative("", env, equityUnderlying), option_(option),
          strike_(strike), payoffCurrency_(payoffCurrency), payoffAmount_(payoffAmount), quantity_(quantity) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    double strike() const { return strike_; }
    const string& payoffCurrency() const { return payoffCurrency_; }
    double payoffAmount() const { return payoffAmount_; }
    double quantity() const { return quantity_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    OptionData option_;
    Real strike_;
    string payoffCurrency_;
    Real payoffAmount_;
    Real quantity_;
};
} // namespace data
} // namespace oreplus
