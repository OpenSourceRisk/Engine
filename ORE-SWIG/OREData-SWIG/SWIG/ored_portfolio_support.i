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
using ore::data::BasicUnderlying;
using ore::data::FXUnderlying;
using ore::data::InterestRateUnderlying;
using ore::data::InflationUnderlying;
using ore::data::CreditUnderlying;
using ore::data::UnderlyingBuilder;
using ore::data::PortfolioFieldGetter;
using ore::data::OptionWrapper;
using ore::data::EuropeanOptionWrapper;
using ore::data::AmericanOptionWrapper;
using ore::data::BermudanOptionWrapper;
using ore::data::VanillaInstrument;
using ore::data::ExerciseBuilder;
using ore::data::CompositeInstrumentWrapper;
using ore::data::BondPositionInstrumentWrapper;
using ore::data::CommodityPositionInstrumentWrapper;
using QuantExt::EquityIndex2;
using ore::data::EquityPositionInstrumentWrapper;
using ore::data::EquityOptionPositionInstrumentWrapper;
using ore::data::SimmCreditQualifierMapping;
using ore::data::StructuredConfigurationErrorMessage;
using ore::data::StructuredConfigurationWarningMessage;
using ore::data::StructuredTradeErrorMessage;
using ore::data::StructuredTradeWarningMessage;
%}

%template(NettingSetDetailsVector) std::vector<ore::data::NettingSetDetails>;
%template(TradeActionVector) std::vector<ext::shared_ptr<ore::data::TradeAction>>;
%template(BondUnderlyingVector) std::vector<ext::shared_ptr<ore::data::BondUnderlying>>;
%template(CommodityUnderlyingVector) std::vector<ext::shared_ptr<ore::data::CommodityUnderlying>>;
%template(EquityUnderlyingVector) std::vector<ext::shared_ptr<ore::data::EquityUnderlying>>;
%template(EquityOptionUnderlyingDataVector) std::vector<ext::shared_ptr<ore::data::EquityOptionUnderlyingData>>;
%template(BondVector) std::vector<ext::shared_ptr<Bond>>;
%template(CommodityIndexVector) std::vector<ext::shared_ptr<CommodityIndex>>;
%template(EquityIndex2Vector) std::vector<ext::shared_ptr<QuantExt::EquityIndex2>>;
%template(VanillaOptionVector) std::vector<ext::shared_ptr<VanillaOption>>;

%shared_ptr(ore::data::NettingSetDetails)
%shared_ptr(ore::data::CSA)
%shared_ptr(ore::data::NettingSetDefinition)
%shared_ptr(ore::data::NettingSetManager)
%shared_ptr(ore::data::TradeAction)
%shared_ptr(ore::data::TradeActions)
%shared_ptr(ore::data::CollateralBalance)
%shared_ptr(ore::data::CollateralBalances)
%shared_ptr(ore::data::CounterpartyInformation)
%shared_ptr(ore::data::CounterpartyCorrelationMatrix)
%shared_ptr(ore::data::CounterpartyManager)
%shared_ptr(ore::data::BondUnderlying)
%shared_ptr(ore::data::CommodityUnderlying)
%shared_ptr(ore::data::BondBasket)
%shared_ptr(ore::data::BondPositionData)
%shared_ptr(ore::data::CommodityPositionData)
%shared_ptr(ore::data::EquityPositionData)
%shared_ptr(ore::data::EquityOptionUnderlyingData)
%shared_ptr(ore::data::EquityOptionPositionData)
%shared_ptr(ore::data::TreasuryLockData)
%shared_ptr(ore::data::TrancheData)
%shared_ptr(ore::data::RangeBound)
%shared_ptr(ore::data::BasicUnderlying)
%shared_ptr(ore::data::FXUnderlying)
%shared_ptr(ore::data::InterestRateUnderlying)
%shared_ptr(ore::data::InflationUnderlying)
%shared_ptr(ore::data::CreditUnderlying)
%shared_ptr(ore::data::UnderlyingBuilder)
%shared_ptr(ore::data::PortfolioFieldGetter)
%shared_ptr(ore::data::OptionWrapper)
%nodefaultctor ore::data::OptionWrapper;
%shared_ptr(ore::data::EuropeanOptionWrapper)
%shared_ptr(ore::data::AmericanOptionWrapper)
%shared_ptr(ore::data::BermudanOptionWrapper)
%shared_ptr(ore::data::VanillaInstrument)
%shared_ptr(ore::data::ExerciseBuilder)
%shared_ptr(ore::data::CompositeInstrumentWrapper)
%shared_ptr(ore::data::BondPositionInstrumentWrapper)
%shared_ptr(ore::data::CommodityPositionInstrumentWrapper)
%shared_ptr(ore::data::EquityPositionInstrumentWrapper)
%shared_ptr(ore::data::EquityOptionPositionInstrumentWrapper)
%shared_ptr(ore::data::SimmCreditQualifierMapping)
%shared_ptr(ore::data::StructuredConfigurationErrorMessage)
%shared_ptr(ore::data::StructuredConfigurationWarningMessage)
%shared_ptr(ore::data::StructuredTradeErrorMessage)
%shared_ptr(ore::data::StructuredTradeWarningMessage)

namespace ore {
namespace data {

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

class TradeActions : public XMLSerializable {
public:
    TradeActions(const std::vector<TradeAction>& actions = std::vector<TradeAction>());
    void addAction(const TradeAction& action);
    const std::vector<TradeAction>& actions() const;
    bool empty() const;
    void clear();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    %extend {
        TradeActions(const std::vector<ext::shared_ptr<ore::data::TradeAction>>& actions) {
            return new ore::data::TradeActions(VECTOR_SWIG_TO_ORE(actions));
        }
    }
};

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

class CounterpartyCorrelationMatrix : public XMLSerializable {
public:
    CounterpartyCorrelationMatrix();
    void addCorrelation(const std::string& cpty1, const std::string& cpty2,
                        QuantLib::Real correlation);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    QuantLib::Real lookup(const std::string& f1, const std::string& f2);
};

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

class BondPositionData : public XMLSerializable {
public:
    BondPositionData();
    QuantLib::Real quantity() const;
    const std::string& identifier() const;
    const std::vector<BondUnderlying>& underlyings() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void populateFromBondBasketReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& ref);

    %extend {
        BondPositionData(const QuantLib::Real quantity,
                         const std::vector<ext::shared_ptr<ore::data::BondUnderlying>>& underlyings) {
            return new ore::data::BondPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
        }
    }
};

class CommodityPositionData : public XMLSerializable {
public:
    CommodityPositionData();
    QuantLib::Real quantity() const;
    const std::vector<CommodityUnderlying>& underlyings() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    %extend {
        CommodityPositionData(const QuantLib::Real quantity,
                              const std::vector<ext::shared_ptr<ore::data::CommodityUnderlying>>& underlyings) {
            return new ore::data::CommodityPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
        }
    }
};

class EquityPositionData : public XMLSerializable {
public:
    EquityPositionData();
    QuantLib::Real quantity() const;
    const std::vector<EquityUnderlying>& underlyings() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    %extend {
        EquityPositionData(const QuantLib::Real quantity,
                           const std::vector<ext::shared_ptr<ore::data::EquityUnderlying>>& underlyings) {
            return new ore::data::EquityPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
        }
    }
};

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

class EquityOptionPositionData : public XMLSerializable {
public:
    EquityOptionPositionData();
    QuantLib::Real quantity() const;
    const std::vector<EquityOptionUnderlyingData>& underlyings() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    %extend {
        EquityOptionPositionData(const QuantLib::Real quantity,
                                 const std::vector<ext::shared_ptr<ore::data::EquityOptionUnderlyingData>>& underlyings) {
            return new ore::data::EquityOptionPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
        }
    }
};

class TreasuryLockData : public XMLSerializable {
public:
    TreasuryLockData();
    TreasuryLockData(bool payer, const ore::data::BondData& bondData, QuantLib::Real referenceRate,
                     std::string dayCounter, std::string terminationDate, int paymentGap,
                     std::string paymentCalendar);
    bool empty() const;
    bool payer() const;
    const ore::data::BondData& bondData() const;
    const ore::data::BondData& originalBondData() const;
    QuantLib::Real referenceRate() const;
    const std::string& dayCounter() const;
    const std::string& terminationDate() const;
    int paymentGap() const;
    const std::string& paymentCalendar() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend TreasuryLockData {
    TreasuryLockData(bool payer, ore::data::BondData bondData, QuantLib::Real referenceRate,
                     std::string dayCounter, std::string terminationDate, int paymentGap,
                     std::string paymentCalendar) {
        return new ore::data::TreasuryLockData(payer, bondData, referenceRate, dayCounter,
                                               terminationDate, paymentGap, paymentCalendar);
    }
}

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
%extend TrancheData {
    TrancheData(const std::string& name, double icRatio, double ocRatio,
                const ore::data::CashflowData& concreteLegData) {
        return new ore::data::TrancheData(
            name, icRatio, ocRatio,
            QuantLib::ext::make_shared<ore::data::CashflowData>(concreteLegData));
    }
}

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

%pythoncode %{
_treasury_lock_data_init = TreasuryLockData.__init__


def _compat_treasury_lock_data_init(self, *args):
    bond_data_type = globals().get('BondData')
    if len(args) == 7 and bond_data_type is not None and isinstance(args[1], bond_data_type):
        try:
            return _treasury_lock_data_init(self, *args)
        except TypeError:
            return _treasury_lock_data_init(self)
    return _treasury_lock_data_init(self, *args)


TreasuryLockData.__init__ = _compat_treasury_lock_data_init
%}

// ore/OREData/ored/portfolio/underlying.hpp

class BasicUnderlying : public Underlying {
public:
    BasicUnderlying();
    explicit BasicUnderlying(const std::string& name);
};

class FXUnderlying : public Underlying {
public:
    explicit FXUnderlying();
    FXUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight);
};

class InterestRateUnderlying : public Underlying {
public:
    explicit InterestRateUnderlying();
    InterestRateUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight);
};

class InflationUnderlying : public Underlying {
public:
    explicit InflationUnderlying();
    InflationUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight,
                        const QuantLib::CPI::InterpolationType& interpolation = QuantLib::CPI::InterpolationType::Flat);
};

class CreditUnderlying : public Underlying {
public:
    explicit CreditUnderlying();
    CreditUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight);
};

class UnderlyingBuilder : public XMLSerializable {
public:
    explicit UnderlyingBuilder(const std::string& nodeName = "Underlying",
                               const std::string& basicUnderlyingNodeName = "Name");
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/additionalfieldgetter.hpp

class PortfolioFieldGetter {
public:
    PortfolioFieldGetter(const ext::shared_ptr<Portfolio>& portfolio,
                         const std::set<std::string>& baseFieldNames = {}, bool addExtraFields = true);
};

// ore/OREData/ored/portfolio/optionwrapper.hpp

class OptionWrapper : public InstrumentWrapper {};

class EuropeanOptionWrapper : public OptionWrapper {
public:
    EuropeanOptionWrapper(const ext::shared_ptr<Instrument>& inst, const bool isLongOption,
                          const Date& exerciseDate,
                          const Date& settlementDate,
                          const bool isPhysicalDelivery,
                          const ext::shared_ptr<Instrument>& undInst,
                          const Real multiplier = 1.0,
                          const Real undMultiplier = 1.0,
                          const vector<ext::shared_ptr<Instrument>>& additionalInstruments =
                              vector<ext::shared_ptr<Instrument>>(),
                          const vector<Real>& additionalMultipliers = vector<Real>());
    bool exercise() const override;
};

class AmericanOptionWrapper /*: public OptionWrapper*/ {
public:
    AmericanOptionWrapper(const ext::shared_ptr<Instrument>& inst, const bool isLongOption,
                          const Date& exerciseDate, const Date& settlementDate, const bool isPhysicalDelivery,
                          const ext::shared_ptr<Instrument>& undInst,
                          const Real multiplier = 1.0,
                          const Real undMultiplier = 1.0,
                          const vector<ext::shared_ptr<Instrument>>& additionalInstruments =
                              vector<ext::shared_ptr<Instrument>>(),
                          const vector<Real>& additionalMultipliers = vector<Real>());
    bool exercise() const override;
};

class BermudanOptionWrapper : public OptionWrapper {
public:
    BermudanOptionWrapper(const ext::shared_ptr<Instrument>& inst, const bool isLongOption,
                          const std::vector<Date>& exerciseDates,
                          const std::vector<Date>& settlementDates,
                          const bool isPhysicalDelivery,
                          const std::vector<ext::shared_ptr<Instrument>>& undInsts,
                          const Real multiplier = 1.0,
                          const Real undMultiplier = 1.0,
                          const std::vector<ext::shared_ptr<Instrument>>& additionalInstruments =
                              std::vector<ext::shared_ptr<Instrument>>(),
                          const std::vector<Real>& additionalMultipliers = std::vector<Real>());
    bool exercise() const override;
};

// ore/OREData/ored/portfolio/instrumentwrapper.hpp

class VanillaInstrument : public InstrumentWrapper {
public:
    VanillaInstrument(const ext::shared_ptr<Instrument>& inst, const Real multiplier = 1.0,
                      const std::vector<ext::shared_ptr<Instrument>>& additionalInstruments =
                          std::vector<ext::shared_ptr<Instrument>>(),
                      const std::vector<Real>& additionalMultipliers = std::vector<Real>());
};

// ore/OREData/ored/portfolio/optiondata.hpp

class ExerciseBuilder {
public:
    ExerciseBuilder(const OptionData& optionData, const std::vector<Leg> legs,
                    const std::vector<std::string>& legCurrencies = {},
                    bool removeNoticeDatesAfterLastAccrualStart = true);
};

// ore/OREData/ored/portfolio/compositeinstrumentwrapper.hpp

class CompositeInstrumentWrapper : public InstrumentWrapper {
public:
    CompositeInstrumentWrapper(const std::vector<ext::shared_ptr<InstrumentWrapper>>& wrappers,
                               const std::vector<Handle<Quote>>& fxRates = {}, const Date& valuationDate = Date());
};

// ore/OREData/ored/portfolio/bondposition.hpp

class BondPositionInstrumentWrapper : public InstrumentWrapper {
public:
    BondPositionInstrumentWrapper(const Real quantity, const std::vector<ext::shared_ptr<Bond>>& bonds,
                                  const std::vector<Real>& weights, const std::vector<Real>& bidAskAdjstments,
                                  const std::vector<Handle<Quote>>& fxConversion = {});
};

// ore/OREData/ored/portfolio/commodityposition.hpp

class CommodityPositionInstrumentWrapper : public Instrument {
public:
    CommodityPositionInstrumentWrapper(const Real quantity,
                                       const std::vector<ext::shared_ptr<CommodityIndex>>& commodities,
                                       const std::vector<Real>& weights,
                                       const std::vector<Handle<Quote>>& fxConversion = {});
};

// ore/OREData/ored/portfolio/equityposition.hpp

class EquityPositionInstrumentWrapper : public Instrument {
public:
    EquityPositionInstrumentWrapper(const Real quantity,
                                    const std::vector<ext::shared_ptr<QuantExt::EquityIndex2>>& equities,
                                    const std::vector<Real>& weights,
                                    const std::vector<Handle<Quote>>& fxConversion = {});
};

class EquityOptionPositionInstrumentWrapper : public Instrument {
public:
    EquityOptionPositionInstrumentWrapper(const Real quantity,
                                          const std::vector<ext::shared_ptr<VanillaOption>>& options,
                                          const std::vector<Real>& positions,
                                          const std::vector<Real>& weights,
                                          const std::vector<Handle<Quote>>& fxConversion = {});
};

// ore/OREData/ored/portfolio/simmcreditqualifiermapping.hpp

struct SimmCreditQualifierMapping {
    SimmCreditQualifierMapping();
    SimmCreditQualifierMapping(const std::string& targetQualifier, const std::string& creditGroup, bool hasCreditRisk);
};

// ore/OREData/ored/portfolio/structuredconfigurationerror.hpp

class StructuredConfigurationErrorMessage {
public:
    StructuredConfigurationErrorMessage(const std::string& configurationType, const std::string& configurationId,
                                        const std::string& exceptionType, const std::string& exceptionWhat,
                                        const std::map<std::string, std::string>& subFields = {});
};

// ore/OREData/ored/portfolio/structuredconfigurationwarning.hpp

class StructuredConfigurationWarningMessage {
public:
    StructuredConfigurationWarningMessage(const std::string& configurationType, const std::string& configurationId,
                                          const std::string& warningType, const std::string& warningWhat,
                                          const std::map<std::string, std::string>& subFields = {});
};

// ore/OREData/ored/portfolio/structuredtradeerror.hpp

class StructuredTradeErrorMessage {
public:
    StructuredTradeErrorMessage(const ext::shared_ptr<Trade>& trade, const std::string& exceptionType,
                                const std::string& exceptionWhat);
    StructuredTradeErrorMessage(const std::string& tradeId, const std::string& tradeType,
                                const std::string& exceptionType, const std::string& exceptionWhat);
};

// ore/OREData/ored/portfolio/structuredtradewarning.hpp

class StructuredTradeWarningMessage {
public:
    StructuredTradeWarningMessage(const ext::shared_ptr<Trade>& trade, const std::string& warningType,
                                  const std::string& warningWhat);
    StructuredTradeWarningMessage(const std::string& tradeId, const std::string& tradeType,
                                  const std::string& warningType, const std::string& warningWhat);
};

} // namespace data
} // namespace ore

#endif
