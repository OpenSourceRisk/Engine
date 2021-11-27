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
#include <set>

namespace ore {
namespace data {
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using std::map;
using std::set;
using std::string;

//! Serializable object holding netting set identification data
class NettingSetDetails : public XMLSerializable {
public:
    typedef boost::tuple<string, string, string, string, string> Key;

    //! Default constructor
    NettingSetDetails()
        : nettingSetId_(string()), agreementType_(string()), callType_(string()), initialMarginType_(string()),
          legalEntityId_(string()){};

    //! Constructor with all fields
    NettingSetDetails(const string& nettingSetId, const string& agreementType = "", const string& callType = "",
                      const string& initialMarginType = "", const string& legalEntityId = "")
        : nettingSetId_(nettingSetId), agreementType_(agreementType), callType_(callType),
          initialMarginType_(initialMarginType), legalEntityId_(legalEntityId) {}

    //! Constructor to reconstruct NettingSetDetails from from NettingSetDetails::Key
    NettingSetDetails(const NettingSetDetails::Key& nettingSetKey);

    //! Constructor to reconstruct NettingSetDetails from map (field name to field value)
    NettingSetDetails(const map<string, string>& nettingSetMap);

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
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
    //! Returns the object members as a tuple for use as a map key
    Key key() const {
        return boost::make_tuple(nettingSetId(), agreementType(), callType(), initialMarginType(), legalEntityId());
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

//! Serializable object holding generic trade data, reporting dimensions
/*!
  \ingroup tradedata
*/
class Envelope : public XMLSerializable {
public:
    //! Default constructor
    Envelope() {}

    //! Constructor with netting set details and portfolio ids, without additional fields
    Envelope(const string& counterparty, const string& nettingSetId = "",
             const NettingSetDetails& nettingSetDetails = NettingSetDetails(),
             const set<string>& portfolioIds = set<string>())
        : counterparty_(counterparty), nettingSetId_(nettingSetId), nettingSetDetails_(nettingSetDetails),
          portfolioIds_(portfolioIds) {}

    // For some reason have a ctor with:
    // map<string, string>& additionalFields = map<string,string>()
    // fails under gcc, apparently it's a gcc bug! So to workaround we just have
    // 2 explict ctors.

    //! Constructor without netting set / portfolio ids, with additional fields
    Envelope(const string& counterparty, const map<string, string>& additionalFields)
        : counterparty_(counterparty), nettingSetId_(""), nettingSetDetails_(NettingSetDetails()),
          additionalFields_(additionalFields) {}

    //! Constructor with netting set / portfolio ids, with additional fields
    Envelope(const string& counterparty, const string& nettingSetId, const map<string, string>& additionalFields,
             const set<string>& portfolioIds = set<string>())
        : counterparty_(counterparty), nettingSetId_(nettingSetId), nettingSetDetails_(NettingSetDetails()),
          portfolioIds_(portfolioIds), additionalFields_(additionalFields) {}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const string& counterparty() const { return counterparty_; }
    const string& nettingSetId() const {
        return (nettingSetDetails_.empty() ? nettingSetId_ : nettingSetDetails_.nettingSetId());
    }
    const NettingSetDetails nettingSetDetails() { return nettingSetDetails_; }
    const set<string>& portfolioIds() const { return portfolioIds_; }
    const map<string, string>& additionalFields() const { return additionalFields_; }
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
    string nettingSetId_;
    NettingSetDetails nettingSetDetails_;
    set<string> portfolioIds_;
    map<string, string> additionalFields_;
};

//! Enable writing of netting set details
std::ostream& operator<<(std::ostream& out, const NettingSetDetails& nettingSetDetails);

bool operator==(const NettingSetDetails& lhs, const NettingSetDetails& rhs);

} // namespace data
} // namespace ore
