/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/nettingsetdetails.hpp
    \brief netting set details data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <map>
#include <ored/utilities/xmlutils.hpp>
#include <set>

using ore::data::XMLNode;
using ore::data::XMLSerializable;
using std::map;
using std::set;
using std::string;

namespace ore {
namespace data {

//! Serializable object holding netting set identification data
class NettingSetDetails : public XMLSerializable {
public:
    //! Default constructor
    NettingSetDetails()
        : nettingSetId_(string()), agreementType_(string()), callType_(string()), initialMarginType_(string()),
          legalEntityId_(string()){};

    //! Constructor with all fields
    NettingSetDetails(const string& nettingSetId, const string& agreementType = "", const string& callType = "",
                      const string& initialMarginType = "", const string& legalEntityId = "")
        : nettingSetId_(nettingSetId), agreementType_(agreementType), callType_(callType),
          initialMarginType_(initialMarginType), legalEntityId_(legalEntityId) {}

    //! Constructor to reconstruct NettingSetDetails from map (field name to field value)
    NettingSetDetails(const map<string, string>& nettingSetMap);

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& nettingSetId() const { return nettingSetId_; }
    const string& agreementType() const { return agreementType_; }
    const string& callType() const { return callType_; }
    const string& initialMarginType() const { return initialMarginType_; }
    const string& legalEntityId() const { return legalEntityId_; }
    //@}

    //! \name Utility
    //@{
    //! Check if the netting set details have been populated
    bool empty() const { return nettingSetId_.empty(); }
    bool emptyOptionalFields() const {
        return agreementType_.empty() && callType_.empty() && initialMarginType_.empty() && legalEntityId_.empty();
    }

    //! Returns a map representation of the object
    const map<string, string> mapRepresentation() const;

    //! Returns the XML field names of all the private members
    static const vector<string> fieldNames(bool includeOptionalFields = true);
    static const vector<string> optionalFieldNames();
    //@}

private:
    string nettingSetId_;
    string agreementType_;
    string callType_;
    string initialMarginType_;
    string legalEntityId_;
};

//! Comparison operators
bool operator<(const NettingSetDetails& lhs, const NettingSetDetails& rhs);
bool operator==(const NettingSetDetails& lhs, const NettingSetDetails& rhs);
bool operator!=(const NettingSetDetails& lhs, const NettingSetDetails& rhs);

//! Enable writing of netting set details
std::ostream& operator<<(std::ostream& out, const NettingSetDetails& nettingSetDetails);

} // namespace data
} // namespace ore
