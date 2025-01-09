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

#pragma once
#include <ored/utilities/xmlutils.hpp>
#include <ql/utilities/null.hpp>

namespace ore {
namespace data {

//! Counterparty Information
/*!
  This class is a container for information on a counterparty

  \ingroup tradedata
*/
class CounterpartyCorrelationMatrix : public ore::data::XMLSerializable {
public:
    CounterpartyCorrelationMatrix(ore::data::XMLNode* node);

    CounterpartyCorrelationMatrix() {}

    void addCorrelation(const std::string& cpty1, const std::string& cpty2,
                                              QuantLib::Real correlation);

    //! loads NettingSetDefinition object from XML
    void fromXML(ore::data::XMLNode* node) override;

    //! writes object to XML
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;


    QuantLib::Real lookup(const std::string& f1, const std::string& f2);
    
private:
    typedef std::pair<std::string, std::string> key;
    key buildkey(const std::string& f1In, const std::string& f2In);

    std::map<key, QuantLib::Real> data_;
};


} // data
} // ore
