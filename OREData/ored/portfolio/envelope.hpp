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
    \ingroup tradedata
*/

#pragma once

#include <map>
#include <ored/utilities/xmlutils.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/any.hpp>
#include <set>
#include <ored/portfolio/nettingsetdetails.hpp>

using ore::data::XMLNode;
using ore::data::XMLSerializable;
using ore::data::NettingSetDetails;
using std::map;
using std::set;
using std::string;

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

    //! Constructor with netting set id and portfolio ids, without additional fields
    Envelope(const string& counterparty, const string& nettingSetId,
             const set<string>& portfolioIds = set<string>())
        : counterparty_(counterparty), nettingSetDetails_(NettingSetDetails(nettingSetId)),
          portfolioIds_(portfolioIds) {}

    //! Constructor with netting set details and portfolio ids, without additional fields
    Envelope(const string& counterparty, const NettingSetDetails& nettingSetDetails = NettingSetDetails(),
             const set<string>& portfolioIds = set<string>())
        : counterparty_(counterparty), nettingSetDetails_(nettingSetDetails), portfolioIds_(portfolioIds) {}

    //! Constructor without netting set / portfolio ids, with additional fields
    Envelope(const string& counterparty, const map<string, string>& additionalFields)
        : counterparty_(counterparty), nettingSetDetails_(NettingSetDetails()) {
        for (const auto& addField : additionalFields)
            additionalFields_[addField.first] = addField.second;
    }

    //! Constructor with netting set, with additional fields
    Envelope(const string& counterparty, const string& nettingSetId, const map<string, string>& additionalFields,
             const set<string>& portfolioIds = set<string>())
        : counterparty_(counterparty), nettingSetDetails_(NettingSetDetails(nettingSetId)), portfolioIds_(portfolioIds) {
        for (const auto& addField : additionalFields)
            additionalFields_[addField.first] = addField.second;
    }

    //! Constructor with netting set details, with additional fields
    Envelope(const string& counterparty, const NettingSetDetails& nettingSetDetails,
             const map<string, string>& additionalFields, const set<string>& portfolioIds = set<string>())
        : counterparty_(counterparty), nettingSetDetails_(nettingSetDetails), portfolioIds_(portfolioIds) {
        for (const auto& addField : additionalFields)
            additionalFields_[addField.first] = addField.second;
    }

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const string& counterparty() const { return counterparty_; }
    const string& nettingSetId() const { return nettingSetDetails_.nettingSetId(); }
    const NettingSetDetails nettingSetDetails() { return nettingSetDetails_; }
    const set<string>& portfolioIds() const { return portfolioIds_; }
    const map<string, string> additionalFields() const {
        map<string, string> stringAddFields;
        for (const auto& f : additionalFields_)
            if (f.second.type() == typeid(string))
                stringAddFields[f.first] = boost::any_cast<string>(f.second);
        return stringAddFields;
    }
    const map<string, boost::any>& fullAdditionalFields() const { return additionalFields_; }
    //@}

    //! \name Utility
    //@{
    //! Check if the envelope has been populated
    bool empty() const { return counterparty_ == ""; }
    //! Check if the netting set details have been populated
    bool hasNettingSetDetails() const { return !nettingSetDetails_.empty(); }
    //@}

private:
    string counterparty_;
    NettingSetDetails nettingSetDetails_;
    set<string> portfolioIds_;
    map<string, boost::any> additionalFields_;
};

} // namespace data
} // namespace ore
