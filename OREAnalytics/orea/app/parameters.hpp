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

/*! \file orea/app/parameters.hpp
    \brief Open Risk Engine setup and analytics choice
    \ingroup app
*/

#pragma once

#include <map>
#include <vector>

#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace analytics {
using namespace ore::data;
using std::map;
using std::string;

//! Provides the input data and references to input files used in OREApp
/*! \ingroup app
 */
class Parameters : public XMLSerializable {
public:
    Parameters() {}

    void clear();
    void fromFile(const string&);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    bool hasGroup(const string& groupName) const;
    bool has(const string& groupName, const string& paramName) const;
    string get(const string& groupName, const string& paramName, bool fail = true) const;
    const map<string, string>& data(const string& groupName) const;
    const map<string, string>& markets() const;
    
    void log();

private:
    map<string, map<string, string>> data_;
};
} // namespace analytics
} // namespace ore
