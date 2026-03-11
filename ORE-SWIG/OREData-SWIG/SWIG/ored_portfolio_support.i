/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_portfolio_support_i
#define ored_portfolio_support_i

%{
using ore::data::NettingSetDetails;
using ore::data::CSA;
using ore::data::NettingSetDefinition;
using ore::data::NettingSetManager;
using ore::data::TradeAction;
using ore::data::TradeActions;
using ore::data::CollateralBalance;
using ore::data::CollateralBalances;
using ore::data::CounterpartyCreditQuality;
using ore::data::CounterpartyInformation;
using ore::data::CounterpartyCorrelationMatrix;
using ore::data::CounterpartyManager;
using ore::data::parseCsaType;
using ore::data::parseCounterpartyCreditQuality;
using ore::data::BondUnderlying;
using ore::data::CommodityUnderlying;
using ore::data::BondBasket;
using ore::data::BondPositionData;
using ore::data::CommodityPositionData;
using ore::data::EquityPositionData;
using ore::data::EquityOptionUnderlyingData;
using ore::data::EquityOptionPositionData;
using ore::data::TreasuryLockData;
using ore::data::TrancheData;
using ore::data::RangeBound;
%}

%template(NettingSetDetailsVector) std::vector<NettingSetDetails>;
%template(TradeActionVector) std::vector<ext::shared_ptr<TradeAction>>;
%template(BondUnderlyingVector) std::vector<ext::shared_ptr<BondUnderlying>>;
%template(CommodityUnderlyingVector) std::vector<ext::shared_ptr<CommodityUnderlying>>;
%template(EquityUnderlyingVector) std::vector<ext::shared_ptr<EquityUnderlying>>;
%template(EquityOptionUnderlyingDataVector) std::vector<ext::shared_ptr<EquityOptionUnderlyingData>>;

%shared_ptr(NettingSetDetails)
class NettingSetDetails : public XMLSerializable {
public:
    NettingSetDetails();
    NettingSetDetails(const std::string& nettingSetId, const std::string& agreementType = "",
                      const std::string& callType = "", const std::string& initialMarginType = "",
                      const std::string& legalEntityId = "");
    NettingSetDetails(const std::map<std::string, std::string>& nettingSetMap);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    const std::string& nettingSetId() const;
    const std::string& agreementType() const;
    const std::string& callType() const;
    const std::string& initialMarginType() const;
    const std::string& legalEntityId() const;
    bool empty() const;
    bool emptyOptionalFields() const;
    void clear();
    const std::map<std::string, std::string> mapRepresentation() const;
};

%shared_ptr(CSA)
class CSA {
public:
    enum Type { Bilateral, CallOnly, PostOnly };

    CSA(const Type& type, const std::string& csaCurrency, const std::string& index,
        const QuantLib::Real& thresholdPay, const QuantLib::Real& thresholdRcv,
        const QuantLib::Real& mtaPay, const QuantLib::Real& mtaRcv, const QuantLib::Real& iaHeld,
        const std::string& iaType, const QuantLib::Period& marginCallFreq,
        const QuantLib::Period& marginPostFreq, const QuantLib::Period& mpr,
        const QuantLib::Real& collatSpreadPay, const QuantLib::Real& collatSpreadRcv,
        const std::vector<std::string>& eligCollatCcys, bool applyInitialMargin,
        Type initialMarginType, const bool calculateIMAmount, const bool calculateVMAmount,
        const std::string& nonExemptIMRegulations);

    const Type& type() const;
    const std::string& csaCurrency() const;
    const std::string& index() const;
    QuantLib::Real thresholdPay() const;
    QuantLib::Real thresholdRcv() const;
    QuantLib::Real mtaPay() const;
    QuantLib::Real mtaRcv() const;
    QuantLib::Real independentAmountHeld() const;
    const std::string& independentAmountType() const;
    const QuantLib::Period& marginCallFrequency() const;
    const QuantLib::Period& marginPostFrequency() const;
    const QuantLib::Period& marginPeriodOfRisk() const;
    QuantLib::Real collatSpreadRcv() const;
    QuantLib::Real collatSpreadPay() const;
    std::vector<std::string> eligCollatCcys() const;
    bool applyInitialMargin();
    Type initialMarginType();
    bool calculateIMAmount();
    bool calculateVMAmount();
    const std::string& nonExemptIMRegulations();
    void invertCSA();
    void validate();
};

CSA::Type parseCsaType(const std::string& s);

%shared_ptr(NettingSetDefinition)
class NettingSetDefinition : public XMLSerializable {
public:
    NettingSetDefinition();
    NettingSetDefinition(XMLNode* node);
    NettingSetDefinition(const NettingSetDetails& nettingSetDetails);
    NettingSetDefinition(const std::string& nettingSetId);
    NettingSetDefinition(const NettingSetDetails& nettingSetDetails, const std::string& bilateral,
                         const std::string& csaCurrency, const std::string& index,
                         const QuantLib::Real& thresholdPay, const QuantLib::Real& thresholdRcv,
                         const QuantLib::Real& mtaPay, const QuantLib::Real& mtaRcv,
                         const QuantLib::Real& iaHeld, const std::string& iaType,
                         const std::string& marginCallFreq, const std::string& marginPostFreq,
                         const std::string& mpr, const QuantLib::Real& collatSpreadPay,
                         const QuantLib::Real& collatSpreadRcv,
                         const std::vector<std::string>& eligCollatCcys,
                         bool applyInitialMargin = false,
                         const std::string& initialMarginType = "Bilateral",
                         const bool calculateIMAmount = false,
                         const bool calculateVMAmount = false,
                         const std::string& nonExemptIMRegulations = "");
    NettingSetDefinition(const std::string& nettingSetId, const std::string& bilateral,
                         const std::string& csaCurrency, const std::string& index,
                         const QuantLib::Real& thresholdPay, const QuantLib::Real& thresholdRcv,
                         const QuantLib::Real& mtaPay, const QuantLib::Real& mtaRcv,
                         const QuantLib::Real& iaHeld, const std::string& iaType,
                         const std::string& marginCallFreq, const std::string& marginPostFreq,
                         const std::string& mpr, const QuantLib::Real& collatSpreadPay,
                         const QuantLib::Real& collatSpreadRcv,
                         const std::vector<std::string>& eligCollatCcys,
                         bool applyInitialMargin = false,
                         const std::string& initialMarginType = "Bilateral",
                         const bool calculateIMAmount = false,
                         const bool calculateVMAmount = false,
                         const std::string& nonExemptIMRegulations = "");

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void validate();

    const std::string& nettingSetId() const;
    const NettingSetDetails nettingSetDetails() const;
    bool activeCsaFlag() const;
    const QuantLib::ext::shared_ptr<CSA>& csaDetails();
};

%shared_ptr(NettingSetManager)
class NettingSetManager : public XMLSerializable {
public:
    NettingSetManager();
    void reset();
    const bool empty() const;
    const bool calculateIMAmount() const;
    const std::set<NettingSetDetails> calculateIMNettingSets() const;
    bool has(const std::string& id) const;
    bool has(const NettingSetDetails& nettingSetDetails) const;
    void add(const QuantLib::ext::shared_ptr<NettingSetDefinition>& nettingSet) const;
    QuantLib::ext::shared_ptr<NettingSetDefinition> get(const std::string& id) const;
    QuantLib::ext::shared_ptr<NettingSetDefinition> get(const NettingSetDetails& nettingSetDetails) const;
    std::vector<NettingSetDetails> uniqueKeys() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void loadAll();
};

%shared_ptr(TradeAction)
class TradeAction : public XMLSerializable {
public:
    TradeAction();
    TradeAction(const std::string& type, const std::string& owner, const ScheduleData& schedule);
    const std::string& type() const;
    const std::string& owner() const;
    const ScheduleData& schedule() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(TradeActions)
class TradeActions : public XMLSerializable {
public:
    TradeActions(const std::vector<TradeAction>& actions = std::vector<TradeAction>());
    void addAction(const TradeAction& action);
    const std::vector<TradeAction>& actions() const;
    bool empty() const;
    void clear();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend TradeActions {
    TradeActions(const std::vector<ext::shared_ptr<TradeAction>>& actions) {
        return new TradeActions(VECTOR_SWIG_TO_ORE(actions));
    }
}

%shared_ptr(CollateralBalance)
class CollateralBalance : public XMLSerializable {
public:
    CollateralBalance();
    CollateralBalance(XMLNode* node);
    CollateralBalance(const NettingSetDetails& nettingSetDetails, const std::string& currency,
                      const QuantLib::Real& im,
                      const QuantLib::Real& vm = QuantLib::Null<QuantLib::Real>());
    CollateralBalance(const std::string& nettingSetId, const std::string& currency,
                      const QuantLib::Real& im,
                      const QuantLib::Real& vm = QuantLib::Null<QuantLib::Real>());
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& nettingSetId() const;
    const NettingSetDetails nettingSetDetails() const;
    const std::string& currency() const;
    const QuantLib::Real& initialMargin() const;
    const QuantLib::Real& variationMargin() const;
};

%shared_ptr(CollateralBalances)
class CollateralBalances : public XMLSerializable {
public:
    CollateralBalances();
    void reset();
    const bool empty();
    bool has(const std::string& nettingSetId) const;
    bool has(const NettingSetDetails& nettingSetDetails) const;
    void add(const QuantLib::ext::shared_ptr<CollateralBalance>& cb, const bool overwrite = false);
    const QuantLib::ext::shared_ptr<CollateralBalance>& get(const std::string& nettingSetId) const;
    const QuantLib::ext::shared_ptr<CollateralBalance>& get(const NettingSetDetails& nettingSetDetails) const;
    void currentIM(const std::string& baseCurrency, std::map<std::string, QuantLib::Real>& currentIM);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

enum class CounterpartyCreditQuality { IG, HY, NR };

CounterpartyCreditQuality parseCounterpartyCreditQuality(const std::string& cq);

%shared_ptr(CounterpartyInformation)
class CounterpartyInformation : public XMLSerializable {
public:
    CounterpartyInformation(const std::string& counterpartyId, bool isClearingCP = false,
                            CounterpartyCreditQuality creditQuality = CounterpartyCreditQuality::NR,
                            QuantLib::Real baCvaRiskWeight = QuantLib::Null<QuantLib::Real>(),
                            QuantLib::Real saCcrRiskWeight = QuantLib::Null<QuantLib::Real>(),
                            std::string saCvaRiskBucket = "");
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& counterpartyId() const;
    bool isClearingCP() const;
    const CounterpartyCreditQuality& creditQuality() const;
    QuantLib::Real baCvaRiskWeight() const;
    QuantLib::Real saCcrRiskWeight() const;
    const std::string& saCvaRiskBucket() const;
};

%shared_ptr(CounterpartyCorrelationMatrix)
class CounterpartyCorrelationMatrix : public XMLSerializable {
public:
    CounterpartyCorrelationMatrix();
    void addCorrelation(const std::string& cpty1, const std::string& cpty2,
                        QuantLib::Real correlation);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    QuantLib::Real lookup(const std::string& f1, const std::string& f2);
};

%shared_ptr(CounterpartyManager)
class CounterpartyManager : public XMLSerializable {
public:
    CounterpartyManager();
    void reset();
    const bool empty();
    bool has(std::string id) const;
    void add(const QuantLib::ext::shared_ptr<CounterpartyInformation>& nettingSet);
    void addCorrelation(const std::string& cpty1, const std::string& cpty2,
                        QuantLib::Real correlation);
    QuantLib::ext::shared_ptr<CounterpartyInformation> get(std::string id) const;
    std::vector<std::string> uniqueKeys() const;
    const QuantLib::ext::shared_ptr<CounterpartyCorrelationMatrix>& counterpartyCorrelations() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(BondUnderlying)
class BondUnderlying : public Underlying {
public:
    BondUnderlying();
    explicit BondUnderlying(const std::string& name);
    BondUnderlying(const std::string& identifier, const std::string& identifierType,
                   const QuantLib::Real weight);
    const std::string& name() const override;
    const std::string& identifierType() const;
    double bidAskAdjustment() const;
    void setBondName();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CommodityUnderlying)
class CommodityUnderlying : public Underlying {
public:
    CommodityUnderlying();
    CommodityUnderlying(const std::string& name, const QuantLib::Real weight,
                        const std::string& priceType,
                        const QuantLib::Size futureMonthOffset,
                        const QuantLib::Size deliveryRollDays,
                        const std::string& deliveryRollCalendar);
    const std::string& priceType() const;
    QuantLib::Size futureMonthOffset() const;
    QuantLib::Size deliveryRollDays() const;
    const std::string& deliveryRollCalendar() const;
    const std::string& futureContractMonth() const;
    const std::string& futureExpiryDate() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(BondBasket)
class BondBasket : public XMLSerializable {
public:
    BondBasket();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const;
    bool empty();
    void clear();
    QuantLib::ext::shared_ptr<QuantExt::BondBasket>
    build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
          const QuantLib::Currency& ccy, const std::string& reinvestmentEndDate);
};

%shared_ptr(BondPositionData)
class BondPositionData : public XMLSerializable {
public:
    BondPositionData();
    QuantLib::Real quantity() const;
    const std::string& identifier() const;
    const std::vector<BondUnderlying>& underlyings() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void populateFromBondBasketReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& ref);
};
%extend BondPositionData {
    BondPositionData(const QuantLib::Real quantity,
                     const std::vector<ext::shared_ptr<BondUnderlying>>& underlyings) {
        return new BondPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
    }
}

%shared_ptr(CommodityPositionData)
class CommodityPositionData : public XMLSerializable {
public:
    CommodityPositionData();
    QuantLib::Real quantity() const;
    const std::vector<CommodityUnderlying>& underlyings() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CommodityPositionData {
    CommodityPositionData(const QuantLib::Real quantity,
                          const std::vector<ext::shared_ptr<CommodityUnderlying>>& underlyings) {
        return new CommodityPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
    }
}

%shared_ptr(EquityPositionData)
class EquityPositionData : public XMLSerializable {
public:
    EquityPositionData();
    QuantLib::Real quantity() const;
    const std::vector<EquityUnderlying>& underlyings() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend EquityPositionData {
    EquityPositionData(const QuantLib::Real quantity,
                       const std::vector<ext::shared_ptr<EquityUnderlying>>& underlyings) {
        return new EquityPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
    }
}

%shared_ptr(EquityOptionUnderlyingData)
class EquityOptionUnderlyingData : public XMLSerializable {
public:
    EquityOptionUnderlyingData();
    EquityOptionUnderlyingData(const EquityUnderlying& underlying, const OptionData& optionData,
                               const QuantLib::Real strike);
    const EquityUnderlying& underlying() const;
    const OptionData& optionData() const;
    QuantLib::Real strike() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityOptionPositionData)
class EquityOptionPositionData : public XMLSerializable {
public:
    EquityOptionPositionData();
    QuantLib::Real quantity() const;
    const std::vector<EquityOptionUnderlyingData>& underlyings() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend EquityOptionPositionData {
    EquityOptionPositionData(const QuantLib::Real quantity,
                             const std::vector<ext::shared_ptr<EquityOptionUnderlyingData>>& underlyings) {
        return new EquityOptionPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
    }
}

%shared_ptr(TreasuryLockData)
class TreasuryLockData : public XMLSerializable {
public:
    TreasuryLockData();
    TreasuryLockData(bool payer, const BondData& bondData, QuantLib::Real referenceRate,
                     std::string dayCounter, std::string terminationDate, int paymentGap,
                     std::string paymentCalendar);
    bool empty() const;
    bool payer() const;
    const BondData& bondData() const;
    const BondData& originalBondData() const;
    QuantLib::Real referenceRate() const;
    const std::string& dayCounter() const;
    const std::string& terminationDate() const;
    int paymentGap() const;
    const std::string& paymentCalendar() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(TrancheData)
class TrancheData : public XMLSerializable {
public:
    TrancheData();
    TrancheData(const std::string& name, double icRatio, double ocRatio,
                const QuantLib::ext::shared_ptr<LegAdditionalData>& concreteLegData);
    const std::string name() const;
    double faceAmount();
    double icRatio();
    double ocRatio();
    const QuantLib::ext::shared_ptr<LegAdditionalData> concreteLegData();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(RangeBound)
class RangeBound : public XMLSerializable {
public:
    RangeBound();
    RangeBound(const QuantLib::Real from, const QuantLib::Real to,
               const QuantLib::Real leverage, const QuantLib::Real strike,
               const QuantLib::Real strikeAdjustment);
    QuantLib::Real from() const;
    QuantLib::Real to() const;
    QuantLib::Real leverage() const;
    QuantLib::Real strike() const;
    QuantLib::Real strikeAdjustment() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

#endif
