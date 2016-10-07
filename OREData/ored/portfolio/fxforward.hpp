/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file portfolio/fxforward.hpp
    \brief FX Forward data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>

namespace openriskengine {
namespace data {

//! Serializable FX Forward
/*!
  \ingroup tradedata
*/
class FxForward : public Trade {
public:
    //! Default constructor
    FxForward() : Trade("FxForward"), boughtAmount_(0.0), soldAmount_(0.0) {}
    //! Constructor
    FxForward(Envelope& env, string maturityDate, string boughtCurrency, double boughtAmount, string soldCurrency,
              double soldAmount)
        : Trade("FxForward", env), maturityDate_(maturityDate), boughtCurrency_(boughtCurrency),
          boughtAmount_(boughtAmount), soldCurrency_(soldCurrency), soldAmount_(soldAmount) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&);

    //! \name Inspectors
    //@{
    const string& maturityDate() const { return maturityDate_; }
    const string& boughtCurrency() const { return boughtCurrency_; }
    double boughtAmount() const { return boughtAmount_; }
    const string& soldCurrency() const { return soldCurrency_; }
    double soldAmount() const { return soldAmount_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    string maturityDate_;
    string boughtCurrency_;
    double boughtAmount_;
    string soldCurrency_;
    double soldAmount_;
};
}
}
