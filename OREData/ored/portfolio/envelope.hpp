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

/*! \file ored/portfolio/envelope.hpp
    \brief trade envelope data model and serialization
    \ingroup openxva::portfolio
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <map>

using std::string;
using std::map;
using ore::data::XMLSerializable;
;
using ore::data::XMLDocument;
using ore::data::XMLNode;

namespace ore {
namespace data {

//! Serializable object holding generic trade data, reporting dimensions
/*!
  \ingroup tradedata
*/
class Envelope : public XMLSerializable {
public:
    //! Default constructor
    Envelope() {}

    //! Constructor without netting set, without additional fields
    Envelope(const string& counterparty, const string& nettingSetId = "")
        : counterparty_(counterparty), nettingSetId_(nettingSetId) {}

    // For some reason have a ctor with:
    // map<string, string>& additionalFields = map<string,string>()
    // fails under gcc, apparently it's a gcc bug! So to workaround we just have
    // 2 explict ctors.

    //! Constructor with netting set, additional fields
    Envelope(const string& counterparty, const map<string, string>& additionalFields)
        : counterparty_(counterparty), nettingSetId_(""), additionalFields_(additionalFields) {}

    //! Constructor without netting set, with additional fields
    Envelope(const string& counterparty, const string& nettingSetId, const map<string, string>& additionalFields)
        : counterparty_(counterparty), nettingSetId_(nettingSetId), additionalFields_(additionalFields) {}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Inspectors
    //@{
    const string& counterparty() const { return counterparty_; }
    const string& nettingSetId() const { return nettingSetId_; }
    const map<string, string>& additionalFields() const { return additionalFields_; }
    //@}
private:
    string counterparty_;
    string nettingSetId_;
    map<string, string> additionalFields_;
};
}
}
