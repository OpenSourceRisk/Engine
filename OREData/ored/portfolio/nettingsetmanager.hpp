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

#include <boost/shared_ptr.hpp>

namespace ore {
namespace data {

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
      checks if object named id exists in manager
    */
    bool has(string id) const;

    /*!
      adds a new NettingSetDefinition object to manager
    */
    void add(const boost::shared_ptr<NettingSetDefinition>& nettingSet);

    /*!
      extracts a pointer to a NettingSetDefinition from manager
    */
    boost::shared_ptr<NettingSetDefinition> get(string id) const;

    /*!
      vector containing the ids of all objects stored in manager
    */
    vector<string> uniqueKeys() const { return uniqueKeys_; }

    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);

private:
    map<string, const boost::shared_ptr<NettingSetDefinition>> data_;
    vector<string> uniqueKeys_;
};
} // namespace data
} // namespace ore
