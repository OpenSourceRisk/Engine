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

/*! \file portfolio/equityswap.hpp
    \brief Equity Swap data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {

class EquityIdentifier : public XMLSerializable {
public:
    //! Deault constructor
    EquityIdentifier() {}

    //! Constructor with vector of LegData
    EquityIdentifier(const std::string& equityName) : equityName_(equityName) {};

    //! Constructor with two legs
    EquityIdentifier(const std::string& identifierType, const std::string& identifierName, const std::string& currency,
        const std::string& exchange) : identifierType_(identifierType), identifierName_(identifierName),
        currency_(currency), exchange_(exchange) {};

    const std::string& equityName() const { return equityName_; };
    const std::string& currency() const { return currency_; };
    const std::string& exchange() const { return exchange_; };


    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

private:
    std::string equityName_;
    std::string identifierType_;
    std::string identifierName_;
    std::string currency_;
    std::string exchange_;
};

} // namespace data
} // namespace ore

