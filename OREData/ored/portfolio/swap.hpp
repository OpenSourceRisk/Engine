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

/*! \file portfolio/swap.hpp
    \brief Swap trade data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/legdata.hpp>

namespace ore {
namespace data {

//! Serializable Swap, Single and Cross Currency
/*!
  \ingroup tradedata
*/
class Swap : public Trade {
public:
    //! Deault constructor
    Swap() : Trade("Swap") {}

    //! Constructor with vector of LegData
    Swap(Envelope env, vector<LegData>& legData) : Trade("Swap", env), legData_(legData) {}

    //! Constructor with two legs
    Swap(Envelope env, LegData leg0, LegData leg1) : Trade("Swap", env), legData_({leg0, leg1}) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    virtual void build(const boost::shared_ptr<EngineFactory>&);

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Inspectors
    //@{
    const vector<LegData>& legData() { return legData_; }
    //@}
private:
    vector<LegData> legData_;
};
}
}
