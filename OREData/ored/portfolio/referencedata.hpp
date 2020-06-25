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

/*! \file portfolio/referencedata.hpp
    \brief Reference data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/referencedatafactory.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/patterns/singleton.hpp>
#include <ql/time/date.hpp>

namespace ore {
namespace data {

//! Base class for reference data
/*! Each reference datum object is a subclass of this base and has it's own accessor functions.
 *  Instances of ReferenceDatum can be gotten from the ReferenceDataManager below, and then cast up as required.
 *  Each instance should be uniquely identified by it's type (which defines it's subclass, e.g. "Bond" for
 *  BondReferenceDatum) and it's id, which is a string. Here it can be any string but in applications there can be
 *  a naming scheme like ISIN for Bonds.
 */
class ReferenceDatum : public XMLSerializable {
public:
    //! Default Constructor
    ReferenceDatum() {}
    //! Base class constructor
    ReferenceDatum(const std::string& type, const std::string& id) : type_(type), id_(id) {}

    //! setters
    void setType(const string& type) { type_ = type; }
    void setId(const string& id) { id_ = id; }

    //! getters
    const std::string& type() const { return type_; }
    const std::string& id() const { return id_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) override;

private:
    std::string type_;
    std::string id_;
};

/*
<ReferenceDatum id="US12345678">
  <Type>Bond</Type>
  <BondReferenceData>
    <IssuerId>...</IssuerId>
    <SettlementDays>...</SettlementDays>
    <Calendar>...</Calendar>
    <IssueDate>...</IssueDate>
    <CreditCurveId>...</CreditCurveId>
    <ReferenceCurveId>...</ReferenceCurveId>
    <IncomCurveId>...</IncomeCurveId>
    <LegData>...</LegData>
  </BondReferenceData>
</ReferenceDatum>
*/
class LegData;
class BondReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "Bond";

    struct BondData : XMLSerializable {
        string issuerId;
        string settlementDays;
        string calendar;
        string issueDate;
        string creditCurveId;
        string referenceCurveId;
        string proxySecurityId;
        string incomeCurveId;
        string volatilityCurveId;
        std::vector<LegData> legData;
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(ore::data::XMLDocument& doc) override;
    };

    BondReferenceDatum() { setType(TYPE); }

    BondReferenceDatum(const string& id) : ReferenceDatum(TYPE, id) {}

    BondReferenceDatum(const string& id, const BondData& bondData) : ReferenceDatum(TYPE, id), bondData_(bondData) {}

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) override;

    const BondData& bondData() const { return bondData_; }
    void setBondData(const BondData& bondData) { bondData_ = bondData; }

private:
    BondData bondData_;
    static ReferenceDatumRegister<ReferenceDatumBuilder<BondReferenceDatum>> reg_;
};

//! Interface for Reference Data lookups
/*! The ReferenceDataManager is a repository of ReferenceDatum objects.
 *
 *  This is an interface, there is a concrete impl below that is BasicReferenceDataManager which is filebased.
 *  It is also possible to have a transactional implementation that will fetch reference data from a DB or via
 *  a restful call, possibly with a cache.
 *
 *  An instance of this class is made available to Trades as they are built, this code be done with a singleton
 *  but it is clearer to pass a pointer to an instance when the Trade is built itself, as there is no access method
 *  for Trade::build() we instead pass a pointer to the Trade constructors, this is then populated when TradeBuilders
 *  are created for ORE+ trades, this also allows us to have custom ORE+ versions of Trades that overload the build()
 *  method to use this data in place of extracting it from XML.
 *
 *  The actual Trade objects will take a copy of any reference data they need, this way they own all the required data
 *  and a call to "Trade::toXML()" will write out the "full" trade. For example we might load a CDS Index trade using
 *  reference data from which the basket is created, but if we call "toXML()" on that CDS Index the trade the whole
 *  basket will be written out.
 */
class ReferenceDataManager {
public:
    virtual ~ReferenceDataManager() {}
    virtual bool hasData(const string& type, const string& id) const = 0;
    virtual boost::shared_ptr<ReferenceDatum> getData(const string& type, const string& id) = 0;
    virtual void add(const boost::shared_ptr<ReferenceDatum>& referenceDatum) = 0;
};

//! Basic Concrete impl that loads an big XML and keeps data in memory
class BasicReferenceDataManager : public ReferenceDataManager, public XMLSerializable {
public:
    BasicReferenceDataManager() {}
    BasicReferenceDataManager(const string& filename) { fromFile(filename); }

    // Load extra data and append to this manger
    void appendData(const string& filename) { fromFile(filename); }

    boost::shared_ptr<ReferenceDatum> buildReferenceDatum(const string& refDataType);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) override;

    // clear this ReferenceData manager, note that we can load multiple files
    void clear() { data_.clear(); }

    bool hasData(const string& type, const string& id) const override;
    boost::shared_ptr<ReferenceDatum> getData(const string& type, const string& id) override;
    void add(const boost::shared_ptr<ReferenceDatum>& referenceDatum) override;

protected:
    map<pair<string, string>, boost::shared_ptr<ReferenceDatum>> data_;
};

} // namespace data
} // namespace ore
