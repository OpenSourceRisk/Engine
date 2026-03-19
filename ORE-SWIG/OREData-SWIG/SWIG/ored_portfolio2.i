
/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_portfolio2_i
#define ored_portfolio2_i

%{
using QuantLib::Instrument;
using ore::data::ScriptedTradeEventData;
using ore::data::ScriptedTradeValueTypeData;
using ore::data::ScriptedTradeScriptData;
using ore::data::ScriptLibraryData;
using ore::data::ScriptedTrade;
using ore::data::Accumulator;
using ore::data::EquityAccumulator;
using ore::data::FxAccumulator;
using ore::data::CommodityAccumulator;
using ore::data::PortfolioFieldGetter;
using ore::data::Ascot;
using ore::data::AsianOption;
using ore::data::EquityAsianOption;
using ore::data::FxAsianOption;
using ore::data::CommodityAsianOption;
using ore::data::Autocallable_01;
using ore::data::BGSTrancheData;
using ore::data::BalanceGuaranteedSwap;
using OREBarrierOption = ore::data::BarrierOption;
using ore::data::FxOptionWithBarrier;
using ore::data::EquityOptionWithBarrier;
using ore::data::OptionWrapper;
using ore::data::EuropeanOptionWrapper;
using ore::data::AmericanOptionWrapper;
using ore::data::BermudanOptionWrapper;
using OREBasketOption = ore::data::BasketOption;
using ore::data::EquityBasketOption;
using ore::data::FxBasketOption;
using ore::data::CommodityBasketOption;
using ore::data::BasketVarianceSwap;
using ore::data::EquityBasketVarianceSwap;
using ore::data::FxBasketVarianceSwap;
using ore::data::CommodityBasketVarianceSwap;
using ore::data::BestEntryOption;
using ore::data::EquityBestEntryOption;
using ore::data::FxBestEntryOption;
using ore::data::CommodityBestEntryOption;
using ore::data::BondBasket;
using ore::data::BondFuture;
using ore::data::BondOption;
using ore::data::BondPositionData;
using ore::data::BondPosition;
using ore::data::BondPositionInstrumentWrapper;
using ore::data::BondRepo;
using ore::data::BondTRS;
using ore::data::CallableBondData;
using ORECallableBond = ore::data::CallableBond;
using ore::data::CallableSwap;
using ORECapFloor = ore::data::CapFloor;
using ore::data::CashPosition;
using ore::data::CBO;
using ORECliquetOption = ore::data::CliquetOption;
using ore::data::EquityCliquetOption;
using ore::data::CollateralBalance;
using ore::data::CollateralBalances;
using ore::data::CommodityAveragePriceOption;
using ore::data::CommodityDigitalAveragePriceOption;
using ore::data::CommodityDigitalOption;
using ore::data::CommodityFixedLegBuilder;
using ore::data::CommodityFloatingLegBuilder;
using ore::data::CommodityOption;
using ore::data::CommodityOption;
using ore::data::CommodityOptionStrip;
using ore::data::CommodityPositionData;
using ore::data::CommodityPosition;
using ore::data::CommodityPositionInstrumentWrapper;
using ore::data::CommoditySpreadOptionData;
using ore::data::CommoditySpreadOption;
using ore::data::CommoditySwap;
using ore::data::CommoditySwaption;
using ore::data::CompositeInstrumentWrapper;
using ore::data::CompositeTrade;
using ore::data::ConvertibleBond;
using ore::data::ConvertibleBondData;
using ore::data::ConvertibleBondReferenceDatum;
using ore::data::CounterpartyCorrelationMatrix;
using ore::data::CounterpartyCreditQuality;
using ore::data::CounterpartyInformation;
using ore::data::CounterpartyManager;
using ore::data::CreditDefaultSwapOption;
using ore::data::CreditLinkedSwap;
using ore::data::CrossCurrencySwap;
using ore::data::DoubleDigitalOption;
using ore::data::DurationAdjustedCmsLegBuilder;
using ore::data::DurationAdjustedCmsLegData;
using ore::data::EquityBarrierOption;
using ore::data::EquityDigitalOption;
using ore::data::EquityDoubleBarrierOption;
using ore::data::EquityDoubleTouchOption;
using ore::data::EquityEuropeanBarrierOption;
using OREEquityForward = ore::data::EquityForward;
using ore::data::EquityFutureOption;
using ore::data::EquityMarginLegBuilder;
using ore::data::EquityMarginLegData;
using ore::data::EquityOptionUnderlyingData;
using ore::data::EquityOptionPositionData;
using ore::data::EquityOptionPosition;
using ore::data::EquityOptionPositionInstrumentWrapper;
using ore::data::EquityOutperformanceOption;
using ore::data::EquityPositionData;
using ore::data::EquityPosition;
using QuantExt::EquityIndex2;
using ore::data::EquityPositionInstrumentWrapper;
using ore::data::EquityOptionPositionInstrumentWrapper;
using ore::data::EquitySwap;
using ore::data::EquityTouchOption;
using ore::data::EuropeanOptionBarrier;
using ore::data::FailedTrade;
using ore::data::FlexiSwap;
using ore::data::FormulaBasedLegBuilder;
using ore::data::FormulaBasedLegData;
using ore::data::ForwardBond;
using OREForwardRateAgreement = ore::data::ForwardRateAgreement;
using ore::data::FxAverageForward;
using ore::data::FxDigitalBarrierOption;
using ore::data::FxDigitalOption;
using ore::data::FxDoubleBarrierOption;
using ore::data::FxDoubleTouchOption;
using ore::data::FxEuropeanBarrierOption;
using ore::data::FxKIKOBarrierOption;
using ore::data::FxSwap;
using ore::data::GenericBarrierOption;
using ore::data::EquityGenericBarrierOption;
using ore::data::FxGenericBarrierOption;
using ore::data::CommodityGenericBarrierOption;
using ore::data::IndexCreditDefaultSwap;
using ore::data::IndexCreditDefaultSwapOption;
using ore::data::Indexing;
using ore::data::InflationSwap;
using ore::data::VanillaInstrument;
using ore::data::KnockOutSwap;
using ore::data::FixedLegBuilder;
using ore::data::ZeroCouponFixedLegBuilder;
using ore::data::FloatingLegBuilder;
using ore::data::CashflowLegBuilder;
using ore::data::CPILegBuilder;
using ore::data::YYLegBuilder;
using ore::data::CMSLegBuilder;
using ore::data::CMBLegBuilder;
using ore::data::DigitalCMSLegBuilder;
using ore::data::CMSSpreadLegBuilder;
using ore::data::DigitalCMSSpreadLegBuilder;
using ore::data::EquityLegBuilder;
using ore::data::MultiLegOption;
using ore::data::NettingSetDetails;
using ore::data::CSA;
using ore::data::NettingSetDefinition;
using ore::data::NettingSetManager;
using ore::data::ExerciseBuilder;
using ore::data::OptionExerciseData;
using ore::data::OptionPaymentData;
using ore::data::PairwiseVarSwap;
using ore::data::EqPairwiseVarSwap;
using ore::data::FxPairwiseVarSwap;
using ore::data::PerformanceOption_01;
using ore::data::RainbowOption;
using ore::data::EquityRainbowOption;
using ore::data::FxRainbowOption;
using ore::data::CommodityRainbowOption;
using ore::data::RangeBound;
using ore::data::BondReferenceDatum;
using ore::data::BondFutureReferenceDatum;
using ore::data::CreditIndexConstituent;
using ore::data::CreditIndexReferenceDatum;
using ore::data::IndexReferenceDatum;
using ore::data::EquityIndexReferenceDatum;
using ore::data::CommodityIndexReferenceDatum;
using ore::data::CurrencyHedgedEquityIndexReferenceDatum;
using ore::data::PortfolioBasketReferenceDatum;
using ore::data::CreditReferenceDatum;
using ore::data::EquityReferenceDatum;
using ore::data::BondBasketReferenceDatum;
using ore::data::BasicReferenceDataManager;
using ore::data::RiskParticipationAgreement;
using ore::data::ScheduleDates;
using ore::data::ScheduleDerived;
using ore::data::SimmCreditQualifierMapping;
using ore::data::StrikeResettableOption;
using ore::data::EquityStrikeResettableOption;
using ore::data::FxStrikeResettableOption;
using ore::data::CommodityStrikeResettableOption;
using ore::data::StructuredConfigurationErrorMessage;
using ore::data::StructuredConfigurationWarningMessage;
using ore::data::StructuredTradeErrorMessage;
using ore::data::StructuredTradeWarningMessage;
using ore::data::SwaptionStraddle;
using ore::data::TaRF;
using ore::data::EquityTaRF;
using ore::data::FxTaRF;
using ore::data::CommodityTaRF;
using ore::data::TreasuryLockData;
using ore::data::TradeAction;
using ore::data::TradeActions;
using ore::data::TradeMonetary;
using ore::data::TrancheData;
using ore::data::TRS;
using ore::data::CFD;
using ore::data::BasicUnderlying;
using ore::data::CommodityUnderlying;
using ore::data::FXUnderlying;
using ore::data::InterestRateUnderlying;
using ore::data::InflationUnderlying;
using ore::data::CreditUnderlying;
using ore::data::BondUnderlying;
using ore::data::UnderlyingBuilder;
using ore::data::VarSwap;
using ore::data::EqVarSwap;
using ore::data::FxVarSwap;
using ore::data::ComVarSwap;
using ore::data::WindowBarrierOption;
using ore::data::EquityWindowBarrierOption;
using ore::data::FxWindowBarrierOption;
using ore::data::CommodityWindowBarrierOption;
using ore::data::WorstOfBasketSwap;
using ore::data::EquityWorstOfBasketSwap;
using ore::data::FxWorstOfBasketSwap;
using ore::data::CommodityWorstOfBasketSwap;
%}

// ore/OREData/ored/portfolio/scriptedtrade.hpp

%shared_ptr(ScriptedTradeEventData)
class ScriptedTradeEventData : public XMLSerializable {
public:
    ScriptedTradeEventData();
    ScriptedTradeEventData(const std::string& name, const std::string& date);
    ScriptedTradeEventData(const std::string& name, const ScheduleData& schedule);
    ScriptedTradeEventData(const std::string& name, const std::string& baseSchedule, const std::string& shift,
                           const std::string& calendar, const std::string& convention);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(ScriptedTradeEventDataVector) vector<ext::shared_ptr<ScriptedTradeEventData>>;

%shared_ptr(ScriptedTradeValueTypeData)
class ScriptedTradeValueTypeData : public XMLSerializable {
public:
    explicit ScriptedTradeValueTypeData(const std::string& nodeName);
    ScriptedTradeValueTypeData(const std::string& nodeName, const std::string& name, const std::string& value);
    ScriptedTradeValueTypeData(const std::string& nodeName, const std::string& name,
                               const std::vector<std::string>& values);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(ScriptedTradeValueTypeDataVector) vector<ext::shared_ptr<ScriptedTradeValueTypeData>>;

%shared_ptr(ScriptedTradeScriptData)
%shared_ptr(ScriptedTradeScriptData::NewScheduleData)
%shared_ptr(ScriptedTradeScriptData::CalibrationData)
class ScriptedTradeScriptData : public XMLSerializable {
public:
    class NewScheduleData : public XMLSerializable {
    public:
        NewScheduleData();
        NewScheduleData(const std::string& name, const std::string& operation,
                        const std::vector<std::string>& sourceSchedules);
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(XMLDocument& doc) const override;
    };
    class CalibrationData : public XMLSerializable {
    public:
        CalibrationData();
        CalibrationData(const std::string& index, const std::vector<std::string>& strikes);
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(XMLDocument& doc) const override;
    };
    ScriptedTradeScriptData();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend ScriptedTradeScriptData {
    ScriptedTradeScriptData(const std::string& code, const std::string& npv,
                            const std::vector<std::pair<std::string, std::string>>& results,
                            const std::vector<std::string>& schedulesEligibleForCoarsening,
                            const std::vector<ext::shared_ptr<NewScheduleData>>& newSchedules = {},
                            const std::vector<ext::shared_ptr<CalibrationData>>& calibrationSpec = {},
                            const std::vector<std::string>& stickyCloseOutStates = {},
                            const std::vector<std::string>& conditionalExpectationModelStates = {}) {
        return new ScriptedTradeScriptData(code, npv, results, schedulesEligibleForCoarsening,
            VECTOR_SWIG_TO_ORE(newSchedules), VECTOR_SWIG_TO_ORE(calibrationSpec), stickyCloseOutStates,
            conditionalExpectationModelStates);
    }
}
// A helper to make it easier to populate parameter "results" above:
%template(VectorPairString) std::vector<std::pair<std::string, std::string>>;

%shared_ptr(ScriptLibraryData)
class ScriptLibraryData : public XMLSerializable {
public:
    ScriptLibraryData();
    ScriptLibraryData(const ScriptLibraryData& d);
    ScriptLibraryData(ScriptLibraryData&& d);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScriptedTrade)
class ScriptedTrade : public Trade {
public:
    ScriptedTrade(const std::string& tradeType = "ScriptedTrade", const Envelope& env = Envelope());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend ScriptedTrade {
    // FIXME If this overload is required then we must implement support for map<string, ScriptedTradeScriptData>
    //ScriptedTrade(const Envelope& env,
    //              const std::vector<ext::shared_ptr<ScriptedTradeEventData>>& events,
    //              const std::vector<ext::shared_ptr<ScriptedTradeValueTypeData>>& numbers,
    //              const std::vector<ext::shared_ptr<ScriptedTradeValueTypeData>>& indices,
    //              const std::vector<ext::shared_ptr<ScriptedTradeValueTypeData>>& currencies,
    //              const std::vector<ext::shared_ptr<ScriptedTradeValueTypeData>&> daycounters,
    //              const std::map<std::string, ext::shared_ptr<ScriptedTradeScriptData>>& script,
    //              const std::string& productTag, const std::string& tradeType = "ScriptedTrade") {
    //    return new ScriptedTrade(env,
    //        VECTOR_SWIG_TO_ORE(events),
    //        VECTOR_SWIG_TO_ORE(numbers),
    //        VECTOR_SWIG_TO_ORE(indices),
    //        VECTOR_SWIG_TO_ORE(currencies),
    //        VECTOR_SWIG_TO_ORE(daycounters),
    //        MAP_SWIG_TO_ORE(script),              <- need to implement this in order to support this function
    //        productTag, tradeType);
    //}
    ScriptedTrade(const Envelope& env,
                  const std::vector<ext::shared_ptr<ScriptedTradeEventData>>& events,
                  const std::vector<ext::shared_ptr<ScriptedTradeValueTypeData>>& numbers,
                  const std::vector<ext::shared_ptr<ScriptedTradeValueTypeData>>& indices,
                  const std::vector<ext::shared_ptr<ScriptedTradeValueTypeData>>& currencies,
                  const std::vector<ext::shared_ptr<ScriptedTradeValueTypeData>>& daycounters,
                  const std::string& scriptName, const std::string& tradeType = "ScriptedTrade") {
        return new ScriptedTrade(env,
            VECTOR_SWIG_TO_ORE(events),
            VECTOR_SWIG_TO_ORE(numbers),
            VECTOR_SWIG_TO_ORE(indices),
            VECTOR_SWIG_TO_ORE(currencies),
            VECTOR_SWIG_TO_ORE(daycounters),
            scriptName, tradeType);
    }
}

// ore/OREData/ored/portfolio/underlying.hpp

%shared_ptr(BasicUnderlying)
class BasicUnderlying : public Underlying {
public:
    BasicUnderlying();
    explicit BasicUnderlying(const std::string& name);
};

%shared_ptr(CommodityUnderlying)
class CommodityUnderlying : public Underlying {
public:
    CommodityUnderlying();
    CommodityUnderlying(const std::string& name, const QuantLib::Real weight, const std::string& priceType,
                        const QuantLib::Size futureMonthOffset, const QuantLib::Size deliveryRollDays,
                        const std::string& deliveryRollCalendar);
};
%template(CommodityUnderlyingVector) vector<ext::shared_ptr<CommodityUnderlying>>;

%shared_ptr(FXUnderlying)
class FXUnderlying : public Underlying {
public:
    explicit FXUnderlying();
    FXUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight);
};

%shared_ptr(InterestRateUnderlying)
class InterestRateUnderlying : public Underlying {
public:
    explicit InterestRateUnderlying();
    InterestRateUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight);
};

%shared_ptr(InflationUnderlying)
class InflationUnderlying : public Underlying {
public:
    explicit InflationUnderlying();
    InflationUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight,
                        const QuantLib::CPI::InterpolationType& interpolation = QuantLib::CPI::InterpolationType::Flat);
};

%shared_ptr(CreditUnderlying)
class CreditUnderlying : public Underlying {
public:
    explicit CreditUnderlying();
    CreditUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight);
};

%shared_ptr(BondUnderlying)
class BondUnderlying : public Underlying {
public:
    BondUnderlying();
    explicit BondUnderlying(const std::string& name);
    BondUnderlying(const std::string& identifier, const std::string& identifierType, const QuantLib::Real weight);
};
%template(BondUnderlyingVector) vector<ext::shared_ptr<BondUnderlying>>;

%shared_ptr(UnderlyingBuilder)
class UnderlyingBuilder : public XMLSerializable {
public:
    explicit UnderlyingBuilder(const std::string& nodeName = "Underlying",
                               const std::string& basicUnderlyingNodeName = "Name");
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/accumulator.hpp

%shared_ptr(Accumulator)
class Accumulator : public Trade {
public:
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityAccumulator)
class EquityAccumulator : public Accumulator {
public:
    EquityAccumulator();
};

%shared_ptr(FxAccumulator)
class FxAccumulator : public Accumulator {
public:
    FxAccumulator();
};

%shared_ptr(CommodityAccumulator)
class CommodityAccumulator : public Accumulator {
public:
    CommodityAccumulator();
};

// ore/OREData/ored/portfolio/additionalfieldgetter.hpp

%shared_ptr(PortfolioFieldGetter)
class PortfolioFieldGetter {
public:
    PortfolioFieldGetter(const ext::shared_ptr<Portfolio>& portfolio,
                         const std::set<std::string>& baseFieldNames = {}, bool addExtraFields = true);
};

// ore/OREData/ored/portfolio/asianoption.hpp

%shared_ptr(AsianOption)
class AsianOption : public Trade {
public:
    explicit AsianOption(const string& tradeType);
    AsianOption(const Envelope& env, const string& tradeType, const double quantity, const TradeStrike& strike,
                const OptionData& option, const ScheduleData& observationDates,
                const ext::shared_ptr<Underlying>& underlying, const Date& settlementDate,
                const std::string& currency);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityAsianOption)
class EquityAsianOption : public AsianOption {
public:
    EquityAsianOption();
};

%shared_ptr(FxAsianOption)
class FxAsianOption : public AsianOption {
public:
    FxAsianOption();
};

%shared_ptr(CommodityAsianOption)
class CommodityAsianOption : public AsianOption {
public:
    CommodityAsianOption();
};

// ore/OREData/ored/portfolio/autocallable_01.hpp

%shared_ptr(Autocallable_01)
class Autocallable_01 : public ScriptedTrade {
public:
    Autocallable_01();
    Autocallable_01(const Envelope& env, const string& notionalAmount, const string& determinationLevel,
                    const string& triggerLevel, const ext::shared_ptr<Underlying>& underlying, const string& position,
                    const string& payCcy, const ScheduleData& fixingDates, const ScheduleData& settlementDates,
                    const vector<string>& accumulationFactors, const string& cap);
};

// ore/OREData/ored/portfolio/balanceguaranteedswap.hpp

%shared_ptr(BGSTrancheData)
class BGSTrancheData : public ore::data::XMLSerializable {
public:
    BGSTrancheData();
    BGSTrancheData(const std::string& description, const std::string& securityId, const int seniority,
                   const std::vector<QuantLib::Real>& notionals, const std::vector<std::string>& notionalDates);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(BGSTrancheDataVector) vector<ext::shared_ptr<BGSTrancheData>>;

%shared_ptr(BalanceGuaranteedSwap)
class BalanceGuaranteedSwap : public ore::data::Trade {
public:
    BalanceGuaranteedSwap();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend BalanceGuaranteedSwap {
    BalanceGuaranteedSwap(const Envelope& env, const std::string& referenceSecurity,
                          const std::vector<ext::shared_ptr<BGSTrancheData>>& tranches, const Schedule schedule,
                          const std::vector<ext::shared_ptr<LegData>>& swap) {
        return new BalanceGuaranteedSwap(env, referenceSecurity,
            VECTOR_SWIG_TO_ORE(tranches), schedule, VECTOR_SWIG_TO_ORE(swap));
    }
}

// ore/OREData/ored/portfolio/barrieroption.hpp

// Pure virtual class, partially exported so that derived classes can inherit from it
class OREBarrierOption : virtual public ore::data::Trade {
public:
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    virtual void checkBarriers() = 0;
    virtual QuantLib::ext::shared_ptr<QuantLib::Index> getIndex() const = 0;
    virtual const QuantLib::Real strike() const = 0;
    virtual QuantLib::Real tradeMultiplier() = 0;
    virtual Currency tradeCurrency() = 0;
    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    vanillaPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate) = 0;
    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    barrierPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate) = 0;
    virtual const QuantLib::Handle<QuantLib::Quote>& spotQuote() = 0;
    virtual void additionalFromXml(ore::data::XMLNode* node) = 0;
    virtual void additionalToXml(ore::data::XMLDocument& doc, ore::data::XMLNode* node) const = 0;
    virtual std::string indexFixingName() = 0;
};

// Pure virtual class, partially exported so that derived classes can inherit from it
class FxOptionWithBarrier : /*public FxSingleAssetDerivative,*/ public OREBarrierOption {
    void additionalFromXml(ore::data::XMLNode* node) override;
    void additionalToXml(ore::data::XMLDocument& doc, ore::data::XMLNode* node) const override;
    QuantLib::ext::shared_ptr<QuantLib::Index> getIndex() const override;
    const QuantLib::Real strike() const override;
    QuantLib::Real tradeMultiplier() override;
    Currency tradeCurrency() override;
    const QuantLib::Handle<QuantLib::Quote>& spotQuote() override;
    std::string indexFixingName() override;
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

// Pure virtual class, partially exported so that derived classes can inherit from it
class EquityOptionWithBarrier : /*public EquitySingleAssetDerivative,*/ public OREBarrierOption {
    void additionalFromXml(ore::data::XMLNode* node) override;
    void additionalToXml(ore::data::XMLDocument& doc, ore::data::XMLNode* node) const override;
    QuantLib::ext::shared_ptr<QuantLib::Index> getIndex() const override;
    const QuantLib::Real strike() const override;
    QuantLib::Real tradeMultiplier() override;
    Currency tradeCurrency() override;
    const QuantLib::Handle<QuantLib::Quote>& spotQuote() override;
    std::string indexFixingName() override;
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/optionwrapper.hpp

// Abstract base class
%shared_ptr(OptionWrapper)
class OptionWrapper : public InstrumentWrapper {};

%shared_ptr(EuropeanOptionWrapper)
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

%shared_ptr(AmericanOptionWrapper)
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

%shared_ptr(BermudanOptionWrapper)
class BermudanOptionWrapper : public OptionWrapper {
public:
    BermudanOptionWrapper(const ext::shared_ptr<Instrument>& inst, const bool isLongOption,
                          const std::vector<Date>& exerciseDates,
                          const std::vector<Date>& settlementDates,
                          const bool isPhysicalDelivery,
                          const std::vector<ext::shared_ptr<Instrument>>& undInsts,
                          // multiplier as seen from the option holder
                          const Real multiplier = 1.0,
                          // undMultiplier w.r.t. underlying as seen from the option holder
                          const Real undMultiplier = 1.0,
                          const std::vector<ext::shared_ptr<Instrument>>& additionalInstruments =
                              std::vector<ext::shared_ptr<Instrument>>(),
                          const std::vector<Real>& additionalMultipliers = std::vector<Real>());
    bool exercise() const override;
};

// ore/OREData/ored/portfolio/basketoption.hpp

%shared_ptr(OREBasketOption)
class OREBasketOption : public ScriptedTrade {
public:
    explicit OREBasketOption(const std::string& tradeType = "BasketOption");
    OREBasketOption(const std::string& currency, const std::string& notional, const TradeStrike& strike,
                 const std::vector<ext::shared_ptr<Underlying>>& underlyings, const OptionData& optionData,
                 const std::string& settlement, const ScheduleData& observationDates);
};

%shared_ptr(EquityBasketOption)
class EquityBasketOption : public OREBasketOption {
public:
    EquityBasketOption();
};

%shared_ptr(FxBasketOption)
class FxBasketOption : public OREBasketOption {
public:
    FxBasketOption();
};

%shared_ptr(CommodityBasketOption)
class CommodityBasketOption : public OREBasketOption {
public:
    CommodityBasketOption();
};

// ore/OREData/ored/portfolio/basketvarianceswap.hpp

%shared_ptr(BasketVarianceSwap)
class BasketVarianceSwap : public ScriptedTrade {
public:
    explicit BasketVarianceSwap(const string& tradeType = "BasketVarianceSwap");
    BasketVarianceSwap(const Envelope& env, const string& longShort, const string& notional, const string& strike,
                       const string& currency, const string& cap, const string& floor, const string& settlementDate,
                       const ScheduleData& valuationSchedule, bool squaredPayoff,
                       const vector<ext::shared_ptr<Underlying>> underlyings);
};

%shared_ptr(EquityBasketVarianceSwap)
class EquityBasketVarianceSwap : public BasketVarianceSwap {
public:
    EquityBasketVarianceSwap();
};

%shared_ptr(FxBasketVarianceSwap)
class FxBasketVarianceSwap : public BasketVarianceSwap {
public:
    FxBasketVarianceSwap();
};

%shared_ptr(CommodityBasketVarianceSwap)
class CommodityBasketVarianceSwap : public BasketVarianceSwap {
public:
    CommodityBasketVarianceSwap();
};

// ore/OREData/ored/portfolio/bestentryoption.hpp

%shared_ptr(BestEntryOption)
class BestEntryOption : public ScriptedTrade {
public:
    explicit BestEntryOption(const string& tradeType = "BestEntryOption");
    BestEntryOption(const Envelope& env, const string& longShort, const string& notional, const string& multiplier,
                    const string& strike, const string& cap, const string& resetMinimum, const string& triggerLevel,
                    const ext::shared_ptr<Underlying> underlying, const string& currency,
                    const ScheduleData& observationDates, const string& premiumDate);
};

%shared_ptr(EquityBestEntryOption)
class EquityBestEntryOption : public BestEntryOption {
public:
    EquityBestEntryOption();
};

%shared_ptr(FxBestEntryOption)
class FxBestEntryOption : public BestEntryOption {
public:
    FxBestEntryOption();
};

%shared_ptr(CommodityBestEntryOption)
class CommodityBestEntryOption : public BestEntryOption {
public:
    CommodityBestEntryOption();
};

// ore/OREData/ored/portfolio/bondbasket.hpp

%shared_ptr(BondBasket)
class BondBasket : public XMLSerializable {
public:
    BondBasket();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/bondfuture.hpp

%shared_ptr(BondFuture)
class BondFuture : public Trade {
public:
    BondFuture();
    BondFuture(const string& contractName, Real contractNotional, const std::string longShort = "Long",
               Envelope env = Envelope());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/bondoption.hpp

%shared_ptr(BondOption)
class BondOption : public Trade {
public:
    BondOption();
    BondOption(Envelope env, const BondData& bondData, const OptionData& optionData, TradeStrike strike, bool knocksOut);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/bondposition.hpp

%shared_ptr(BondPositionData)
class BondPositionData : public XMLSerializable {
public:
    BondPositionData();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend BondPositionData {
    BondPositionData(const Real quantity, const std::vector<ext::shared_ptr<BondUnderlying>>& underlyings) {
        return new BondPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
    }
}

%shared_ptr(BondPosition)
class BondPosition : public Trade {
public:
    BondPosition();
    BondPosition(const Envelope& env, const BondPositionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// Add support for a vector of Bond, which was defined in
// ore/ORE-SWIG/QuantLib-SWIG/swig/bonds.i
%template(BondVector) vector<ext::shared_ptr<Bond>>;

%shared_ptr(BondPositionInstrumentWrapper)
class BondPositionInstrumentWrapper : public InstrumentWrapper {
public:
    BondPositionInstrumentWrapper(const Real quantity, const std::vector<ext::shared_ptr<Bond>>& bonds,
                                  const std::vector<Real>& weights, const std::vector<Real>& bidAskAdjstments,
                                  const std::vector<Handle<Quote>>& fxConversion = {});
};

// ore/OREData/ored/portfolio/bondrepo.hpp

%shared_ptr(BondRepo)
class BondRepo : public Trade {
public:
    BondRepo();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/bondtotalreturnswap.hpp

%shared_ptr(BondTRS)
class BondTRS : public Trade {
public:
    BondTRS();
    BondTRS(Envelope env, const BondData& bondData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/callablebond.hpp

%shared_ptr(CallableBondData)
class CallableBondData : public ore::data::XMLSerializable {
public:
    CallableBondData();
    explicit CallableBondData(const BondData& bondData);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECallableBond)
class ORECallableBond : public Trade {
public:
    ORECallableBond();
    ORECallableBond(const Envelope& env, const CallableBondData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/callableswap.hpp

%shared_ptr(CallableSwap)
class CallableSwap : public ore::data::Trade {
public:
    CallableSwap();
    CallableSwap(const Envelope& env, const ORESwap& swap, const ORESwaption& swaption);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/cashposition.hpp

%shared_ptr(CashPosition)
class CashPosition : public Trade {
public:
    CashPosition();
    CashPosition(const Envelope& env, const string& currency, double amount);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/cbo.hpp

%shared_ptr(CBO)
class CBO : public Trade {
public:
    CBO();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/cliquetoption.hpp

%shared_ptr(ORECliquetOption)
class ORECliquetOption : public ore::data::Trade {
public:
    ORECliquetOption(const std::string& tradeType);
    ORECliquetOption(const std::string& tradeType, Envelope& env,
                  const ext::shared_ptr<Underlying>& underlying, std::string currency,
                  QuantLib::Real notional, std::string longShort, std::string callPut,
                  ScheduleData scheduleData, QuantLib::Real moneyness = 1.0,
                  QuantLib::Real localCap = QuantLib::Null<QuantLib::Real>(),
                  QuantLib::Real localFloor = QuantLib::Null<QuantLib::Real>(),
                  QuantLib::Real globalCap = QuantLib::Null<QuantLib::Real>(),
                  QuantLib::Real globalFloor = QuantLib::Null<QuantLib::Real>(), QuantLib::Size settlementDays = 0,
                  QuantLib::Real premium = 0.0, std::string premiumCcy = "", std::string premiumPayDate = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityCliquetOption)
class EquityCliquetOption : public ORECliquetOption {
public:
    EquityCliquetOption();
    EquityCliquetOption(const std::string& tradeType, Envelope& env,
                        const ext::shared_ptr<Underlying>& underlying, std::string currency,
                        QuantLib::Real notional, std::string longShort, std::string callPut,
                        ScheduleData scheduleData, QuantLib::Real moneyness = 1.0,
                        QuantLib::Real localCap = QuantLib::Null<QuantLib::Real>(),
                        QuantLib::Real localFloor = QuantLib::Null<QuantLib::Real>(),
                        QuantLib::Real globalCap = QuantLib::Null<QuantLib::Real>(),
                        QuantLib::Real globalFloor = QuantLib::Null<QuantLib::Real>(),
                        QuantLib::Size settlementDays = 0, QuantLib::Real premium = 0.0, std::string premiumCcy = "",
                        std::string premiumPayDate = "");
};

// ore/OREData/ored/portfolio/collateralbalance.hpp

%shared_ptr(CollateralBalance)
class CollateralBalance : public ore::data::XMLSerializable {
public:
    CollateralBalance();
    CollateralBalance(ore::data::XMLNode* node);
    CollateralBalance(const NettingSetDetails& nettingSetDetails, const std::string& currency,
                      const QuantLib::Real& im, const QuantLib::Real& vm = QuantLib::Null<QuantLib::Real>());
    CollateralBalance(const std::string& nettingSetId, const std::string& currency,
                      const QuantLib::Real& im, const QuantLib::Real& vm = QuantLib::Null<QuantLib::Real>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CollateralBalances)
class CollateralBalances : public ore::data::XMLSerializable {
public:
    CollateralBalances();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commodityapo.hpp

%shared_ptr(CommodityAveragePriceOption)
class CommodityAveragePriceOption : public Trade {
public:
    CommodityAveragePriceOption();
    CommodityAveragePriceOption(
        const Envelope& envelope, const OptionData& optionData, Real quantity,
        Real strike, const std::string& currency, const std::string& name, CommodityPriceType priceType,
        const std::string& startDate, const std::string& endDate, const std::string& paymentCalendar,
        const std::string& paymentLag, const std::string& paymentConvention, const std::string& pricingCalendar,
        const std::string& paymentDate = "", Real gearing = 1.0, Spread spread = 0.0,
        QuantExt::CommodityQuantityFrequency commodityQuantityFrequency =
            QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
        CommodityPayRelativeTo commodityPayRelativeTo = CommodityPayRelativeTo::CalculationPeriodEndDate,
        Integer futureMonthOffset = 0, Natural deliveryRollDays = 0, bool includePeriodEnd = true,
        const BarrierData& barrierData = {}, const std::string& fxIndex = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commoditydigitalapo.hpp

%shared_ptr(CommodityDigitalAveragePriceOption)
class CommodityDigitalAveragePriceOption : public Trade {
public:
    CommodityDigitalAveragePriceOption();
    CommodityDigitalAveragePriceOption(
        const Envelope& envelope, const OptionData& optionData,
        QuantLib::Real strike, QuantLib::Real digitalCashPayoff, const std::string& currency, const std::string& name,
        CommodityPriceType priceType,
        const std::string& startDate, const std::string& endDate, const std::string& paymentCalendar,
        const std::string& paymentLag, const std::string& paymentConvention, const std::string& pricingCalendar,
        const std::string& paymentDate = "", QuantLib::Real gearing = 1.0, QuantLib::Spread spread = 0.0,
        QuantExt::CommodityQuantityFrequency commodityQuantityFrequency =
            QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
        CommodityPayRelativeTo commodityPayRelativeTo = CommodityPayRelativeTo::CalculationPeriodEndDate,
        QuantLib::Integer futureMonthOffset = 0, QuantLib::Natural deliveryRollDays = 0, bool includePeriodEnd = true,
        const BarrierData& barrierData = {}, const std::string& fxIndex = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commoditydigitaloption.hpp

%shared_ptr(CommodityDigitalOption)
class CommodityDigitalOption : public ore::data::Trade {
public:
    CommodityDigitalOption();
    CommodityDigitalOption(const Envelope& env, const OptionData& optionData, const std::string& commodityName,
               const std::string& currency, Real strike, Real payoff,
               const ext::optional<bool>& isFuturePrice = ext::nullopt,
               const Date& futureExpiryDate = Date());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commoditylegbuilder.hpp

%shared_ptr(CommodityFixedLegBuilder)
class CommodityFixedLegBuilder : public ore::data::LegBuilder {
public:
    CommodityFixedLegBuilder();
};

%shared_ptr(CommodityFloatingLegBuilder)
class CommodityFloatingLegBuilder : public ore::data::LegBuilder {
public:
    CommodityFloatingLegBuilder();
};

// ore/OREData/ored/portfolio/commodityoption.hpp

%shared_ptr(CommodityOption)
class CommodityOption : public VanillaOptionTrade {
public:
    CommodityOption();
    CommodityOption(const Envelope& env, const OptionData& optionData, const std::string& commodityName,
                    const std::string& currency, QuantLib::Real quantity, TradeStrike strike,
                    const QuantLib::ext::optional<bool>& isFuturePrice = QuantLib::ext::nullopt,
                    const QuantLib::Date& futureExpiryDate = QuantLib::Date());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commodityoptionstrip.hpp

%template(PositionTypeVector) std::vector<Position::Type>;
%shared_ptr(CommodityOptionStrip)
class CommodityOptionStrip : public Trade {
public:
    CommodityOptionStrip();
    CommodityOptionStrip(const Envelope& envelope, const LegData& legData,
                         const std::vector<Position::Type>& callPositions,
                         const std::vector<Real>& callStrikes,
                         const std::vector<Position::Type>& putPositions,
                         const std::vector<Real>& putStrikes, Real premium = 0.0,
                         const std::string& premiumCurrency = "",
                         const Date& premiumPayDate = Date(), const std::string& style = "",
                         const std::string& settlement = "", const BarrierData& callBarrierData = {},
                         const BarrierData& putBarrierData = {},
                         const std::string& fxIndex = "",
                         const bool isDigital = false,
                         Real payoffPerUnit = 0.0);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commodityposition.hpp

%shared_ptr(CommodityPositionData)
class CommodityPositionData : public XMLSerializable {
public:
    CommodityPositionData();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend CommodityPositionData {
    CommodityPositionData(const Real quantity, const std::vector<ext::shared_ptr<CommodityUnderlying>>& underlyings) {
        return new CommodityPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
    }
}

%shared_ptr(CommodityPosition)
class CommodityPosition : public Trade {
public:
    CommodityPosition();
    CommodityPosition(const Envelope& env, const CommodityPositionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%template(CommodityIndexVector) std::vector<ext::shared_ptr<CommodityIndex>>;
%shared_ptr(CommodityPositionInstrumentWrapper)
class CommodityPositionInstrumentWrapper : public Instrument {
public:
    CommodityPositionInstrumentWrapper(const Real quantity,
                                       const std::vector<ext::shared_ptr<CommodityIndex>>& commodities,
                                       const std::vector<Real>& weights,
                                       const std::vector<Handle<Quote>>& fxConversion = {});
};

// ore/OREData/ored/portfolio/commodityspreadoption.hpp

%shared_ptr(CommoditySpreadOptionData)
class CommoditySpreadOptionData : public XMLSerializable {
public:
    CommoditySpreadOptionData();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend CommoditySpreadOptionData {
    CommoditySpreadOptionData(const std::vector<ext::shared_ptr<LegData>>& legData, const OptionData& optionData,
                              Real strike) {
        return new CommoditySpreadOptionData(VECTOR_SWIG_TO_ORE(legData), optionData, strike);
    }
}

%shared_ptr(CommoditySpreadOption)
class CommoditySpreadOption : public ore::data::Trade {
public:
    CommoditySpreadOption();
    CommoditySpreadOption(const CommoditySpreadOptionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commodityswap.hpp

%shared_ptr(CommoditySwap)
class CommoditySwap : public ore::data::Trade {
public:
    CommoditySwap();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend CommoditySwap {
    CommoditySwap(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& legs) {
        return new CommoditySwap(env, VECTOR_SWIG_TO_ORE(legs));
    }
}

// ore/OREData/ored/portfolio/commodityswaption.hpp

%shared_ptr(CommoditySwaption)
class CommoditySwaption : public ore::data::Trade {
public:
    CommoditySwaption();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend CommoditySwaption {
    CommoditySwaption(const Envelope& env, const OptionData& option,
                      const std::vector<ext::shared_ptr<LegData>>& legData) {
        return new CommoditySwaption(env, option, VECTOR_SWIG_TO_ORE(legData));
    }
}

// ore/OREData/ored/portfolio/compositeinstrumentwrapper.hpp

%shared_ptr(CompositeInstrumentWrapper)
class CompositeInstrumentWrapper : public ore::data::InstrumentWrapper {
public:
    CompositeInstrumentWrapper(const std::vector<ext::shared_ptr<InstrumentWrapper>>& wrappers,
                               const std::vector<Handle<Quote>>& fxRates = {}, const Date& valuationDate = Date());
};

// ore/OREData/ored/portfolio/compositetrade.hpp

%shared_ptr(CompositeTrade)
class CompositeTrade : public Trade {
public:
    CompositeTrade(const Envelope& env = Envelope(), const TradeActions& ta = TradeActions());
    CompositeTrade(const string currency, const vector<ext::shared_ptr<Trade>>& trades,
                   const string notionalCalculation = "", const Real notionalOverride = 0.0,
                   const Envelope& env = Envelope(), const TradeActions& ta = TradeActions(), const double indexQuantity=Null<Real>());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/convertiblebonddata.hpp

%shared_ptr(ConvertibleBondData)
class ConvertibleBondData : public ore::data::XMLSerializable {
public:
    ConvertibleBondData();
    explicit ConvertibleBondData(const BondData& bondData);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/convertiblebond.hpp

%shared_ptr(ConvertibleBond)
class ConvertibleBond : public Trade {
public:
    ConvertibleBond();
    ConvertibleBond(const Envelope& env, const ConvertibleBondData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/ascot.hpp

%shared_ptr(Ascot)
class Ascot : public Trade {
public:
    Ascot();
    Ascot(const Envelope& env, const ConvertibleBond& bond, const OptionData& optionData,
          const LegData& fundingLegData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/convertiblebondreferencedata.hpp

%shared_ptr(ConvertibleBondReferenceDatum)
class ConvertibleBondReferenceDatum : public ReferenceDatum {
public:
    ConvertibleBondReferenceDatum();
    ConvertibleBondReferenceDatum(const string& id);
    // FIXME - export nested classes from ore/OREData/ored/portfolio/convertiblebonddata.hpp?
    //ConvertibleBondReferenceDatum(const string& id, const BondReferenceDatum::BondData& bondData,
    //                              const ConvertibleBondData::CallabilityData& callData,
    //                              const ConvertibleBondData::CallabilityData& putData,
    //                              const ConvertibleBondData::ConversionData& conversionData,
    //                              const ConvertibleBondData::DividendProtectionData& dividendProtectionData);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/counterpartycorrelationmatrix.hpp

%shared_ptr(CounterpartyCorrelationMatrix)
class CounterpartyCorrelationMatrix : public ore::data::XMLSerializable {
public:
    CounterpartyCorrelationMatrix(ore::data::XMLNode* node);
    CounterpartyCorrelationMatrix();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/counterpartyinformation.hpp

enum class CounterpartyCreditQuality { IG, HY, NR };

%shared_ptr(CounterpartyInformation)
class CounterpartyInformation : public ore::data::XMLSerializable {
public:
    CounterpartyInformation(ore::data::XMLNode* node);
    CounterpartyInformation(std::string counterpartyId, bool isClearingCP = false,
        CounterpartyCreditQuality creditQuality = CounterpartyCreditQuality::NR,
        QuantLib::Real baCvaRiskWeight = QuantLib::Null<QuantLib::Real>(),
        QuantLib::Real saCcrRiskWeight = QuantLib::Null<QuantLib::Real>(), std::string saCvaRiskBucket = "" );
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/counterpartymanager.hpp

%shared_ptr(CounterpartyManager)
class CounterpartyManager : public ore::data::XMLSerializable {
public:
    CounterpartyManager() {}
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/creditdefaultswapoption.hpp

%shared_ptr(CreditDefaultSwapOption)
%shared_ptr(CreditDefaultSwapOption::AuctionSettlementInformation)
class CreditDefaultSwapOption : public Trade {
public:
    class AuctionSettlementInformation : public XMLSerializable {
    public:
        AuctionSettlementInformation();
        // The underlying C++ code declares parameter auctionFinalPrice as a non const reference,
        // which seems wrong.  This causes an error in the SWIG wrapper.  As a workaround,
        // comment out the reference, to trick SWIG into thinking that it is pass by value.
        AuctionSettlementInformation(const Date& auctionSettlementDate,
            Real /*&*/ auctionFinalPrice);
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(XMLDocument& doc) const override;
    };
    CreditDefaultSwapOption();
    CreditDefaultSwapOption(const Envelope& env,
        const OptionData& option,
        const CreditDefaultSwapData& swap,
        Real strike = Null<Real>(),
        const std::string& strikeType = "Spread",
        bool knockOut = true,
        const std::string& term = "",
        const ext::optional<AuctionSettlementInformation>& asi = ext::nullopt);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/creditlinkedswap.hpp

%shared_ptr(CreditLinkedSwap)
class CreditLinkedSwap : public Trade {
public:
    CreditLinkedSwap();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend CreditLinkedSwap {
    CreditLinkedSwap(const std::string& creditCurveId, const bool settlesAccrual, const Real fixedRecoveryRate,
                     const QuantExt::CreditDefaultSwap::ProtectionPaymentTime& defaultPaymentTime,
                     const std::vector<ext::shared_ptr<LegData>>& independentPayments,
                     const std::vector<ext::shared_ptr<LegData>>& contingentPayments,
                     const std::vector<ext::shared_ptr<LegData>>& defaultPayments,
                     const std::vector<ext::shared_ptr<LegData>>& recoveryPayments) {
        return new CreditLinkedSwap(creditCurveId, settlesAccrual, fixedRecoveryRate, defaultPaymentTime,
            VECTOR_SWIG_TO_ORE(independentPayments), VECTOR_SWIG_TO_ORE(contingentPayments),
            VECTOR_SWIG_TO_ORE(defaultPayments), VECTOR_SWIG_TO_ORE(recoveryPayments));
    }
}

// ore/OREData/ored/portfolio/crosscurrencyswap.hpp

%shared_ptr(CrossCurrencySwap)
class CrossCurrencySwap : public ORESwap {
public:
    CrossCurrencySwap();
    CrossCurrencySwap(const Envelope& env, const LegData& leg0, const LegData& leg1);
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend CrossCurrencySwap {
    CrossCurrencySwap(const Envelope& env, const vector<ext::shared_ptr<LegData>>& legData) {
        return new CrossCurrencySwap(env, VECTOR_SWIG_TO_ORE(legData));
    }
}

// ore/OREData/ored/portfolio/doubledigitaloption.hpp

%shared_ptr(DoubleDigitalOption)
class DoubleDigitalOption : public ScriptedTrade {
public:
    DoubleDigitalOption();
    DoubleDigitalOption(const Envelope& env, const string& expiry, const string& settlement, const string& binaryPayout,
                        const string& binaryLevel1, const string& binaryLevel2, const string& type1,
                        const string& type2, const string& position,
                        const ext::shared_ptr<Underlying>& underlying1,
                        const ext::shared_ptr<Underlying>& underlying2,
                        const ext::shared_ptr<Underlying>& underlying3,
                        const ext::shared_ptr<Underlying>& underlying4, const string& payCcy,
                        const ext::shared_ptr<Conventions>& conventions = nullptr,
                        const std::string& binaryLevelUpper1 = std::string(),
                        const std::string& binaryLevelUpper2 = std::string());
};

// ore/OREData/ored/portfolio/durationadjustedcmslegbuilder.hpp

%shared_ptr(DurationAdjustedCmsLegBuilder)
class DurationAdjustedCmsLegBuilder : public ore::data::LegBuilder {
public:
    DurationAdjustedCmsLegBuilder();
};

// ore/OREData/ored/portfolio/durationadjustedcmslegdata.hpp

%shared_ptr(DurationAdjustedCmsLegData)
class DurationAdjustedCmsLegData : public ore::data::LegAdditionalData {
public:
    DurationAdjustedCmsLegData();
    DurationAdjustedCmsLegData(const std::string& swapIndex, Size duration, Size fixingDays, bool isInArrears,
                               const std::vector<double>& spreads,
                               const std::vector<std::string>& spreadDates = std::vector<std::string>(),
                               const std::vector<double>& caps = std::vector<double>(),
                               const std::vector<std::string>& capDates = std::vector<std::string>(),
                               const std::vector<double>& floors = std::vector<double>(),
                               const std::vector<std::string>& floorDates = std::vector<std::string>(),
                               const std::vector<double>& gearings = std::vector<double>(),
                               const std::vector<std::string>& gearingDates = std::vector<std::string>(),
                               bool nakedOption = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equitybarrieroption.hpp

%shared_ptr(EquityBarrierOption)
class EquityBarrierOption : public EquityOptionWithBarrier {
public:
    EquityBarrierOption();
    EquityBarrierOption(Envelope& env, OptionData option, BarrierData barrier, QuantLib::Date startDate,
                        std::string calendar, EquityUnderlying equityUnderlying, QuantLib::Currency currency,
                        QuantLib::Real quantity,TradeStrike strike);
    void checkBarriers() override;
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    vanillaPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    barrierPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
};

%shared_ptr(EquityDigitalOption)
class EquityDigitalOption : public /*EquitySingleAssetDerivative*/ ore::data::Trade {
public:
    EquityDigitalOption();
    EquityDigitalOption(Envelope& env, OptionData option, double strike, const string& payoffCurrency, double payoffAmount,
                    const EquityUnderlying& equityUnderlying, double quantity);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equitydoublebarrieroption.hpp

%shared_ptr(EquityDoubleBarrierOption)
class EquityDoubleBarrierOption : public /*EquityOptionWithBarrier*/ ore::data::Trade {
public:
    EquityDoubleBarrierOption();
    EquityDoubleBarrierOption(Envelope& env, OptionData option, BarrierData barrier, QuantLib::Date startDate,
                              std::string calendar, EquityUnderlying equityUnderlying, QuantLib::Currency currency,
                              QuantLib::Real quantity, TradeStrike strike);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equitydoubletouchoption.hpp

%shared_ptr(EquityDoubleTouchOption)
class EquityDoubleTouchOption : public /*EquitySingleAssetDerivative*/ ore::data::Trade {
public:
    EquityDoubleTouchOption();
    EquityDoubleTouchOption(Envelope& env, OptionData option, BarrierData barrier,
                            const EquityUnderlying& equityUnderlying, string payoffCurrency, double payoffAmount,
                            string startDate = "", string calendar = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityeuropeanbarrieroption.hpp

%shared_ptr(EquityEuropeanBarrierOption)
class EquityEuropeanBarrierOption : public /*ore::data::EquityOption*/ ore::data::Trade {
public:
    EquityEuropeanBarrierOption();
    EquityEuropeanBarrierOption(Envelope& env, OptionData option, BarrierData barrier,
                                EquityUnderlying equityUnderlying, string currency, TradeStrike strike, double quantity);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityfuturesoption.hpp

%shared_ptr(EquityFutureOption)
class EquityFutureOption : public VanillaOptionTrade {
public:
    EquityFutureOption();
    EquityFutureOption(Envelope& env, OptionData option, const string& currency, Real quantity,
        const ext::shared_ptr<Underlying>& underlying, TradeStrike strike, Date forwardDate,
        const ext::shared_ptr<Index>& index = nullptr, const std::string& indexName = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityfxlegbuilder.hpp

%shared_ptr(EquityMarginLegBuilder)
class EquityMarginLegBuilder : public ore::data::LegBuilder {
public:
    EquityMarginLegBuilder();
};

// ore/OREData/ored/portfolio/equityfxlegdata.hpp

%shared_ptr(EquityMarginLegData)
class EquityMarginLegData : public ore::data::LegAdditionalData {
public:
    EquityMarginLegData();
    EquityMarginLegData(QuantLib::ext::shared_ptr<ore::data::EquityLegData>& equityLegData, const vector<double>& rates,
        const vector<string>& rateDates = vector<string>(), const double& initialMarginFactor = QuantExt::Null<double>(),
        const double& multiplier = QuantExt::Null<double>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityoptionposition.hpp

%shared_ptr(EquityOptionUnderlyingData)
class EquityOptionUnderlyingData : public XMLSerializable {
public:
    EquityOptionUnderlyingData();
    EquityOptionUnderlyingData(const EquityUnderlying& underlying, const OptionData& optionData, const Real strike);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(EquityOptionUnderlyingDataVector) vector<ext::shared_ptr<EquityOptionUnderlyingData>>;

%shared_ptr(EquityOptionPositionData)
class EquityOptionPositionData : public XMLSerializable {
public:
    EquityOptionPositionData();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend EquityOptionPositionData {
    EquityOptionPositionData(const Real quantity, const std::vector<ext::shared_ptr<EquityOptionUnderlyingData>>& underlyings) {
        return new EquityOptionPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
    }
}

%shared_ptr(EquityOptionPosition)
class EquityOptionPosition : public Trade {
public:
    EquityOptionPosition();
    EquityOptionPosition(const Envelope& env, const EquityOptionPositionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// Add support for a vector of VanillaOption, which was defined in
// ore/ORE-SWIG/QuantLib-SWIG/swig/options.i
%template(VanillaOptionVector) vector<ext::shared_ptr<VanillaOption>>;

%shared_ptr(EquityOptionPositionInstrumentWrapper)
class EquityOptionPositionInstrumentWrapper : public QuantLib::Instrument {
public:
    EquityOptionPositionInstrumentWrapper(const Real quantity,
                                          const std::vector<ext::shared_ptr<VanillaOption>>& options,
                                          const std::vector<Real>& positions,
                                          const std::vector<Real>& weights,
                                          const std::vector<Handle<Quote>>& fxConversion = {});
};

// ore/OREData/ored/portfolio/equityoutperformanceoption.hpp

%shared_ptr(EquityOutperformanceOption)
class EquityOutperformanceOption : public ore::data::Trade {
public:
    EquityOutperformanceOption();
    EquityOutperformanceOption(Envelope& env, OptionData option, const string& currency, Real notional,
        const ext::shared_ptr<Underlying>& underlying1,
        const ext::shared_ptr<Underlying>& underlying2,
        Real initialPrice1, Real initialPrice2, Real strike, const string& initialPriceCurrency1 = "",
        const string& initialPriceCurrency2 = "",
        Real knockInPrice = Null<Real>(), Real knockOutPrice = Null<Real>(), string fxIndex1 = "", string fxIndex2 = "" );
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityposition.hpp

%shared_ptr(EquityPositionData)
class EquityPositionData : public XMLSerializable {
public:
    EquityPositionData();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend EquityPositionData {
    EquityPositionData(const Real quantity, const std::vector<ext::shared_ptr<EquityUnderlying>>& underlyings) {
        return new EquityPositionData(quantity, VECTOR_SWIG_TO_ORE(underlyings));
    }
}

%shared_ptr(EquityPosition)
class EquityPosition : public Trade {
public:
    EquityPosition();
    EquityPosition(const Envelope& env, const EquityPositionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// Add support for a vector of EquityIndex2, which was defined in
// ore/ORE-SWIG/QuantExt-SWIG/swig/qle_indexes.i
%template(EquityIndex2Vector) vector<ext::shared_ptr<EquityIndex2>>;

%shared_ptr(EquityPositionInstrumentWrapper)
class EquityPositionInstrumentWrapper : public Instrument {
public:
    EquityPositionInstrumentWrapper(const Real quantity,
                                    const std::vector<ext::shared_ptr<EquityIndex2>>& equities,
                                    const std::vector<Real>& weights,
                                    const std::vector<Handle<Quote>>& fxConversion = {});
};

%shared_ptr(EquityOptionPositionInstrumentWrapper)
class EquityOptionPositionInstrumentWrapper : public Instrument {
public:
    EquityOptionPositionInstrumentWrapper(const Real quantity,
                                          const std::vector<ext::shared_ptr<VanillaOption>>& options,
                                          const std::vector<Real>& positions,
                                          const std::vector<Real>& weights,
                                          const std::vector<Handle<Quote>>& fxConversion = {});
};

// ore/OREData/ored/portfolio/equityswap.hpp

%shared_ptr(EquitySwap)
class EquitySwap : public ORESwap {
public:
    EquitySwap();
    EquitySwap(const Envelope& env, const LegData& leg0, const LegData& leg1);
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend EquitySwap {
    EquitySwap(const Envelope& env, const vector<ext::shared_ptr<LegData>>& legData) {
        return new EquitySwap(env, VECTOR_SWIG_TO_ORE(legData));
    }
}

// ore/OREData/ored/portfolio/equitytouchoption.hpp

%shared_ptr(EquityTouchOption)
class EquityTouchOption : public EquitySingleAssetDerivative {
public:
    EquityTouchOption();
    EquityTouchOption(Envelope& env, OptionData option, BarrierData barrier, const EquityUnderlying& equityUnderlying,
                      string payoffCurrency, double payoffAmount, string startDate = "", string calendar = "",
                      string eqIndex = "");
};

// ore/OREData/ored/portfolio/europeanoptionbarrier.hpp

%shared_ptr(EuropeanOptionBarrier)
class EuropeanOptionBarrier : public ScriptedTrade {
public:
    explicit EuropeanOptionBarrier(const ext::shared_ptr<Conventions>& conventions = nullptr);
    EuropeanOptionBarrier(const Envelope& env, const string& quantity, const string& putCall, const string& longShort,
                          const string& strike, const string& premiumAmount, const string& premiumCurrency,
                          const string& premiumDate, const string& optionExpiry,
                          const ext::shared_ptr<Underlying>& optionUnderlying,
                          const ext::shared_ptr<Underlying>& barrierUnderlying, const string& barrierLevel,
                          const string& barrierType, const string& barrierStyle, const string& settlementDate,
                          const string& payCcy, const ScheduleData& barrierSchedule,
                          const ext::shared_ptr<Conventions>& conventions = nullptr);
};

// ore/OREData/ored/portfolio/failedtrade.hpp

%shared_ptr(FailedTrade)
class FailedTrade : public ore::data::Trade {
public:
    FailedTrade();
    FailedTrade(const Envelope& env);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/flexiswap.hpp

%shared_ptr(FlexiSwap)
class FlexiSwap : public ore::data::Trade {
public:
    FlexiSwap();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend FlexiSwap {
    FlexiSwap(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& underlyingData,
              const std::map<std::string, std::pair<std::vector<double>, std::vector<std::string>>>& lowerNotionalBounds,
              const std::string& optionLongShort) {
        return new FlexiSwap(env, VECTOR_SWIG_TO_ORE(underlyingData), lowerNotionalBounds, lowerNotionalBoundsDates, optionLongShort);
    }
}

// ore/OREData/ored/portfolio/formulabasedlegbuilder.hpp

%shared_ptr(FormulaBasedLegBuilder)
class FormulaBasedLegBuilder : public ore::data::LegBuilder {
public:
    explicit FormulaBasedLegBuilder();
};

// ore/OREData/ored/portfolio/formulabasedlegdata.hpp

%shared_ptr(FormulaBasedLegData)
class FormulaBasedLegData : public LegAdditionalData {
public:
    FormulaBasedLegData();
    FormulaBasedLegData(const string& formulaBasedIndex, int fixingDays, bool isInArrears);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/forwardbond.hpp

%shared_ptr(ForwardBond)
class ForwardBond : public Trade {
public:
    ForwardBond();
    ForwardBond(Envelope env, const BondData& bondData, string fwdMaturityDate, string fwdSettlementDate,
                string settlement, string amount, string lockRate, string lockRateDayCounter, string settlementDirty,
                string compensationPayment, string compensationPaymentDate, string longInForward,
                string dv01 = string(), string knockOut = string());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/forwardrateagreement.hpp

%shared_ptr(OREForwardRateAgreement)
class OREForwardRateAgreement : public ORESwap {
public:
    OREForwardRateAgreement();
    OREForwardRateAgreement(Envelope& env, string longShort, string currency, string startDate, string endDate,
                         string index, double strike, double amount);
};

// ore/OREData/ored/portfolio/fxaverageforward.hpp

%shared_ptr(FxAverageForward)
 class FxAverageForward : public Trade {
 public:
     FxAverageForward();
     FxAverageForward(const Envelope& env, const ScheduleData& observationDates, const string& paymentDate,
              bool fixedPayer,
              const std::string& referenceCurrency, double referenceNotional,
              const std::string settlementCurrency, double settlementNotional,
              const std::string& fxIndex, const string& settlement = "Cash");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/fxdigitalbarrieroption.hpp

%shared_ptr(FxDigitalBarrierOption)
class FxDigitalBarrierOption : public FxSingleAssetDerivative {
public:
    FxDigitalBarrierOption();
    FxDigitalBarrierOption(Envelope& env, OptionData option, BarrierData barrier, double strike, double payoffAmount,
                           const string& foreignCurrency, const string& domesticCurrency, const string& startDate = "",
                           const string& calendar = "", const string& fxIndex = "", const string& payoffCurrency = "",
                           const string& fxIndexDailyLows = "", const string& fxIndexDailyHighs = "");
};

// ore/OREData/ored/portfolio/fxdigitaloption.hpp

%shared_ptr(FxDigitalOption)
class FxDigitalOption : public FxSingleAssetDerivative {
public:
    FxDigitalOption();
    FxDigitalOption(Envelope& env, OptionData option, double strike, const string& payoffCurrency, double payoffAmount,
                    const string& foreignCurrency, const string& domesticCurrency);
    FxDigitalOption(Envelope& env, OptionData option, double strike, double payoffAmount, const string& foreignCurrency,
                    const string& domesticCurrency);
};

// ore/OREData/ored/portfolio/fxdoublebarrieroption.hpp

%shared_ptr(FxDoubleBarrierOption)
class FxDoubleBarrierOption : public FxOptionWithBarrier {
public:
    FxDoubleBarrierOption();
    FxDoubleBarrierOption(Envelope& env, OptionData option, BarrierData barrier, Date startDate,
        string calendar, string boughtCurrency, Real boughtAmount, string soldCurrency,
        Real soldAmount, string fxIndex = "");
    void checkBarriers() override;
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    vanillaPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    barrierPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
};

// ore/OREData/ored/portfolio/fxdoubletouchoption.hpp

%shared_ptr(FxDoubleTouchOption)
class FxDoubleTouchOption : public FxSingleAssetDerivative {
public:
    FxDoubleTouchOption();
    FxDoubleTouchOption(Envelope& env, OptionData option, BarrierData barrier, string foreignCurrency,
                        string domesticCurrency, string payoffCurrency, double payoffAmount, string startDate = "",
                        string calendar = "", string fxIndex = "");
};

// ore/OREData/ored/portfolio/fxeuropeanbarrieroption.hpp

%shared_ptr(FxEuropeanBarrierOption)
class FxEuropeanBarrierOption : public FxSingleAssetDerivative {
public:
    FxEuropeanBarrierOption();
    FxEuropeanBarrierOption(Envelope& env, OptionData option, BarrierData barrier, string boughtCurrency,
                            double boughtAmount, string soldCurrency, double soldAmount, string startDate = "",
                            string calendar = "", string fxIndex = "");
};

// ore/OREData/ored/portfolio/fxkikobarrieroption.hpp

%shared_ptr(FxKIKOBarrierOption)
class FxKIKOBarrierOption : public FxSingleAssetDerivative {
public:
    FxKIKOBarrierOption();
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend FxKIKOBarrierOption {
    FxKIKOBarrierOption(Envelope& env, OptionData option, vector<ext::shared_ptr<BarrierData>> barriers, string boughtCurrency,
                        double boughtAmount, string soldCurrency, double soldAmount, string startDate = "",
                        string calendar = "", string fxIndex = "") {
        return new FxKIKOBarrierOption(env, option, VECTOR_SWIG_TO_ORE(barriers), boughtCurrency, boughtAmount,
            soldCurrency, soldAmount, startDate, calendar, fxIndex);
    }
}

// ore/OREData/ored/portfolio/fxswap.hpp

%shared_ptr(FxSwap)
class FxSwap : public Trade {
public:
    FxSwap();
    //! Constructor
    FxSwap(const Envelope& env, const string& nearDate, const string& farDate, const string& nearBoughtCurrency,
           double nearBoughtAmount, const string& nearSoldCurrency, double nearSoldAmount, double farBoughtAmount,
           double farSoldAmount, const string& settlement = "Physical");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/genericbarrieroption.hpp

%shared_ptr(GenericBarrierOption)
class GenericBarrierOption : public ScriptedTrade {
public:
    explicit GenericBarrierOption(const std::string& tradeType = "GenericBarrierOption");
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend GenericBarrierOption {
    GenericBarrierOption(ext::shared_ptr<Underlying>& underlying, const OptionData& optionData,
                         const std::vector<ext::shared_ptr<BarrierData>>& barriers, const ScheduleData& barrierMonitoringDates,
                         const BarrierData& transatlanticBarrier, const std::string& payCurrency,
                         const std::string& settlementDate, const std::string& quantity, const std::string& strike,
                         const std::string& amount, const std::string& kikoType) {
        return new GenericBarrierOption(underlying, optionData, VECTOR_SWIG_TO_ORE(barriers), barrierMonitoringDates,
            transatlanticBarrier, payCurrency, settlementDate, quantity, strike, amount, kikoType);
    }
    GenericBarrierOption(std::vector<ext::shared_ptr<Underlying>> underlyings, const OptionData& optionData,
                         const std::vector<ext::shared_ptr<BarrierData>>& barriers, const ScheduleData& barrierMonitoringDates,
                         const std::vector<ext::shared_ptr<BarrierData>>& transatlanticBarrier, const std::string& payCurrency,
                         const std::string& settlementDate, const std::string& quantity, const std::string& strike,
                         const std::string& amount, const std::string& kikoType) {
        return new GenericBarrierOption(underlyings, optionData, VECTOR_SWIG_TO_ORE(barriers), barrierMonitoringDates,
            VECTOR_SWIG_TO_ORE(transatlanticBarrier), payCurrency, settlementDate, quantity, strike, amount, kikoType);
    }
}

%shared_ptr(EquityGenericBarrierOption)
class EquityGenericBarrierOption : public GenericBarrierOption {
public:
    EquityGenericBarrierOption();
};

%shared_ptr(FxGenericBarrierOption)
class FxGenericBarrierOption : public GenericBarrierOption {
public:
    FxGenericBarrierOption();
};

%shared_ptr(CommodityGenericBarrierOption)
class CommodityGenericBarrierOption : public GenericBarrierOption {
public:
    CommodityGenericBarrierOption();
};

// ore/OREData/ored/portfolio/indexcreditdefaultswap.hpp

%shared_ptr(IndexCreditDefaultSwap)
class IndexCreditDefaultSwap : public Trade {
public:
    IndexCreditDefaultSwap();
    IndexCreditDefaultSwap(const Envelope& env, const IndexCreditDefaultSwapData& swap, const BasketData& basket);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/indexcreditdefaultswapoption.hpp

%shared_ptr(IndexCreditDefaultSwapOption)
class IndexCreditDefaultSwapOption : public Trade {
public:
    IndexCreditDefaultSwapOption();
    IndexCreditDefaultSwapOption(const Envelope& env, const IndexCreditDefaultSwapData& swap,
                                 const OptionData& option, Real strike,
                                 const std::string& indexTerm = "", const std::string& strikeType = "Spread",
                                 const Date& tradeDate = Date(), const Date& fepStartDate = Date());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/indexing.hpp

%shared_ptr(Indexing)
class Indexing : public XMLSerializable {
public:
    Indexing();
    explicit Indexing(const std::string& index, const string& indexFixingCalendar = "", const bool indexIsDirty = false,
                      const bool indexIsRelative = true, const bool indexIsConditionalOnSurvival = true,
                      const Real quantity = 1.0, const Real initialFixing = Null<Real>(),
                      const Real initialNotionalFixing = Null<Real>(),
                      const ScheduleData& valuationSchedule = ScheduleData(), const Size fixingDays = 0,
                      const string& fixingCalendar = "", const string& fixingConvention = "",
                      const bool inArrearsFixing = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/inflationswap.hpp

%shared_ptr(InflationSwap)
class InflationSwap : public ORESwap {
public:
    InflationSwap();
    InflationSwap(const Envelope& env, const LegData& leg0, const LegData& leg1);
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend InflationSwap {
    InflationSwap(const Envelope& env, const vector<ext::shared_ptr<LegData>>& legData) {
        return new InflationSwap(env, VECTOR_SWIG_TO_ORE(legData));
    }
}

// ore/OREData/ored/portfolio/instrumentwrapper.hpp

%shared_ptr(VanillaInstrument)
class VanillaInstrument : public InstrumentWrapper {
public:
    VanillaInstrument(const ext::shared_ptr<Instrument>& inst, const Real multiplier = 1.0,
                      const std::vector<ext::shared_ptr<Instrument>>& additionalInstruments =
                          std::vector<ext::shared_ptr<Instrument>>(),
                      const std::vector<Real>& additionalMultipliers = std::vector<Real>());
};

// ore/OREData/ored/portfolio/knockoutswap.hpp

%shared_ptr(KnockOutSwap)
class KnockOutSwap : public ScriptedTrade {
public:
    explicit KnockOutSwap(const std::string& tradeType = "KnockOutSwap");
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend KnockOutSwap {
    KnockOutSwap(const std::vector<ext::shared_ptr<LegData>>& legData, const BarrierData& barrierData,
                 const std::string& barrierStartDate) {
        return new KnockOutSwap(VECTOR_SWIG_TO_ORE(legData), barrierData, barrierStartDate);
    }
}

// ore/OREData/ored/portfolio/legbuilders.hpp

%shared_ptr(FixedLegBuilder)
class FixedLegBuilder : public LegBuilder {
public:
    FixedLegBuilder();
};

%shared_ptr(ZeroCouponFixedLegBuilder)
class ZeroCouponFixedLegBuilder : public LegBuilder {
public:
    ZeroCouponFixedLegBuilder();
};

%shared_ptr(FloatingLegBuilder)
class FloatingLegBuilder : public LegBuilder {
public:
    FloatingLegBuilder();
};

%shared_ptr(CashflowLegBuilder)
class CashflowLegBuilder : public LegBuilder {
public:
    CashflowLegBuilder();
};

%shared_ptr(CPILegBuilder)
class CPILegBuilder : public LegBuilder {
public:
    CPILegBuilder();
};

%shared_ptr(YYLegBuilder)
class YYLegBuilder : public LegBuilder {
public:
    YYLegBuilder();
};

%shared_ptr(CMSLegBuilder)
class CMSLegBuilder : public LegBuilder {
public:
    CMSLegBuilder();
};

%shared_ptr(CMBLegBuilder)
class CMBLegBuilder : public LegBuilder {
public:
    CMBLegBuilder();
};

%shared_ptr(DigitalCMSLegBuilder)
class DigitalCMSLegBuilder : public LegBuilder {
public:
    DigitalCMSLegBuilder();
};

%shared_ptr(CMSSpreadLegBuilder)
class CMSSpreadLegBuilder : public LegBuilder {
public:
    CMSSpreadLegBuilder();
};

%shared_ptr(DigitalCMSSpreadLegBuilder)
class DigitalCMSSpreadLegBuilder : public LegBuilder {
public:
    DigitalCMSSpreadLegBuilder();
};

%shared_ptr(EquityLegBuilder)
class EquityLegBuilder : public LegBuilder {
public:
    EquityLegBuilder();
};

// ore/OREData/ored/portfolio/multilegoption.hpp

%shared_ptr(MultiLegOption)
class MultiLegOption : public Trade {
public:
    MultiLegOption();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend MultiLegOption {
    MultiLegOption(const Envelope& env, const vector<ext::shared_ptr<LegData>>& underlyingData) {
        return new MultiLegOption(env, VECTOR_SWIG_TO_ORE(underlyingData));
    }
    MultiLegOption(const Envelope& env, const OptionData& optionData, const vector<ext::shared_ptr<LegData>>& underlyingData) {
        return new MultiLegOption(env, optionData, VECTOR_SWIG_TO_ORE(underlyingData));
    }
}

// ore/OREData/ored/portfolio/nettingsetdetails.hpp

%shared_ptr(NettingSetDetails)
class NettingSetDetails : public XMLSerializable {
public:
    NettingSetDetails();
    NettingSetDetails(const string& nettingSetId, const string& agreementType = "", const string& callType = "",
                      const string& initialMarginType = "", const string& legalEntityId = "");
    NettingSetDetails(const map<string, string>& nettingSetMap);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/nettingsetdefinition.hpp

%shared_ptr(CSA)
class CSA {
public:
    enum Type { Bilateral, CallOnly, PostOnly };
    CSA(const Type& type,
        const string& csaCurrency, // three letter ISO code
        const string& index, const Real& thresholdPay, const Real& thresholdRcv, const Real& mtaPay, const Real& mtaRcv,
        const Real& iaHeld, const string& iaType, const Period& marginCallFreq, const Period& marginPostFreq,
        const Period& mpr, const Real& collatSpreadPay, const Real& collatSpreadRcv,
        const vector<string>& eligCollatCcys, // vector of three letter ISO codes
        bool applyInitialMargin, Type initialMarginType, const bool calculateIMAmount, const bool calculateVMAmount,
        const string& nonExemptIMRegulations);
};

%shared_ptr(NettingSetDefinition)
class NettingSetDefinition : public XMLSerializable {
public:
    NettingSetDefinition();
    NettingSetDefinition(XMLNode* node);
    NettingSetDefinition(const NettingSetDetails& nettingSetDetails);
    NettingSetDefinition(const string& nettingSetId);
    NettingSetDefinition(const NettingSetDetails& nettingSetDetails, const string& bilateral,
                         const string& csaCurrency, // three letter ISO code
                         const string& index, const Real& thresholdPay, const Real& thresholdRcv, const Real& mtaPay,
                         const Real& mtaRcv, const Real& iaHeld, const string& iaType,
                         const string& marginCallFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& marginPostFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& mpr,            // e.g. "1D", "2W", "3M", "4Y"
                         const Real& collatSpreadPay, const Real& collatSpreadRcv,
                         const vector<string>& eligCollatCcys, // vector of three letter ISO codes
                         bool applyInitialMargin = false, const string& initialMarginType = "Bilateral",
                         const bool calculateIMAmount = false, const bool calculateVMAmount = false,
                         const string& nonExemptIMRegulations = "");
    NettingSetDefinition(const string& nettingSetId, const string& bilateral,
                         const string& csaCurrency, // three letter ISO code
                         const string& index, const Real& thresholdPay, const Real& thresholdRcv, const Real& mtaPay,
                         const Real& mtaRcv, const Real& iaHeld, const string& iaType,
                         const string& marginCallFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& marginPostFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& mpr,            // e.g. "1D", "2W", "3M", "4Y"
                         const Real& collatSpreadPay, const Real& collatSpreadRcv,
                         const vector<string>& eligCollatCcys, // vector of three letter ISO codes
                         bool applyInitialMargin = false, const string& initialMarginType = "Bilateral",
                         const bool calculateIMAmount = false, const bool calculateVMAmount = false,
                         const string& nonExemptIMRegulations = "");
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/nettingsetmanager.hpp

%shared_ptr(NettingSetManager)
class NettingSetManager : public XMLSerializable {
public:
    NettingSetManager();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/optiondata.hpp

%shared_ptr(ExerciseBuilder)
class ExerciseBuilder {
public:
    ExerciseBuilder(const OptionData& optionData, const std::vector<Leg> legs,
                    bool removeNoticeDatesAfterLastAccrualStart = true);
};

// ore/OREData/ored/portfolio/optionexercisedata.hpp

%shared_ptr(OptionExerciseData)
class OptionExerciseData : public XMLSerializable {
public:
    OptionExerciseData();
    OptionExerciseData(const std::string& date, const std::string& price);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/optionpaymentdata.hpp

%shared_ptr(OptionPaymentData)
class OptionPaymentData : public XMLSerializable {
public:
    OptionPaymentData();
    OptionPaymentData(const std::vector<std::string>& dates);
    OptionPaymentData(const std::string& lag, const std::string& calendar, const std::string& convention,
                      const std::string& relativeTo = "Expiry");
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/pairwisevarianceswap.hpp

// protected constructors
%shared_ptr(PairwiseVarSwap)
class PairwiseVarSwap : public Trade {
public:
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EqPairwiseVarSwap)
class EqPairwiseVarSwap : public PairwiseVarSwap {
public:
    EqPairwiseVarSwap();
    EqPairwiseVarSwap(Envelope& env, string longShort,
                      const vector<ext::shared_ptr<Underlying>>& underlyings, vector<double> underlyingStrikes,
                      vector<double> underlyingNotionals, double basketNotional, double basketStrike,
                      ScheduleData valuationSchedule, string currency, string settlementDate,
                      ScheduleData laggedValuationSchedule, double payoffLimit = 0.0, double cap = 0.0,
                      double floor = 0.0, int accrualLag = 1);
};

%shared_ptr(FxPairwiseVarSwap)
class FxPairwiseVarSwap : public PairwiseVarSwap {
public:
    FxPairwiseVarSwap();
    FxPairwiseVarSwap(Envelope& env, string longShort, const vector<ext::shared_ptr<Underlying>>& underlyings,
                      vector<double> underlyingStrikes, vector<double> underlyingNotionals, double basketNotional,
                      double basketStrike, ScheduleData valuationSchedule, string currency, string settlementDate,
                      ScheduleData laggedValuationSchedule, double payoffLimit = 0.0, double cap = 0.0,
                      double floor = 0.0, int accrualLag = 1);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/performanceoption_01.hpp

%shared_ptr(PerformanceOption_01)
class PerformanceOption_01 : public ScriptedTrade {
public:
    explicit PerformanceOption_01(const ext::shared_ptr<Conventions>& conventions = nullptr);
    PerformanceOption_01(const Envelope& env, const string& notionalAmount, const string& participationRate,
                         const string& valuationDate, const string& settlementDate,
                         const vector<ext::shared_ptr<Underlying>>& underlyings, const vector<string>& strikePrices,
                         const string& strike, const bool strikeIncluded, const string& position, const string& payCcy,
                         const ext::shared_ptr<Conventions>& conventions = nullptr);
};

// ore/OREData/ored/portfolio/rainbowoption.hpp

%shared_ptr(RainbowOption)
class RainbowOption : public ScriptedTrade {
public:
    explicit RainbowOption(const ext::shared_ptr<Conventions>& conventions = nullptr,
                           const std::string& tradeType = "RainbowOption");
    RainbowOption(const std::string& currency, const std::string& notional, const std::string& strike,
                  const std::vector<ext::shared_ptr<Underlying>>& underlyings, const OptionData& optionData,
                  const std::string& settlement);
};

%shared_ptr(EquityRainbowOption)
class EquityRainbowOption : public RainbowOption {
public:
    explicit EquityRainbowOption(const ext::shared_ptr<Conventions>& conventions = nullptr);
};

%shared_ptr(FxRainbowOption)
class FxRainbowOption : public RainbowOption {
public:
    explicit FxRainbowOption(const ext::shared_ptr<Conventions>& conventions = nullptr);
};

%shared_ptr(CommodityRainbowOption)
class CommodityRainbowOption : public RainbowOption {
public:
    explicit CommodityRainbowOption(const ext::shared_ptr<Conventions>& conventions = nullptr);
};

// ore/OREData/ored/portfolio/rangebound.hpp

%shared_ptr(RangeBound)
class RangeBound : public ore::data::XMLSerializable {
public:
    RangeBound();
    RangeBound(const Real from, const Real to, const Real leverage, const Real strike, const Real strikeAdjustment);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/referencedata.hpp

// Resolve conflict between ore::data::BondData and ore::data::BondReferenceDatum::BondData
%ignore ore::data::BondData;
%rename(BondReferenceDatum_BondData) BondReferenceDatum::BondData;

%shared_ptr(BondReferenceDatum)
%shared_ptr(BondReferenceDatum::BondData)
class BondReferenceDatum : public ReferenceDatum {
public:
    struct BondData : XMLSerializable {
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(XMLDocument& doc) const override;
    };
    BondReferenceDatum();
    BondReferenceDatum(const string& id);
    BondReferenceDatum(const string& id, const Date& validFrom);
    BondReferenceDatum(const string& id, const BondData& bondData);
    BondReferenceDatum(const string& id, const Date& validFrom, const BondData& bondData);
};

%shared_ptr(BondFutureReferenceDatum)
%shared_ptr(BondFutureReferenceDatum::BondFutureData)
class BondFutureReferenceDatum : public ReferenceDatum {
public:
    struct BondFutureData : XMLSerializable {
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(XMLDocument& doc) const override;
    };
    BondFutureReferenceDatum();
    BondFutureReferenceDatum(const string& id);
    BondFutureReferenceDatum(const string& id, const BondFutureData& bondFutureData);
    BondFutureReferenceDatum(const string& id, const QuantLib::Date& validFrom);
    BondFutureReferenceDatum(const string& id, const QuantLib::Date& validFrom, const BondFutureData& bondFutureData);
};

%shared_ptr(CreditIndexConstituent)
class CreditIndexConstituent : public XMLSerializable {
public:
    CreditIndexConstituent();
    CreditIndexConstituent(const std::string& name,
                           QuantLib::Real weight,
                           QuantLib::Real priorWeight = QuantLib::Null<QuantLib::Real>(),
                           QuantLib::Real recovery = QuantLib::Null<QuantLib::Real>(),
                           const QuantLib::Date& auctionDate = QuantLib::Date(),
                           const QuantLib::Date& auctionSettlementDate = QuantLib::Date(),
                           const QuantLib::Date& defaultDate = QuantLib::Date(),
                           const QuantLib::Date& eventDeterminationDate = QuantLib::Date());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CreditIndexReferenceDatum)
class CreditIndexReferenceDatum : public ReferenceDatum {
public:
    CreditIndexReferenceDatum();
    CreditIndexReferenceDatum(const std::string& name);
    CreditIndexReferenceDatum(const string& id, const QuantLib::Date& validFrom);
};

%shared_ptr(IndexReferenceDatum)
class IndexReferenceDatum : public ReferenceDatum {
protected:
    IndexReferenceDatum();
};

%shared_ptr(EquityIndexReferenceDatum)
class EquityIndexReferenceDatum : public ReferenceDatum {
public:
    EquityIndexReferenceDatum();
    EquityIndexReferenceDatum(const string& name);
    EquityIndexReferenceDatum(const string& name, const QuantLib::Date& validFrom);
};

%shared_ptr(CommodityIndexReferenceDatum)
class CommodityIndexReferenceDatum : public ReferenceDatum {
public:
    CommodityIndexReferenceDatum();
    CommodityIndexReferenceDatum(const string& name);
    CommodityIndexReferenceDatum(const string& name, const QuantLib::Date& validFrom);
};

%shared_ptr(CurrencyHedgedEquityIndexReferenceDatum)
class CurrencyHedgedEquityIndexReferenceDatum : public ReferenceDatum {
public:
    CurrencyHedgedEquityIndexReferenceDatum();
    CurrencyHedgedEquityIndexReferenceDatum(const string& name);
    CurrencyHedgedEquityIndexReferenceDatum(const string& name, const QuantLib::Date& validFrom);
};

%shared_ptr(PortfolioBasketReferenceDatum)
class PortfolioBasketReferenceDatum : public ReferenceDatum {
public:
    PortfolioBasketReferenceDatum();
    PortfolioBasketReferenceDatum(const string& id);
    PortfolioBasketReferenceDatum(const string& id, const QuantLib::Date& validFrom);
};

%shared_ptr(CreditReferenceDatum)
%shared_ptr(CreditReferenceDatum::CreditData)
class CreditReferenceDatum : public ReferenceDatum {
public:
    struct CreditData {};
    CreditReferenceDatum();
    CreditReferenceDatum(const string& id);
    CreditReferenceDatum(const string& id, const QuantLib::Date& validFrom);
    CreditReferenceDatum(const string& id, const CreditData& creditData);
    CreditReferenceDatum(const string& id, const Date& validFrom, const CreditData& creditData);
};

%shared_ptr(EquityReferenceDatum)
%shared_ptr(EquityReferenceDatum::EquityData)
class EquityReferenceDatum : public ReferenceDatum {
public:
    struct EquityData {};
    EquityReferenceDatum();
    EquityReferenceDatum(const std::string& id);
    EquityReferenceDatum(const std::string& id, const Date& validFrom);
    EquityReferenceDatum(const std::string& id, const EquityData& equityData);
    EquityReferenceDatum(const std::string& id, const Date& validFrom, const EquityData& equityData);
};

%shared_ptr(BondBasketReferenceDatum)
class BondBasketReferenceDatum : public ReferenceDatum {
public:
    BondBasketReferenceDatum();
    BondBasketReferenceDatum(const std::string& id);
    BondBasketReferenceDatum(const std::string& id, const QuantLib::Date& validFrom);
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend BondBasketReferenceDatum {
    BondBasketReferenceDatum(const std::string& id, const std::vector<ext::shared_ptr<BondUnderlying>>& underlyingData) {
        return new BondBasketReferenceDatum(id, VECTOR_SWIG_TO_ORE(underlyingData));
    }
    BondBasketReferenceDatum(const std::string& id,
                             const QuantLib::Date& validFrom, const std::vector<ext::shared_ptr<BondUnderlying>>& underlyingData) {
        return new BondBasketReferenceDatum(id, validFrom, VECTOR_SWIG_TO_ORE(underlyingData));
    }
}

%shared_ptr(BasicReferenceDataManager)
class BasicReferenceDataManager : public ReferenceDataManager, public XMLSerializable {
public:
    BasicReferenceDataManager();
    BasicReferenceDataManager(const string& filename,
                              const QuantLib::ext::shared_ptr<ReferenceDataManager>& rdmOverride = nullptr);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/tlockdata.hpp

%shared_ptr(TreasuryLockData)
class TreasuryLockData : public XMLSerializable {
public:
    TreasuryLockData();
    TreasuryLockData(bool payer, const BondData& bondData, Real referenceRate, string dayCounter,
                     string terminationDate, int paymentGap, string paymentCalendar);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/riskparticipationagreement.hpp

%shared_ptr(RiskParticipationAgreement)
class RiskParticipationAgreement : public ore::data::Trade {
public:
    RiskParticipationAgreement();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend RiskParticipationAgreement {
    RiskParticipationAgreement(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& underlying,
                               const std::vector<ext::shared_ptr<LegData>>& protectionFee, const Real participationRate,
                               const Date& protectionStart, const Date& protectionEnd, const std::string& creditCurveId,
                               const std::string& issuerId = "", const bool settlesAccrual = true,
                               const Real fixedRecoveryRate = Null<Real>(),
                               const ext::optional<OptionData>& optionData = ext::nullopt) {
        return new RiskParticipationAgreement(env, VECTOR_SWIG_TO_ORE(underlying), VECTOR_SWIG_TO_ORE(protectionFee),
            participationRate, protectionStart, protectionEnd, creditCurveId, issuerId, settlesAccrual,
            fixedRecoveryRate, optionData);
    }
    RiskParticipationAgreement(const Envelope& env, const TreasuryLockData& tlockData,
                               const std::vector<ext::shared_ptr<LegData>>& protectionFee, const Real participationRate,
                               const Date& protectionStart, const Date& protectionEnd, const std::string& creditCurveId,
                               const std::string& issuerId = "", const bool settlesAccrual = true,
                               const Real fixedRecoveryRate = Null<Real>()) {
        return new RiskParticipationAgreement(env, tlockData, VECTOR_SWIG_TO_ORE(protectionFee),
            participationRate, protectionStart, protectionEnd, creditCurveId, issuerId, settlesAccrual, fixedRecoveryRate);
    }
}

// ore/OREData/ored/portfolio/schedule.hpp

%shared_ptr(ScheduleDates)
class ScheduleDates : public XMLSerializable {
public:
    ScheduleDates();
    ScheduleDates(const string& calendar, const string& convention, const string& tenor, const vector<string>& dates,
                  const string& endOfMonth = "", const string& endOfMonthConvention = "", bool includeDuplicateDates = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScheduleDerived)
class ScheduleDerived : public XMLSerializable {
public:
    ScheduleDerived();
    ScheduleDerived(const string& baseSchedule, const string& calendar, const string& convention, const string& shift,
                    const bool removeFirstDate = false, const bool removeLastDate = false);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/simmcreditqualifiermapping.hpp

%shared_ptr(SimmCreditQualifierMapping)
struct SimmCreditQualifierMapping {
    SimmCreditQualifierMapping();
    SimmCreditQualifierMapping(const std::string& targetQualifier, const std::string& creditGroup, bool hasCreditRisk);
    // No point wrapping this ctor because SWIG can't differentiate it from the one above
    //SimmCreditQualifierMapping(std::string&& targetQualifier, std::string&& creditGroup, bool hasCreditRisk);
};

// ore/OREData/ored/portfolio/strikeresettableoption.hpp

%shared_ptr(StrikeResettableOption)
class StrikeResettableOption : public ScriptedTrade {
public:
    explicit StrikeResettableOption(const string& tradeType = "StrikeResettableOption");
    StrikeResettableOption(const Envelope& env, const string& longShort, const string& optionType, const string& currency, const string& quantity,
                           const string& strike, const string& resetStrike, const string& triggerType, const string& triggerPrice,
                           const ext::shared_ptr<Underlying> underlying, const ScheduleData& observationDates,
                           const string& expiryDate, const string& settlementDate, const string& premium,
                           const string& premiumDate);
};

%shared_ptr(EquityStrikeResettableOption)
class EquityStrikeResettableOption : public StrikeResettableOption {
public:
    EquityStrikeResettableOption();
};

%shared_ptr(FxStrikeResettableOption)
class FxStrikeResettableOption : public StrikeResettableOption {
public:
    FxStrikeResettableOption();
};

%shared_ptr(CommodityStrikeResettableOption)
class CommodityStrikeResettableOption : public StrikeResettableOption {
public:
    CommodityStrikeResettableOption();
};

// ore/OREData/ored/portfolio/structuredconfigurationerror.hpp

%shared_ptr(StructuredConfigurationErrorMessage)
// FIXME need to export class StructuredMessage in file ore/OREData/ored/utilities/log.hpp
class StructuredConfigurationErrorMessage /*: public StructuredMessage*/ {
public:
    StructuredConfigurationErrorMessage(const std::string& configurationType, const std::string& configurationId,
                                        const std::string& exceptionType, const std::string& exceptionWhat,
                                        const std::map<std::string, std::string>& subFields = {});
};

// ore/OREData/ored/portfolio/structuredconfigurationwarning.hpp

%shared_ptr(StructuredConfigurationWarningMessage)
// FIXME need to export class StructuredMessage in file ore/OREData/ored/utilities/log.hpp
class StructuredConfigurationWarningMessage /*: public StructuredMessage*/ {
public:
    StructuredConfigurationWarningMessage(const std::string& configurationType, const std::string& configurationId,
                                          const std::string& warningType, const std::string& warningWhat,
                                          const std::map<std::string, std::string>& subFields = {});
};

// ore/OREData/ored/portfolio/structuredtradeerror.hpp

%shared_ptr(StructuredTradeErrorMessage)
// FIXME need to export class StructuredMessage in file ore/OREData/ored/utilities/log.hpp
class StructuredTradeErrorMessage /*: public StructuredMessage*/ {
public:
    StructuredTradeErrorMessage(const ext::shared_ptr<Trade>& trade, const std::string& exceptionType,
                                const std::string& exceptionWhat);
    StructuredTradeErrorMessage(const std::string& tradeId, const std::string& tradeType,
                                const std::string& exceptionType, const std::string& exceptionWhat);
};

// ore/OREData/ored/portfolio/structuredtradewarning.hpp

%shared_ptr(StructuredTradeWarningMessage)
// FIXME need to export class StructuredMessage in file ore/OREData/ored/utilities/log.hpp
class StructuredTradeWarningMessage /*: public StructuredMessage*/ {
public:
    StructuredTradeWarningMessage(const ext::shared_ptr<Trade>& trade, const std::string& warningType,
                                  const std::string& warningWhat);
    StructuredTradeWarningMessage(const std::string& tradeId, const std::string& tradeType,
                                  const std::string& warningType, const std::string& warningWhat);
};

// ore/OREData/ored/portfolio/swaptionstraddle.hpp

%shared_ptr(SwaptionStraddle)
class SwaptionStraddle : public Trade {
public:
    SwaptionStraddle();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
%extend SwaptionStraddle {
    SwaptionStraddle(const Envelope& env, const OptionData& optionData, const std::vector<ext::shared_ptr<LegData>>& legData) {
        return new SwaptionStraddle(env, optionData, VECTOR_SWIG_TO_ORE(legData));
    }
}

// ore/OREData/ored/portfolio/tarf.hpp

%shared_ptr(TaRF)
class TaRF : public ScriptedTrade {
public:
    explicit TaRF(const std::string& tradeType = "TaRF");
    // FIXME In order to wrap the ctor below, we need to implement support for converting from:
    //      vector<vector<shared_ptr<RangeBound>>>;
    // to:
    //      vector<vector<RangeBound>>;
    //TaRF(const std::string& currency, const std::string& fixingAmount, const std::string& targetAmount,
    //     const std::string& targetPoints, const std::vector<std::string>& strikes,
    //     const std::vector<std::string>& strikeDates, const ext::shared_ptr<Underlying>& underlying,
    //     const ScheduleData& fixingDates, const std::string& settlementLag, const std::string& settlementCalendar,
    //     const std::string& settlementConvention, OptionData& optionData,
    //     const std::vector<std::vector<RangeBound>>& rangeBoundSet, const std::vector<std::string>& rangeBoundSetDates,
    //     const std::vector<BarrierData>& barriers);
};
//%template(RangeBoundVectorVector) vector<vector<ext::shared_ptr<RangeBound>>>;

%shared_ptr(EquityTaRF)
class EquityTaRF : public TaRF {
public:
    EquityTaRF();
};

%shared_ptr(FxTaRF)
class FxTaRF : public TaRF {
public:
    explicit FxTaRF();
};

%shared_ptr(CommodityTaRF)
class CommodityTaRF : public TaRF {
public:
    explicit CommodityTaRF();
};

// ore/OREData/ored/portfolio/tradeactions.hpp

%shared_ptr(TradeAction)
class TradeAction : public XMLSerializable {
public:
    TradeAction();
    TradeAction(const string& type, const string& owner, const ScheduleData& schedule);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(TradeActions)
class TradeActions : public XMLSerializable {
public:
    TradeActions(const vector<TradeAction>& actions = vector<TradeAction>());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/trademonetary.hpp

%shared_ptr(TradeMonetary)
class TradeMonetary {
public:
    TradeMonetary();
    TradeMonetary(const Real& value, std::string currency = std::string());
    TradeMonetary(const std::string& valueString);
};

// ore/OREData/ored/portfolio/tranche.hpp

%shared_ptr(TrancheData)
class TrancheData : public XMLSerializable {
public:
    TrancheData();
    TrancheData(const std::string& name, double icRatio, double ocRatio, const ext::shared_ptr<LegAdditionalData>& concreteLegData);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/trs.hpp

%shared_ptr(TRS)
%shared_ptr(TRS::ReturnData)
%shared_ptr(TRS::FundingData)
%shared_ptr(TRS::AdditionalCashflowData)
class TRS : public Trade {
public:
    enum class FXConversion { Start, End };
    class ReturnData : public XMLSerializable {
    public:
        ReturnData();
        // FIXME Calling this function from Python triggers a run time error,
        // I think because optional<FXConversion> is not supported correctly
        ReturnData(const bool payer, const std::string& currency, const ScheduleData& scheduleData,
                   const std::string& observationLag, const std::string& observationConvention,
                   const std::string& observationCalendar, const std::string& paymentLag,
                   const std::string& paymentConvention, const std::string& paymentCalendar,
                   const std::vector<std::string>& paymentDates, const Real initialPrice,
                   const std::string& initialPriceCurrency, const std::vector<std::string>& fxTerms,
                   const ext::optional<bool> payUnderlyingCashFlowsImmediately,
                   const ext::optional<FXConversion> fxConversion);
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(XMLDocument& doc) const override;
    };
    class FundingData : public XMLSerializable {
    public:
        enum class NotionalType { PeriodReset, DailyReset, Fixed };
        FundingData();
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(XMLDocument& doc) const override;
    };
    class AdditionalCashflowData : public XMLSerializable {
    public:
        AdditionalCashflowData();
        AdditionalCashflowData(const LegData& legData);
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(XMLDocument& doc) const override;
    };
    TRS();
    TRS(const Envelope& env, const std::vector<ext::shared_ptr<Trade>>& underlying,
        const std::vector<std::string>& underlyingDerivativeId, const ReturnData& returnData,
        const FundingData& fundingData, const AdditionalCashflowData& additionalCashflowData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
// FIXME the code below does not work, I think there is a conflict between
// 1) the declaration "using ore::data::TRS", and
// 2) using %extend for a nested class
// Call VECTOR_SWIG_TO_ORE to convert vectors from SWIG format to ORE format
//%extend TRS::FundingData {
//    explicit TRS::FundingData(
//        const std::vector<ext::shared_ptr<LegData>>& legData,
//        const std::vector<TRS::FundingData::NotionalType>& notionalType = {},
//        const Size fundingResetGracePeriod = 0) {
//        return new TRS::FundingData(VECTOR_SWIG_TO_ORE(legData), notionalType, fundingResetGracePeriod);
//    }
//}

%shared_ptr(CFD)
class CFD : public TRS {
public:
    CFD();
    CFD(const Envelope& env, const std::vector<ext::shared_ptr<Trade>>& underlying,
        const std::vector<std::string>& underlyingDerivativeId, const ReturnData& returnData,
        const FundingData& fundingData, const AdditionalCashflowData& additionalCashflowData);
};

// ore/OREData/ored/portfolio/trswrapper.hpp

// WIP

// ore/OREData/ored/portfolio/varianceswap.hpp

%shared_ptr(VarSwap)
class VarSwap : public Trade {
protected:
    VarSwap(AssetClass assetClassUnderlying);
    VarSwap(Envelope& env, string longShort, const ext::shared_ptr<Underlying>& underlying,
            string currency, double strike, double notional, string startDate, string endDate,
            AssetClass assetClassUnderlying, string momentType, bool addPastDividends);
};

%shared_ptr(EqVarSwap)
class EqVarSwap : public VarSwap {
public:
    EqVarSwap();
    EqVarSwap(Envelope& env, string longShort, const ext::shared_ptr<Underlying>& underlying,
              string currency, double strike, double notional, string startDate, string endDate, string momentType,
              bool addPastDividends);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FxVarSwap)
class FxVarSwap : public VarSwap {
public:
    FxVarSwap();
    FxVarSwap(Envelope& env, string longShort, const ext::shared_ptr<Underlying>& underlying,
              string currency, double strike, double notional, string startDate, string endDate, string momentType,
              bool addPastDividends);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ComVarSwap)
class ComVarSwap : public VarSwap {
public:
    ComVarSwap();
    ComVarSwap(Envelope& env, string longShort, const ext::shared_ptr<Underlying>& underlying,
              string currency, double strike, double notional, string startDate, string endDate, string momentType,
              bool addPastDividends);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/windowbarrieroption.hpp

%shared_ptr(WindowBarrierOption)
class WindowBarrierOption : public ScriptedTrade {
public:
    explicit WindowBarrierOption(const std::string& tradeType = "WindowBarrierOption");
    WindowBarrierOption(const std::string& currency, const std::string& fixingAmount, const TradeStrike& strike,
                        const ext::shared_ptr<Underlying>& underlying, const std::string& startDate,
                        const std::string& endDate, const OptionData& optionData, const BarrierData& barrier);
};

%shared_ptr(EquityWindowBarrierOption)
class EquityWindowBarrierOption : public WindowBarrierOption {
public:
    EquityWindowBarrierOption();
};

%shared_ptr(FxWindowBarrierOption)
class FxWindowBarrierOption : public WindowBarrierOption {
public:
    FxWindowBarrierOption();
};

%shared_ptr(CommodityWindowBarrierOption)
class CommodityWindowBarrierOption : public WindowBarrierOption {
public:
    CommodityWindowBarrierOption();
};

// ore/OREData/ored/portfolio/worstofbasketswap.hpp

%shared_ptr(WorstOfBasketSwap)
class WorstOfBasketSwap : public ScriptedTrade {
public:
    explicit WorstOfBasketSwap(const string& tradeType = "WorstOfBasketSwap");
    WorstOfBasketSwap(const Envelope& env, const string& longShort, const string& quantity, const string& strike,
                      const string& initialFixedRate, const vector<string>& initialPrices, const string& fixedRate,
                      const ScriptedTradeEventData& floatingPeriodSchedule,
                      const ScriptedTradeEventData& floatingFixingSchedule,
                      const ScriptedTradeEventData& fixedDeterminationSchedule,
                      const ScriptedTradeEventData& floatingPayDates,
                      const ScriptedTradeEventData& fixedPayDates,
                      const ScriptedTradeEventData& knockOutDeterminationSchedule,
                      const ScriptedTradeEventData& knockInDeterminationSchedule,
                      const string& knockInPayDate, const string& initialFixedPayDate, const bool bermudanKnockIn,
                      const bool accumulatingFixedCoupons, const bool accruingFixedCoupons, const bool isAveraged,
                      const string& floatingIndex, const string& floatingSpread, const string& floatingRateCutoff,
                      const DayCounter& floatingDayCountFraction, const Period& floatingLookback,
                      const bool includeSpread, const string& currency,
                      const vector<ext::shared_ptr<Underlying>> underlyings, const string& knockInLevel,
                      const vector<string> fixedTriggerLevels, const vector<string> knockOutLevels,
                      const ScriptedTradeEventData& fixedAccrualSchedule);
};

%shared_ptr(EquityWorstOfBasketSwap)
class EquityWorstOfBasketSwap : public WorstOfBasketSwap {
public:
    EquityWorstOfBasketSwap();
};

%shared_ptr(FxWorstOfBasketSwap)
class FxWorstOfBasketSwap : public WorstOfBasketSwap {
public:
    FxWorstOfBasketSwap();
};

%shared_ptr(CommodityWorstOfBasketSwap)
class CommodityWorstOfBasketSwap : public WorstOfBasketSwap {
public:
    CommodityWorstOfBasketSwap();
};

#endif

