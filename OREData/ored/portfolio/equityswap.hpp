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

/*! \file portfolio/equityswap.hpp
    \brief Equity Swap data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/trade.hpp>

using std::string;

namespace ore {
namespace data {

//! Serializable Equity Forward contract
/*!
\ingroup tradedata
*/
class EquitySwap: public Swap {
public:
    //! Deault constructor
    EquitySwap() : Swap("EquitySwap") {}

    //! Constructor with vector of LegData
    EquitySwap(const Envelope& env, const vector<LegData>& legData);

    //! Constructor with two legs
    EquitySwap(const Envelope& env, const LegData& leg0, const LegData& leg1);

    void checkEquitySwap(const vector<LegData>& legData);

    //! Build QuantLib/QuantExt instrument, link pricing engine
    //virtual void build(const boost::shared_ptr<EngineFactory>&);

    //! \name Serialisation
    //@{
    //virtual void fromXML(XMLNode* node);
    //virtual XMLNode* toXML(XMLDocument& doc);
    //@}

};
} // namespace data
} // namespace ore
