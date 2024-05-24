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

/*! \file ored/portfolio/nettingsetmanager.hpp
    \brief Manager class for repository of netting set details
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/nettingsetdefinition.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ql/shared_ptr.hpp>

namespace ore {
namespace data {

using ore::data::NettingSetDetails;

//! Netting Set Manager
/*!
  This class is a manager to store netting set definitions

  \ingroup tradedata
*/
class NettingSetManager : public XMLSerializable {
public:
    /*!
      default constructor
    */
    NettingSetManager() {}

    /*!
      clears the manager of all data
    */
    void reset();

    /*!
      checks if the manager is empty
    */
    const bool empty() const;

    /*!
      checks if at least one of the netting set definitions has calculateIMAmount = true
    */
    const bool calculateIMAmount() const;

    /*!
      returns the list of netting sets for which SIMM will be calculated as IM
    */
    const std::set<NettingSetDetails> calculateIMNettingSets() const;

    /*!
      checks if object named id exists in manager
    */
    bool has(const string& id) const;

    /*!
      checks if object with the given nettingSetDetails exists in manager
    */
    bool has(const NettingSetDetails& nettingSetDetails) const;

    /*!
      adds a new NettingSetDefinition object to manager
    */
    void add(const QuantLib::ext::shared_ptr<NettingSetDefinition>& nettingSet);

    /*!
      extracts a pointer to a NettingSetDefinition from manager
    */
    QuantLib::ext::shared_ptr<NettingSetDefinition> get(const string& id) const;
    QuantLib::ext::shared_ptr<NettingSetDefinition> get(const NettingSetDetails& nettingSetDetails) const;

    /*!
      vector containing the ids of all objects stored in manager
    */
    vector<NettingSetDetails> uniqueKeys() const { return uniqueKeys_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::map<NettingSetDetails, const QuantLib::ext::shared_ptr<NettingSetDefinition>>& nettingSetDefinitions() { return data_; }

private:
    map<NettingSetDetails, const QuantLib::ext::shared_ptr<NettingSetDefinition>> data_;
    vector<NettingSetDetails> uniqueKeys_;
};
} // namespace data
} // namespace ore
