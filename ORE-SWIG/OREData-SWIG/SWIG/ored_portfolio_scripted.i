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

#ifndef ored_portfolio_scripted_i
#define ored_portfolio_scripted_i

%{
using ore::data::ScriptedTradeEventData;
using ore::data::ScriptedTradeValueTypeData;
using ore::data::ScriptedTradeScriptData;
using ore::data::ScriptLibraryData;
using ore::data::ScriptedTrade;
using ore::data::Accumulator;
using ore::data::EquityAccumulator;
using ore::data::FxAccumulator;
using ore::data::CommodityAccumulator;
using ore::data::AsianOption;
using ore::data::EquityAsianOption;
using ore::data::FxAsianOption;
using ore::data::CommodityAsianOption;
using ore::data::CommodityBasketVarianceSwap;
using ore::data::CommodityBestEntryOption;
using ore::data::CommodityGenericBarrierOption;
using ore::data::Autocallable_01;
using ore::data::EquityBasketOption;
using ore::data::FxBasketOption;
using ore::data::CommodityBasketOption;
using ore::data::BasketVarianceSwap;
using ore::data::EquityBasketVarianceSwap;
using ore::data::FxBasketVarianceSwap;
using ore::data::BestEntryOption;
using ore::data::EquityBestEntryOption;
using ore::data::FxBestEntryOption;
using ore::data::DoubleDigitalOption;
using ore::data::EuropeanOptionBarrier;
using ore::data::GenericBarrierOption;
using ore::data::EquityGenericBarrierOption;
using ore::data::FxGenericBarrierOption;
using ore::data::KnockOutSwap;
using ore::data::PerformanceOption_01;
using ore::data::RainbowOption;
using ore::data::EquityRainbowOption;
using ore::data::FxRainbowOption;
using ore::data::CommodityRainbowOption;
using ore::data::StrikeResettableOption;
using ore::data::EquityStrikeResettableOption;
using ore::data::FxStrikeResettableOption;
using ore::data::CommodityStrikeResettableOption;
using ore::data::TaRF;
using ore::data::EquityTaRF;
using ore::data::FxTaRF;
using ore::data::CommodityTaRF;
using ore::data::WindowBarrierOption;
using ore::data::EquityWindowBarrierOption;
using ore::data::FxWindowBarrierOption;
using ore::data::CommodityWindowBarrierOption;
using ore::data::WorstOfBasketSwap;
using ore::data::EquityWorstOfBasketSwap;
using ore::data::FxWorstOfBasketSwap;
using ore::data::CommodityWorstOfBasketSwap;
%}

%rename(NewScheduleData) ore::data::ScriptedTradeScriptData::NewScheduleData;
%rename(CalibrationData) ore::data::ScriptedTradeScriptData::CalibrationData;

%template(VectorPairString) std::vector<std::pair<std::string, std::string>>;
%template(ScriptedTradeEventDataVector) std::vector<ext::shared_ptr<ore::data::ScriptedTradeEventData>>;
%template(ScriptedTradeValueTypeDataVector) std::vector<ext::shared_ptr<ore::data::ScriptedTradeValueTypeData>>;
%template(ScriptedTradeNewScheduleDataVector) std::vector<ext::shared_ptr<ore::data::ScriptedTradeScriptData::NewScheduleData>>;
%template(ScriptedTradeCalibrationDataVector) std::vector<ext::shared_ptr<ore::data::ScriptedTradeScriptData::CalibrationData>>;

%shared_ptr(ore::data::ScriptedTradeScriptData::NewScheduleData)
%shared_ptr(ore::data::ScriptedTradeScriptData::CalibrationData)

%shared_ptr(ore::data::ScriptedTradeEventData)
%shared_ptr(ore::data::ScriptedTradeValueTypeData)
%shared_ptr(ore::data::ScriptedTradeScriptData)
%shared_ptr(ore::data::ScriptLibraryData)
%shared_ptr(ore::data::ScriptedTrade)
%shared_ptr(ore::data::Accumulator)
%shared_ptr(ore::data::EquityAccumulator)
%shared_ptr(ore::data::FxAccumulator)
%shared_ptr(ore::data::CommodityAccumulator)
%shared_ptr(ore::data::AsianOption)
%shared_ptr(ore::data::EquityAsianOption)
%shared_ptr(ore::data::FxAsianOption)
%shared_ptr(ore::data::CommodityAsianOption)
%shared_ptr(ore::data::Autocallable_01)
%shared_ptr(ore::data::BasketVarianceSwap)
%shared_ptr(ore::data::EquityBasketVarianceSwap)
%shared_ptr(ore::data::FxBasketVarianceSwap)
%shared_ptr(ore::data::CommodityBasketVarianceSwap)
%shared_ptr(ore::data::BestEntryOption)
%shared_ptr(ore::data::EquityBestEntryOption)
%shared_ptr(ore::data::FxBestEntryOption)
%shared_ptr(ore::data::CommodityBestEntryOption)
%shared_ptr(ore::data::DoubleDigitalOption)
%shared_ptr(ore::data::EuropeanOptionBarrier)
%shared_ptr(ore::data::GenericBarrierOption)
%shared_ptr(ore::data::EquityGenericBarrierOption)
%shared_ptr(ore::data::FxGenericBarrierOption)
%shared_ptr(ore::data::CommodityGenericBarrierOption)
%shared_ptr(ore::data::KnockOutSwap)
%shared_ptr(ore::data::PerformanceOption_01)
%shared_ptr(ore::data::RainbowOption)
%shared_ptr(ore::data::EquityRainbowOption)
%shared_ptr(ore::data::FxRainbowOption)
%shared_ptr(ore::data::CommodityRainbowOption)

SWIG_SHARED_PTR_VECTOR_TYPEMAP(ore::data::ScriptedTradeEventData, ScriptedTradeEventDataVector)
SWIG_SHARED_PTR_VECTOR_TYPEMAP(ore::data::ScriptedTradeValueTypeData, ScriptedTradeValueTypeDataVector)
%shared_ptr(ore::data::StrikeResettableOption)
%shared_ptr(ore::data::EquityStrikeResettableOption)
%shared_ptr(ore::data::FxStrikeResettableOption)
%shared_ptr(ore::data::CommodityStrikeResettableOption)
%shared_ptr(ore::data::TaRF)
%shared_ptr(ore::data::EquityTaRF)
%shared_ptr(ore::data::FxTaRF)
%shared_ptr(ore::data::CommodityTaRF)
%shared_ptr(ore::data::WindowBarrierOption)
%shared_ptr(ore::data::EquityWindowBarrierOption)
%shared_ptr(ore::data::FxWindowBarrierOption)
%shared_ptr(ore::data::CommodityWindowBarrierOption)
%shared_ptr(ore::data::WorstOfBasketSwap)
%shared_ptr(ore::data::EquityWorstOfBasketSwap)
%shared_ptr(ore::data::FxWorstOfBasketSwap)
%shared_ptr(ore::data::CommodityWorstOfBasketSwap)

namespace ore {
namespace data {

class ScriptedTradeEventData : public XMLSerializable {
public:
    ScriptedTradeEventData();
    ScriptedTradeEventData(const std::string& name, const std::string& date);
    ScriptedTradeEventData(const std::string& name, const ScheduleData& schedule);
    ScriptedTradeEventData(const std::string& name, const std::string& baseSchedule, const std::string& shift,
                           const std::string& calendar, const std::string& convention);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class ScriptedTradeValueTypeData : public XMLSerializable {
public:
    explicit ScriptedTradeValueTypeData(const std::string& nodeName);
    ScriptedTradeValueTypeData(const std::string& nodeName, const std::string& name, const std::string& value);
    ScriptedTradeValueTypeData(const std::string& nodeName, const std::string& name,
                               const std::vector<std::string>& values);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class ScriptedTradeScriptData : public XMLSerializable {
public:
    class NewScheduleData : public XMLSerializable {
    public:
        NewScheduleData();
        NewScheduleData(const std::string& name, const std::string& operation,
                        const std::vector<std::string>& sourceSchedules);
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        const std::string& name() const;
        const std::string& operation() const;
        const std::vector<std::string>& sourceSchedules() const;
    };

    class CalibrationData : public XMLSerializable {
    public:
        CalibrationData();
        CalibrationData(const std::string& index, const std::vector<std::string>& strikes);
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        const std::string& index() const;
        const std::vector<std::string>& strikes() const;
    };

    ScriptedTradeScriptData();
    ScriptedTradeScriptData(const std::string& code, const std::string& npv,
                            const std::vector<std::pair<std::string, std::string>>& results,
                            const std::vector<std::string>& stickyCloseOutStates);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class ScriptLibraryData : public XMLSerializable {
public:
    ScriptLibraryData();
    ScriptLibraryData(const ScriptLibraryData& d);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    bool has(const std::string& scriptName, const std::string& purpose,
             const bool fallBackOnEmptyPurpose = true) const;

    %extend {
        std::string productTag(const std::string& scriptName, const std::string& purpose,
                               const bool fallBackOnEmptyPurpose = true) const {
            return self->get(scriptName, purpose, fallBackOnEmptyPurpose).first;
        }

        ScriptedTradeScriptData scriptData(const std::string& scriptName, const std::string& purpose,
                                           const bool fallBackOnEmptyPurpose = true) const {
            return self->get(scriptName, purpose, fallBackOnEmptyPurpose).second;
        }
    }
};

class ScriptedTrade : public Trade {
public:
    ScriptedTrade(const std::string& tradeType = "ScriptedTrade", const Envelope& env = Envelope());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    %extend {
        ScriptedTrade(const Envelope& env,
                      const std::vector<ext::shared_ptr<ore::data::ScriptedTradeEventData>>& events,
                      const std::vector<ext::shared_ptr<ore::data::ScriptedTradeValueTypeData>>& numbers,
                      const std::vector<ext::shared_ptr<ore::data::ScriptedTradeValueTypeData>>& indices,
                      const std::vector<ext::shared_ptr<ore::data::ScriptedTradeValueTypeData>>& currencies,
                      const std::vector<ext::shared_ptr<ore::data::ScriptedTradeValueTypeData>>& daycounters,
                      const std::string& scriptName, const std::string& tradeType = "ScriptedTrade") {
            return new ore::data::ScriptedTrade(env,
                VECTOR_SWIG_TO_ORE(events),
                VECTOR_SWIG_TO_ORE(numbers),
                VECTOR_SWIG_TO_ORE(indices),
                VECTOR_SWIG_TO_ORE(currencies),
                VECTOR_SWIG_TO_ORE(daycounters),
                scriptName, tradeType);
        }
    }
};

class Accumulator : public ScriptedTrade {
public:
    explicit Accumulator(const std::string& tradeType = "Accumulator");
    Accumulator(const std::string& currency, const std::string& fixingAmount, const TradeStrike& strike,
                const ext::shared_ptr<Underlying>& underlying, const OptionData& optionData,
                const std::string& startDate, const ScheduleData& observationDates,
                const ScheduleData& pricingDates, const ScheduleData& settlementDates,
                const std::string& settlementLag, const std::string& settlementCalendar,
                const std::string& settlementConvention, const std::vector<RangeBound>& rangeBounds,
                const std::vector<BarrierData>& barriers, bool knockOutSettlementAtPeriodEnd,
                bool knockOutFixingAtKOSettlement, const ext::shared_ptr<Underlying>& fxUnderlying);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityAccumulator : public Accumulator {
public:
    EquityAccumulator();
};

class FxAccumulator : public Accumulator {
public:
    FxAccumulator();
};

class CommodityAccumulator : public Accumulator {
public:
    CommodityAccumulator();
};

class AsianOption : public Trade {
public:
    explicit AsianOption(const std::string& tradeType);
    AsianOption(const Envelope& env, const std::string& tradeType, double quantity, const TradeStrike& strike,
                const OptionData& option, const ScheduleData& observationDates,
                const ext::shared_ptr<Underlying>& underlying, const QuantLib::Date& settlementDate,
                const std::string& currency);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityAsianOption : public AsianOption {
public:
    EquityAsianOption();
};

class FxAsianOption : public AsianOption {
public:
    FxAsianOption();
};

class CommodityAsianOption : public AsianOption {
public:
    CommodityAsianOption();
};

class Autocallable_01 : public ScriptedTrade {
public:
    Autocallable_01();
    Autocallable_01(const Envelope& env, const std::string& notionalAmount, const std::string& determinationLevel,
                    const std::string& triggerLevel, const ext::shared_ptr<Underlying>& underlying,
                    const std::string& position, const std::string& payCcy,
                    const ScheduleData& fixingDates, const ScheduleData& settlementDates,
                    const std::vector<std::string>& accumulationFactors, const std::string& cap);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore

// ore/OREData/ored/portfolio/basketoption.hpp
// Renamed OREBasketOption to avoid potential clashes

%rename(OREBasketOption) ore::data::BasketOption;
%shared_ptr(ore::data::BasketOption)
%shared_ptr(ore::data::EquityBasketOption)
%shared_ptr(ore::data::FxBasketOption)
%shared_ptr(ore::data::CommodityBasketOption)
%shared_ptr(ore::data::BasketVarianceSwap)
%shared_ptr(ore::data::EquityBasketVarianceSwap)
%shared_ptr(ore::data::FxBasketVarianceSwap)
%shared_ptr(ore::data::CommodityBasketVarianceSwap)
%shared_ptr(ore::data::BestEntryOption)
%shared_ptr(ore::data::EquityBestEntryOption)
%shared_ptr(ore::data::FxBestEntryOption)
%shared_ptr(ore::data::CommodityBestEntryOption)
%shared_ptr(ore::data::DoubleDigitalOption)
%shared_ptr(ore::data::EuropeanOptionBarrier)
%shared_ptr(ore::data::GenericBarrierOption)
%shared_ptr(ore::data::EquityGenericBarrierOption)
%shared_ptr(ore::data::FxGenericBarrierOption)
%shared_ptr(ore::data::CommodityGenericBarrierOption)
%shared_ptr(ore::data::KnockOutSwap)
%shared_ptr(ore::data::PerformanceOption_01)
%shared_ptr(ore::data::RainbowOption)
%shared_ptr(ore::data::EquityRainbowOption)
%shared_ptr(ore::data::FxRainbowOption)
%shared_ptr(ore::data::CommodityRainbowOption)
%shared_ptr(ore::data::StrikeResettableOption)
%shared_ptr(ore::data::EquityStrikeResettableOption)
%shared_ptr(ore::data::FxStrikeResettableOption)
%shared_ptr(ore::data::CommodityStrikeResettableOption)
%shared_ptr(ore::data::TaRF)
%shared_ptr(ore::data::EquityTaRF)
%shared_ptr(ore::data::FxTaRF)
%shared_ptr(ore::data::CommodityTaRF)
%shared_ptr(ore::data::WindowBarrierOption)
%shared_ptr(ore::data::EquityWindowBarrierOption)
%shared_ptr(ore::data::FxWindowBarrierOption)
%shared_ptr(ore::data::CommodityWindowBarrierOption)
%shared_ptr(ore::data::WorstOfBasketSwap)
%shared_ptr(ore::data::EquityWorstOfBasketSwap)
%shared_ptr(ore::data::FxWorstOfBasketSwap)
%shared_ptr(ore::data::CommodityWorstOfBasketSwap)

namespace ore {
namespace data {

class BasketOption : public ScriptedTrade {
public:
    BasketOption(const std::string& tradeType = "BasketOption");
    BasketOption(const std::string& currency, const std::string& notional, const TradeStrike& strike,
                 const std::vector<ext::shared_ptr<Underlying>>& underlyings, const OptionData& optionData,
                 const std::string& settlement, const ScheduleData& observationDates);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityBasketOption : public BasketOption {
public:
    EquityBasketOption();
};

class FxBasketOption : public BasketOption {
public:
    FxBasketOption();
};

class CommodityBasketOption : public BasketOption {
public:
    CommodityBasketOption();
};

class BasketVarianceSwap : public ScriptedTrade {
public:
    BasketVarianceSwap(const std::string& tradeType = "BasketVarianceSwap");
    BasketVarianceSwap(const Envelope& env, const std::string& longShort, const std::string& notional,
                       const std::string& strike, const std::string& currency, const std::string& cap,
                       const std::string& floor, const std::string& settlementDate,
                       const ScheduleData& valuationSchedule, bool squaredPayoff,
                       const std::vector<ext::shared_ptr<Underlying>>& underlyings);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityBasketVarianceSwap : public BasketVarianceSwap {
public:
    EquityBasketVarianceSwap();
};

class FxBasketVarianceSwap : public BasketVarianceSwap {
public:
    FxBasketVarianceSwap();
};

class CommodityBasketVarianceSwap : public BasketVarianceSwap {
public:
    CommodityBasketVarianceSwap();
};

class BestEntryOption : public ScriptedTrade {
public:
    BestEntryOption(const std::string& tradeType = "BestEntryOption");
    BestEntryOption(const Envelope& env, const std::string& longShort, const std::string& notional,
                    const std::string& multiplier, const std::string& strike, const std::string& cap,
                    const std::string& resetMinimum, const std::string& triggerLevel,
                    const ext::shared_ptr<Underlying>& underlying, const std::string& currency,
                    const ScheduleData& observationDates, const std::string& premiumDate);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityBestEntryOption : public BestEntryOption {
public:
    EquityBestEntryOption();
};

class FxBestEntryOption : public BestEntryOption {
public:
    FxBestEntryOption();
};

class CommodityBestEntryOption : public BestEntryOption {
public:
    CommodityBestEntryOption();
};

class DoubleDigitalOption : public ScriptedTrade {
public:
    DoubleDigitalOption();
    DoubleDigitalOption(const Envelope& env, const std::string& expiry, const std::string& settlement,
                        const std::string& binaryPayout, const std::string& binaryLevel1,
                        const std::string& binaryLevel2, const std::string& type1, const std::string& type2,
                        const std::string& position, const ext::shared_ptr<Underlying>& underlying1,
                        const ext::shared_ptr<Underlying>& underlying2,
                        const ext::shared_ptr<Underlying>& underlying3,
                        const ext::shared_ptr<Underlying>& underlying4, const std::string& payCcy);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EuropeanOptionBarrier : public ScriptedTrade {
public:
    explicit EuropeanOptionBarrier(const ext::shared_ptr<Conventions>& conventions = nullptr);
    EuropeanOptionBarrier(const Envelope& env, const std::string& quantity, const std::string& putCall,
                          const std::string& longShort, const std::string& strike, const std::string& premiumAmount,
                          const std::string& premiumCurrency, const std::string& premiumDate,
                          const std::string& optionExpiry,
                          const ext::shared_ptr<Underlying>& optionUnderlying,
                          const ext::shared_ptr<Underlying>& barrierUnderlying,
                          const std::string& barrierLevel, const std::string& barrierType,
                          const std::string& barrierStyle, const std::string& settlementDate,
                          const std::string& payCcy, const ScheduleData& barrierSchedule,
                          const ext::shared_ptr<Conventions>& conventions = nullptr);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class GenericBarrierOption : public ScriptedTrade {
public:
    GenericBarrierOption(const std::string& tradeType = "GenericBarrierOption");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    %extend {
        GenericBarrierOption(ext::shared_ptr<Underlying>& underlying, const OptionData& optionData,
                             const std::vector<ext::shared_ptr<BarrierData>>& barriers,
                             const ScheduleData& barrierMonitoringDates,
                             const BarrierData& transatlanticBarrier, const std::string& payCurrency,
                             const std::string& settlementDate, const std::string& quantity,
                             const std::string& strike, const std::string& amount, const std::string& kikoType) {
            return new ore::data::GenericBarrierOption(underlying, optionData, VECTOR_SWIG_TO_ORE(barriers),
                barrierMonitoringDates, transatlanticBarrier, payCurrency, settlementDate, quantity,
                strike, amount, kikoType);
        }
        GenericBarrierOption(const std::vector<ext::shared_ptr<Underlying>>& underlyings, const OptionData& optionData,
                             const std::vector<ext::shared_ptr<BarrierData>>& barriers,
                             const ScheduleData& barrierMonitoringDates,
                             const std::vector<ext::shared_ptr<BarrierData>>& transatlanticBarrier,
                             const std::string& payCurrency, const std::string& settlementDate,
                             const std::string& quantity, const std::string& strike,
                             const std::string& amount, const std::string& kikoType) {
            return new ore::data::GenericBarrierOption(underlyings, optionData, VECTOR_SWIG_TO_ORE(barriers),
                barrierMonitoringDates, VECTOR_SWIG_TO_ORE(transatlanticBarrier), payCurrency,
                settlementDate, quantity, strike, amount, kikoType);
        }
    }
};

class EquityGenericBarrierOption : public GenericBarrierOption {
public:
    EquityGenericBarrierOption();
};

class FxGenericBarrierOption : public GenericBarrierOption {
public:
    FxGenericBarrierOption();
};

class CommodityGenericBarrierOption : public GenericBarrierOption {
public:
    CommodityGenericBarrierOption();
};

class KnockOutSwap : public ScriptedTrade {
public:
    KnockOutSwap();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    %extend {
        KnockOutSwap(const std::vector<ext::shared_ptr<LegData>>& legData, const BarrierData& barrierData,
                     const std::string& barrierStartDate) {
            return new ore::data::KnockOutSwap(VECTOR_SWIG_TO_ORE(legData), barrierData, barrierStartDate);
        }
    }
};

class PerformanceOption_01 : public ScriptedTrade {
public:
    explicit PerformanceOption_01(const ext::shared_ptr<Conventions>& conventions = nullptr);
    PerformanceOption_01(const Envelope& env, const std::string& notionalAmount,
                         const std::string& participationRate, const std::string& valuationDate,
                         const std::string& settlementDate,
                         const std::vector<ext::shared_ptr<Underlying>>& underlyings,
                         const std::vector<std::string>& strikePrices,
                         const std::string& strike, const bool strikeIncluded, const std::string& position,
                         const std::string& payCcy,
                         const ext::shared_ptr<Conventions>& conventions = nullptr);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class RainbowOption : public ScriptedTrade {
public:
    RainbowOption();
    RainbowOption(const std::string& currency, const std::string& notional, const std::string& strike,
                  const std::vector<ext::shared_ptr<Underlying>>& underlyings, const OptionData& optionData,
                  const std::string& settlement);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void setIsdaTaxonomyFields();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityRainbowOption : public RainbowOption {
public:
    EquityRainbowOption();
    EquityRainbowOption(const ext::shared_ptr<Conventions>& conventions);
};

class FxRainbowOption : public RainbowOption {
public:
    FxRainbowOption();
    FxRainbowOption(const ext::shared_ptr<Conventions>& conventions);
};

class CommodityRainbowOption : public RainbowOption {
public:
    CommodityRainbowOption();
    CommodityRainbowOption(const ext::shared_ptr<Conventions>& conventions);
};

class StrikeResettableOption : public ScriptedTrade {
public:
    StrikeResettableOption(const std::string& tradeType = "StrikeResettableOption");
    StrikeResettableOption(const Envelope& env, const std::string& longShort, const std::string& optionType,
                           const std::string& currency, const std::string& quantity,
                           const std::string& strike, const std::string& resetStrike,
                           const std::string& triggerType, const std::string& triggerPrice,
                           const ext::shared_ptr<Underlying>& underlying,
                           const ScheduleData& observationDates,
                           const std::string& expiryDate, const std::string& settlementDate,
                           const std::string& premium, const std::string& premiumDate);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityStrikeResettableOption : public StrikeResettableOption {
public:
    EquityStrikeResettableOption();
};

class FxStrikeResettableOption : public StrikeResettableOption {
public:
    FxStrikeResettableOption();
};

class CommodityStrikeResettableOption : public StrikeResettableOption {
public:
    CommodityStrikeResettableOption();
};

class TaRF : public ScriptedTrade {
public:
    TaRF(const std::string& tradeType = "TaRF");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityTaRF : public TaRF {
public:
    EquityTaRF();
};

class FxTaRF : public TaRF {
public:
    FxTaRF();
};

class CommodityTaRF : public TaRF {
public:
    CommodityTaRF();
};

class WindowBarrierOption : public ScriptedTrade {
public:
    WindowBarrierOption(const std::string& tradeType = "WindowBarrierOption");
    WindowBarrierOption(const std::string& currency, const std::string& fixingAmount,
                        const TradeStrike& strike, const ext::shared_ptr<Underlying>& underlying,
                        const std::string& startDate, const std::string& endDate,
                        const OptionData& optionData, const BarrierData& barrier);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityWindowBarrierOption : public WindowBarrierOption {
public:
    EquityWindowBarrierOption();
};

class FxWindowBarrierOption : public WindowBarrierOption {
public:
    FxWindowBarrierOption();
};

class CommodityWindowBarrierOption : public WindowBarrierOption {
public:
    CommodityWindowBarrierOption();
};

class WorstOfBasketSwap : public ScriptedTrade {
public:
    WorstOfBasketSwap(const std::string& tradeType = "WorstOfBasketSwap");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    %extend {
        WorstOfBasketSwap(const Envelope& env, const std::string& longShort,
                          const std::string& quantity, const std::string& strike,
                          const std::string& initialFixedRate, const std::vector<std::string>& initialPrices,
                          const std::string& fixedRate,
                          const ScriptedTradeEventData& floatingPeriodSchedule,
                          const ScriptedTradeEventData& floatingFixingSchedule,
                          const ScriptedTradeEventData& fixedDeterminationSchedule,
                          const ScriptedTradeEventData& floatingPayDates,
                          const ScriptedTradeEventData& fixedPayDates,
                          const ScriptedTradeEventData& knockOutDeterminationSchedule,
                          const ScriptedTradeEventData& knockInDeterminationSchedule,
                          const std::string& knockInPayDate, const std::string& initialFixedPayDate,
                          const bool bermudanKnockIn, const bool accumulatingFixedCoupons,
                          const bool accruingFixedCoupons, const bool isAveraged,
                          const std::string& floatingIndex, const std::string& floatingSpread,
                          const std::string& floatingRateCutoff,
                          const QuantLib::DayCounter& floatingDayCountFraction,
                          const QuantLib::Period& floatingLookback, const bool includeSpread,
                          const std::string& currency,
                          const std::vector<ext::shared_ptr<Underlying>>& underlyings,
                          const std::string& knockInLevel,
                          const std::vector<std::string>& fixedTriggerLevels,
                          const std::vector<std::string>& knockOutLevels,
                          const ScriptedTradeEventData& fixedAccrualSchedule) {
            return new ore::data::WorstOfBasketSwap(env, longShort, quantity, strike, initialFixedRate, initialPrices,
                fixedRate, floatingPeriodSchedule, floatingFixingSchedule, fixedDeterminationSchedule,
                floatingPayDates, fixedPayDates, knockOutDeterminationSchedule, knockInDeterminationSchedule,
                knockInPayDate, initialFixedPayDate, bermudanKnockIn, accumulatingFixedCoupons,
                accruingFixedCoupons, isAveraged, floatingIndex, floatingSpread, floatingRateCutoff,
                floatingDayCountFraction, floatingLookback, includeSpread, currency,
                underlyings, knockInLevel, fixedTriggerLevels, knockOutLevels,
                fixedAccrualSchedule);
        }
    }
};

class EquityWorstOfBasketSwap : public WorstOfBasketSwap {
public:
    EquityWorstOfBasketSwap();
};

class FxWorstOfBasketSwap : public WorstOfBasketSwap {
public:
    FxWorstOfBasketSwap();
};

class CommodityWorstOfBasketSwap : public WorstOfBasketSwap {
public:
    CommodityWorstOfBasketSwap();
};

} // namespace data
} // namespace ore

#endif
