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
using ore::data::BondFutureReferenceDatum;
using ore::data::CreditIndexConstituent;
using ore::data::CreditIndexReferenceDatum;
using ore::data::PortfolioBasketReferenceDatum;
using ore::data::CreditReferenceDatum;
using ore::data::EquityReferenceDatum;
using ore::data::BondBasketReferenceDatum;
using ore::data::CallableBondReferenceDatum;
using ore::data::ConvertibleBondReferenceDatum;
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
%feature("flatnested") BondReferenceDatum;
%rename(BondReferenceBondData) BondReferenceDatum::BondData;
%shared_ptr(BondReferenceDatum::BondData)
class BondReferenceDatum : public ReferenceDatum {
public:
    struct BondData : XMLSerializable {
        BondData();
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
    };

    BondReferenceDatum();
    BondReferenceDatum(const std::string& id);
    BondReferenceDatum(const std::string& id, const QuantLib::Date& validFrom);
    BondReferenceDatum(const std::string& id, const BondData& bondData);
    BondReferenceDatum(const std::string& id, const QuantLib::Date& validFrom,
                       const BondData& bondData);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const BondData& bondData() const;
    void setBondData(const BondData& bondData);
};
%extend BondReferenceDatum::BondData {
    BondData(const std::string& issuerId, const std::string& settlementDays,
             const std::string& calendar, const std::string& issueDate,
             const std::string& creditCurveId, const std::string& creditGroup,
             const std::string& referenceCurveId, const std::string& incomeCurveId,
             const std::string& volatilityCurveId, const std::string& priceQuoteMethod,
             const std::string& priceQuoteBaseValue, const std::string& subType) {
        auto* result = new BondReferenceDatum::BondData();
        result->issuerId = issuerId;
        result->settlementDays = settlementDays;
        result->calendar = calendar;
        result->issueDate = issueDate;
        result->creditCurveId = creditCurveId;
        result->creditGroup = creditGroup;
        result->referenceCurveId = referenceCurveId;
        result->incomeCurveId = incomeCurveId;
        result->volatilityCurveId = volatilityCurveId;
        result->priceQuoteMethod = priceQuoteMethod;
        result->priceQuoteBaseValue = priceQuoteBaseValue;
        result->subType = subType;
        return result;
    }

    const std::string& issuerId() const { return self->issuerId; }
    void setIssuerId(const std::string& issuerId) { self->issuerId = issuerId; }
    const std::string& settlementDays() const { return self->settlementDays; }
    void setSettlementDays(const std::string& settlementDays) { self->settlementDays = settlementDays; }
    const std::string& calendar() const { return self->calendar; }
    void setCalendar(const std::string& calendar) { self->calendar = calendar; }
    const std::string& issueDate() const { return self->issueDate; }
    void setIssueDate(const std::string& issueDate) { self->issueDate = issueDate; }
    const std::string& creditCurveId() const { return self->creditCurveId; }
    void setCreditCurveId(const std::string& creditCurveId) { self->creditCurveId = creditCurveId; }
    const std::string& creditGroup() const { return self->creditGroup; }
    void setCreditGroup(const std::string& creditGroup) { self->creditGroup = creditGroup; }
    const std::string& referenceCurveId() const { return self->referenceCurveId; }
    void setReferenceCurveId(const std::string& referenceCurveId) { self->referenceCurveId = referenceCurveId; }
    const std::string& incomeCurveId() const { return self->incomeCurveId; }
    void setIncomeCurveId(const std::string& incomeCurveId) { self->incomeCurveId = incomeCurveId; }
    const std::string& volatilityCurveId() const { return self->volatilityCurveId; }
    void setVolatilityCurveId(const std::string& volatilityCurveId) { self->volatilityCurveId = volatilityCurveId; }
    const std::string& priceQuoteMethod() const { return self->priceQuoteMethod; }
    void setPriceQuoteMethod(const std::string& priceQuoteMethod) { self->priceQuoteMethod = priceQuoteMethod; }
    const std::string& priceQuoteBaseValue() const { return self->priceQuoteBaseValue; }
    void setPriceQuoteBaseValue(const std::string& priceQuoteBaseValue) { self->priceQuoteBaseValue = priceQuoteBaseValue; }
    const std::string& subType() const { return self->subType; }
    void setSubType(const std::string& subType) { self->subType = subType; }
}

%feature("flatnested") BondFutureReferenceDatum;
%rename(BondFutureReferenceBondFutureData) BondFutureReferenceDatum::BondFutureData;
%shared_ptr(BondFutureReferenceDatum)
%shared_ptr(BondFutureReferenceDatum::BondFutureData)
class BondFutureReferenceDatum : public ReferenceDatum {
public:
    struct BondFutureData : XMLSerializable {
        BondFutureData();
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
    };

    BondFutureReferenceDatum();
    BondFutureReferenceDatum(const std::string& id);
    BondFutureReferenceDatum(const std::string& id, const BondFutureData& bondFutureData);
    BondFutureReferenceDatum(const std::string& id, const QuantLib::Date& validFrom);
    BondFutureReferenceDatum(const std::string& id, const QuantLib::Date& validFrom,
                             const BondFutureData& bondFutureData);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const BondFutureData& bondFutureData() const;
};
%extend BondFutureReferenceDatum::BondFutureData {
    const std::string& currency() const { return self->currency; }
    void setCurrency(const std::string& currency) { self->currency = currency; }
    const std::vector<std::string>& deliveryBasket() const { return self->deliveryBasket; }
    void setDeliveryBasket(const std::vector<std::string>& deliveryBasket) { self->deliveryBasket = deliveryBasket; }
    const std::string& deliverableGrade() const { return self->deliverableGrade; }
    void setDeliverableGrade(const std::string& deliverableGrade) { self->deliverableGrade = deliverableGrade; }
    const std::string& lastTrading() const { return self->lastTrading; }
    void setLastTrading(const std::string& lastTrading) { self->lastTrading = lastTrading; }
    const std::string& lastDelivery() const { return self->lastDelivery; }
    void setLastDelivery(const std::string& lastDelivery) { self->lastDelivery = lastDelivery; }
    const std::string& settlement() const { return self->settlement; }
    void setSettlement(const std::string& settlement) { self->settlement = settlement; }
    const std::string& dirtyQuotation() const { return self->dirtyQuotation; }
    void setDirtyQuotation(const std::string& dirtyQuotation) { self->dirtyQuotation = dirtyQuotation; }
    const std::string& contractMonth() const { return self->contractMonth; }
    void setContractMonth(const std::string& contractMonth) { self->contractMonth = contractMonth; }
    const std::string& rootDate() const { return self->rootDate; }
    void setRootDate(const std::string& rootDate) { self->rootDate = rootDate; }
    const std::string& expiryBasis() const { return self->expiryBasis; }
    void setExpiryBasis(const std::string& expiryBasis) { self->expiryBasis = expiryBasis; }
    const std::string& settlementBasis() const { return self->settlementBasis; }
    void setSettlementBasis(const std::string& settlementBasis) { self->settlementBasis = settlementBasis; }
    const std::string& expiryLag() const { return self->expiryLag; }
    void setExpiryLag(const std::string& expiryLag) { self->expiryLag = expiryLag; }
    const std::string& settlementLag() const { return self->settlementLag; }
    void setSettlementLag(const std::string& settlementLag) { self->settlementLag = settlementLag; }
}

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

%shared_ptr(PortfolioBasketReferenceDatum)
class PortfolioBasketReferenceDatum : public ReferenceDatum {
public:
    PortfolioBasketReferenceDatum();
    PortfolioBasketReferenceDatum(const std::string& id);
    PortfolioBasketReferenceDatum(const std::string& id, const QuantLib::Date& validFrom);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::vector<QuantLib::ext::shared_ptr<Trade>> getTrades() const;
};

%shared_ptr(CreditReferenceDatum)
class CreditReferenceDatum : public ReferenceDatum {
public:
    CreditReferenceDatum();
    CreditReferenceDatum(const std::string& id);
    CreditReferenceDatum(const std::string& id, const QuantLib::Date& validFrom);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CreditReferenceDatum {
    CreditReferenceDatum(const std::string& id, const std::string& name, const std::string& group,
                         const std::string& successor, const std::string& predecessor,
                         const QuantLib::Date& successorImplementationDate,
                         const QuantLib::Date& predecessorImplementationDate,
                         const std::string& entityType, const std::string& primaryPriceType,
                         const QuantLib::Real runningSpread) {
        ore::data::CreditReferenceDatum::CreditData data;
        data.name = name;
        data.group = group;
        data.successor = successor;
        data.predecessor = predecessor;
        data.successorImplementationDate = successorImplementationDate;
        data.predecessorImplementationDate = predecessorImplementationDate;
        data.entityType = entityType;
        data.primaryPriceType = primaryPriceType;
        data.runningSpread = runningSpread;
        return new CreditReferenceDatum(id, data);
    }
}

%extend EquityReferenceDatum {
    EquityReferenceDatum(const std::string& id, const std::string& equityId,
                         const std::string& equityName, const std::string& currency,
                         QuantLib::Size scalingFactor, const std::string& exchangeCode,
                         bool isIndex, const QuantLib::Date& equityStartDate,
                         const std::string& proxyIdentifier, const std::string& simmBucket,
                         const std::string& crifQualifier, const std::string& proxyVolatilityId) {
        ore::data::EquityReferenceDatum::EquityData data;
        data.equityId = equityId;
        data.equityName = equityName;
        data.currency = currency;
        data.scalingFactor = scalingFactor;
        data.exchangeCode = exchangeCode;
        data.isIndex = isIndex;
        data.equityStartDate = equityStartDate;
        data.proxyIdentifier = proxyIdentifier;
        data.simmBucket = simmBucket;
        data.crifQualifier = crifQualifier;
        data.proxyVolatilityId = proxyVolatilityId;
        return new EquityReferenceDatum(id, data);
    }
}

%shared_ptr(BondBasketReferenceDatum)
class BondBasketReferenceDatum : public ReferenceDatum {
public:
    BondBasketReferenceDatum();
    BondBasketReferenceDatum(const std::string& id);
    BondBasketReferenceDatum(const std::string& id, const QuantLib::Date& validFrom);
    BondBasketReferenceDatum(const std::string& id, const std::vector<BondUnderlying>& underlyingData);
    BondBasketReferenceDatum(const std::string& id, const QuantLib::Date& validFrom,
                             const std::vector<BondUnderlying>& underlyingData);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::vector<BondUnderlying>& underlyingData() const;
};
%extend BondBasketReferenceDatum {
    BondBasketReferenceDatum(const std::string& id,
                             const std::vector<ext::shared_ptr<BondUnderlying>>& underlyingData) {
        return new BondBasketReferenceDatum(id, VECTOR_SWIG_TO_ORE(underlyingData));
    }

    BondBasketReferenceDatum(const std::string& id, const QuantLib::Date& validFrom,
                             const std::vector<ext::shared_ptr<BondUnderlying>>& underlyingData) {
        return new BondBasketReferenceDatum(id, validFrom, VECTOR_SWIG_TO_ORE(underlyingData));
    }
}

%shared_ptr(CallableBondReferenceDatum)
class CallableBondReferenceDatum : public ReferenceDatum {
public:
    CallableBondReferenceDatum();
    CallableBondReferenceDatum(const std::string& id);
    CallableBondReferenceDatum(const std::string& id, const BondReferenceDatum::BondData& bondData,
                               const CallableBondData::CallabilityData& callData,
                               const CallableBondData::CallabilityData& putData);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const BondReferenceDatum::BondData& bondData() const;
    const CallableBondData::CallabilityData& callData() const;
    const CallableBondData::CallabilityData& putData() const;
    void setBondData(const BondReferenceDatum::BondData& bondData);
    void setCallData(const CallableBondData::CallabilityData& callData);
    void setPutData(const CallableBondData::CallabilityData& putData);
};

%shared_ptr(ConvertibleBondReferenceDatum)
class ConvertibleBondReferenceDatum : public ReferenceDatum {
public:
    ConvertibleBondReferenceDatum();
    ConvertibleBondReferenceDatum(const std::string& id);
    ConvertibleBondReferenceDatum(const std::string& id, const BondReferenceDatum::BondData& bondData,
                                  const ConvertibleBondData::CallabilityData& callData,
                                  const ConvertibleBondData::CallabilityData& putData,
                                  const ConvertibleBondData::ConversionData& conversionData,
                                  const ConvertibleBondData::DividendProtectionData& dividendProtectionData);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const BondReferenceDatum::BondData& bondData() const;
    const ConvertibleBondData::CallabilityData& callData() const;
    const ConvertibleBondData::CallabilityData& putData() const;
    const ConvertibleBondData::ConversionData& conversionData() const;
    const ConvertibleBondData::DividendProtectionData& dividendProtectionData() const;
    std::string detachable() const;
    void setBondData(const BondReferenceDatum::BondData& bondData);
    void setCallData(const ConvertibleBondData::CallabilityData& callData);
    void setPutData(const ConvertibleBondData::CallabilityData& putData);
    void setConversionData(const ConvertibleBondData::ConversionData& conversionData);
    void setDividendProtectionData(
        const ConvertibleBondData::DividendProtectionData& dividendProtectionData);
};


#endif
