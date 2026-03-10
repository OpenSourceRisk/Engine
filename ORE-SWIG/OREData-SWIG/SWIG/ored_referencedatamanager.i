/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#ifndef ored_referencedatamanager_i
#define ored_referencedatamanager_i

%include ored_xmlutils.i

%{
using ore::data::XMLSerializable;
using ore::data::ReferenceDatum;
using ore::data::BondReferenceDatum;
using ore::data::CreditIndexConstituent;
using ore::data::CreditIndexReferenceDatum;
using ore::data::EquityReferenceDatum;
using ore::data::CurrencyHedgedEquityIndexReferenceDatum;
using ore::data::EquityIndexReferenceDatum;
using ore::data::CommodityIndexReferenceDatum;
using ore::data::ReferenceDataManager;
using ore::data::BasicReferenceDataManager;
%}



%shared_ptr(ReferenceDatum)
class ReferenceDatum : public XMLSerializable {
public:
    ReferenceDatum();
    ReferenceDatum(const std::string& type, const std::string& id);
    ReferenceDatum(const std::string& type, const std::string& id, const QuantLib::Date& validFrom);
    void setType(const std::string& type);
    void setId(const std::string& id);
    void setValidFrom(const QuantLib::Date& validFrom);
    const std::string& type();
    const std::string& id();
    const QuantLib::Date& validFrom();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ReferenceDataManager)
class ReferenceDataManager {
public:
    virtual ~ReferenceDataManager();
    virtual bool hasData(const std::string& type, const std::string& id,
                         const QuantLib::Date& asof = QuantLib::Null<QuantLib::Date>()) = 0;
    virtual ext::shared_ptr<ReferenceDatum> getData(const std::string& type, const std::string& id, const QuantLib::Date& asof = QuantLib::Null<QuantLib::Date>()) = 0;
    virtual void add(const ext::shared_ptr<ReferenceDatum>& referenceDatum) = 0;
};

%shared_ptr(BasicReferenceDataManager)
class BasicReferenceDataManager : public ReferenceDataManager, public XMLSerializable {
public:
    BasicReferenceDataManager();
    BasicReferenceDataManager(const std::string& filename);

    // Load extra data and append to this manger
    void appendData(const std::string& filename);

    ext::shared_ptr<ReferenceDatum> buildReferenceDatum(const std::string& refDataType);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    // clear this ReferenceData manager, note that we can load multiple files
    void clear();

    bool hasData(const std::string& type, const std::string& id,
                 const QuantLib::Date& asof = QuantLib::Null<QuantLib::Date>()) override;
    ext::shared_ptr<ReferenceDatum> getData(const std::string& type, const std::string& id,
                                              const QuantLib::Date& asof = QuantLib::Null<QuantLib::Date>()) override;
    void add(const ext::shared_ptr<ReferenceDatum>& referenceDatum) override;
    // adds a datum from an xml node and returns it (or nullptr if nothing was added due to an error)
    ext::shared_ptr<ReferenceDatum> addFromXMLNode(XMLNode* node, const std::string& id = std::string(),
                                                     const QuantLib::Date& validFrom = QuantLib::Null<QuantLib::Date>());
};

%shared_ptr(BondReferenceDatum)
class BondReferenceDatum : public ReferenceDatum {
public:
    BondReferenceDatum();
    BondReferenceDatum(const std::string& id);
    BondReferenceDatum(const std::string& id, const QuantLib::Date& validFrom);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CreditIndexConstituent)
class CreditIndexConstituent : public XMLSerializable {
public:
    CreditIndexConstituent();
    CreditIndexConstituent(const std::string& name, QuantLib::Real weight,
                           QuantLib::Real priorWeight = QuantLib::Null<QuantLib::Real>(),
                           QuantLib::Real recovery = QuantLib::Null<QuantLib::Real>(),
                           const QuantLib::Date& auctionDate = QuantLib::Date(),
                           const QuantLib::Date& auctionSettlementDate = QuantLib::Date(),
                           const QuantLib::Date& defaultDate = QuantLib::Date(),
                           const QuantLib::Date& eventDeterminationDate = QuantLib::Date());
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CreditIndexReferenceDatum)
class CreditIndexReferenceDatum : public ReferenceDatum {
public:
    CreditIndexReferenceDatum();
    CreditIndexReferenceDatum(const std::string& name);
    CreditIndexReferenceDatum(const std::string& id, const QuantLib::Date& validFrom);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityIndexReferenceDatum)
class EquityIndexReferenceDatum : public ReferenceDatum {
public:
    EquityIndexReferenceDatum();
    EquityIndexReferenceDatum(const std::string& name);
    EquityIndexReferenceDatum(const std::string& name, const QuantLib::Date& validFrom);
};

%shared_ptr(CommodityIndexReferenceDatum)
class CommodityIndexReferenceDatum : public ReferenceDatum {
public:
    CommodityIndexReferenceDatum();
    CommodityIndexReferenceDatum(const std::string& name);
    CommodityIndexReferenceDatum(const std::string& name, const QuantLib::Date& validFrom);
};

%shared_ptr(CurrencyHedgedEquityIndexReferenceDatum)
class CurrencyHedgedEquityIndexReferenceDatum : public ReferenceDatum {
public:
    CurrencyHedgedEquityIndexReferenceDatum();
    CurrencyHedgedEquityIndexReferenceDatum(const std::string& name);
    CurrencyHedgedEquityIndexReferenceDatum(const std::string& name, const QuantLib::Date& validFrom);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityReferenceDatum)
class EquityReferenceDatum : public ReferenceDatum {
public:
    EquityReferenceDatum();
    EquityReferenceDatum(const std::string& id);
    EquityReferenceDatum(const std::string& id, const QuantLib::Date& validFrom);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};


#endif
