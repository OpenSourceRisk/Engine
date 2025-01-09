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
#include <ored/portfolio/counterpartyinformation.hpp>
#include <ored/portfolio/counterpartycorrelationmatrix.hpp>

namespace ore {
namespace data {

//! Counterparty Manager
/*!
  This class is a container for a counterparty level information that is relevant
  for credit risk and credit risk capital calculation.
  \ingroup tradedata
*/
class CounterpartyManager : public ore::data::XMLSerializable {
public:
    //! default constructor
    CounterpartyManager() {}

    //! clears the manager of all data
    void reset();

    //! checks if the manager is empty
    const bool empty();

    //! checks if object named id exists in manager
    bool has(std::string id) const;

    //! adds a new CounterpartyInformation object to manager
    void add(const QuantLib::ext::shared_ptr<CounterpartyInformation>& nettingSet);

    //! adds a new CounterpartyInformation object to manager
    void addCorrelation(const std::string& cpty1, const std::string& cpty2,
                                              QuantLib::Real correlation);

    //! extracts a pointer to a CounterpartyInformation from manager
    QuantLib::ext::shared_ptr<CounterpartyInformation> get(std::string id) const;

    //! vector containing the ids of all objects stored in manager
    std::vector<std::string> uniqueKeys() const { return uniqueKeys_; }

    const QuantLib::ext::shared_ptr<CounterpartyCorrelationMatrix>& counterpartyCorrelations() const { return correlations_; }

    const std::map<std::string, const QuantLib::ext::shared_ptr<CounterpartyInformation>>& counterpartyInformation() const { return data_; }
    
    //! loads NettingSetDefinition object from XML
    void fromXML(ore::data::XMLNode* node) override;

    //! writes object to XML
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

private:
    std::map<std::string, const QuantLib::ext::shared_ptr<CounterpartyInformation>> data_;
    std::vector<std::string> uniqueKeys_;
    QuantLib::ext::shared_ptr<CounterpartyCorrelationMatrix> correlations_;
};
} // data
} // ore
