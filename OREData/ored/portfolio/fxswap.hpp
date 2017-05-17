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

/*! \file portfolio/fxswap.hpp
    \brief FX Swap data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>

using std::string;

namespace ore {
namespace data {

//! Serializable FX Swap
/*!
  \ingroup tradedata
*/
class FxSwap : public Trade {
public:
    //! Default constructor
    FxSwap()
        : Trade("FxSwap"), nearBoughtAmount_(0.0), nearSoldAmount_(0.0), farBoughtAmount_(0.0), farSoldAmount_(0.0) {}
    //! Constructor
    FxSwap(Envelope& env, string nearDate, string farDate, string nearBoughtCurrency, double nearBoughtAmount,
           string nearSoldCurrency, double nearSoldAmount, double farBoughtAmount, double farSoldAmount)
        : Trade("FxSwap", env), nearDate_(nearDate), farDate_(farDate), nearBoughtCurrency_(nearBoughtCurrency),
          nearBoughtAmount_(nearBoughtAmount), nearSoldCurrency_(nearSoldCurrency), nearSoldAmount_(nearSoldAmount),
          farBoughtAmount_(farBoughtAmount), farSoldAmount_(farSoldAmount) {}
    /*! Constructs a composite pricing engine of two FX forward pricing engines.
    One with the near amounts as notionals, the other with the far amounts.
    NPV is the total npv of these trades.
    */

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const boost::shared_ptr<EngineFactory>&);

    //! \name Inspectors
    //@{
    string nearDate() { return nearDate_; }
    string farDate() { return farDate_; }
    string nearBoughtCurrency() { return nearBoughtCurrency_; }
    double nearBoughtAmount() { return nearBoughtAmount_; }
    string nearSoldCurrency() { return nearSoldCurrency_; }
    double nearSoldAmount() { return nearSoldAmount_; }
    double farBoughtAmount() { return farBoughtAmount_; }
    double farSoldAmount() { return farSoldAmount_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    string nearDate_;
    string farDate_;
    string nearBoughtCurrency_; // farBoughtCurrency==nearSoldCurrency
    double nearBoughtAmount_;
    string nearSoldCurrency_;
    double nearSoldAmount_;
    double farBoughtAmount_;
    double farSoldAmount_;
};
} // namespace data
} // namespace ore
