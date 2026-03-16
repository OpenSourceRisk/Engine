/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#ifndef ored_xmlutils_i
#define ored_xmlutils_i

%{
// put c++ declarations here
using ore::data::XMLNode;
using ore::data::XMLDocument;
using ore::data::XMLSerializable;
%}

%shared_ptr(XMLSerializable)
class XMLSerializable {
public:
    virtual ~XMLSerializable();
    virtual void fromXML(XMLNode* node) = 0;
    virtual XMLNode* toXML(XMLDocument& doc) const = 0;

    void fromFile(const std::string& filename);
    void toFile(const std::string& filename) const;

    void fromXMLString(const std::string& xml);
    std::string toXMLString() const;
};

#endif
