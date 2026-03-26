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

#ifndef ored_portfolio_trades_i
#define ored_portfolio_trades_i

%{
using ore::data::VanillaOptionTrade;
using ore::data::FxOption;
using ore::data::EquityOption;
using ore::data::BondData;
using ore::data::ForwardBond;
using ore::data::BondOption;
using ore::data::TRS;
using ore::data::CallableBondData;
using ore::data::ConvertibleBondData;
using ore::data::parseTrsFundingNotionalType;
using ore::data::FxDoubleBarrierOption;
using ore::data::FxEuropeanBarrierOption;
using ore::data::FxBarrierOption;
using ore::data::FxTouchOption;
%}

%rename(ORESwap) ore::data::Swap;
%rename(OREForwardRateAgreement) ore::data::ForwardRateAgreement;
%rename(ORESwaption) ore::data::Swaption;
%rename(OREFxForward) ore::data::FxForward;
%rename(OREEquityForward) ore::data::EquityForward;
%rename(ORECapFloor) ore::data::CapFloor;
%rename(OREBond) ore::data::Bond;
%rename(ORECommodityForward) ore::data::CommodityForward;
%rename(ORECommoditySwap) ore::data::CommoditySwap;
%shared_ptr(ore::data::Swap)
%shared_ptr(ore::data::ForwardRateAgreement)
%shared_ptr(ore::data::Swaption)
%shared_ptr(ore::data::VanillaOptionTrade)
%nodefaultctor ore::data::VanillaOptionTrade;
%shared_ptr(ore::data::FxOption)
%shared_ptr(ore::data::FxForward)
%shared_ptr(ore::data::EquityOption)
%shared_ptr(ore::data::EquityForward)
%shared_ptr(ore::data::CapFloor)
%shared_ptr(ore::data::BondData)
%shared_ptr(ore::data::Bond)
%shared_ptr(ore::data::CommodityForward)
%shared_ptr(ore::data::CommoditySwap)

namespace ore {
namespace data {

class Swap : public Trade {
public:
    Swap(const std::string swapType = "Swap");
    Swap(const Envelope& env, const std::string swapType = "Swap");
    Swap(const Envelope& env, const std::vector<LegData>& legData, const std::string swapType = "Swap",
         const std::string settlement = "Physical");
    Swap(const Envelope& env, const LegData& leg0, const LegData& leg1, const std::string swapType = "Swap",
         const std::string settlement = "Physical");

    void build(const ext::shared_ptr<EngineFactory>&) override;
    virtual void setIsdaTaxonomyFields();
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;

    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    const std::string& settlement() const { return settlement_; }
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void fromXMLString(const std::string& xmlString);
    std::string toXMLString();
    const std::vector<LegData>& legData() const { return legData_; }
    const std::map<std::string,QuantLib::ext::any>& additionalData() const override;
};

class ForwardRateAgreement : public Swap {
public:
    ForwardRateAgreement();
    ForwardRateAgreement(Envelope& env, std::string longShort, std::string currency, std::string startDate, std::string endDate,
                         std::string index, double strike, double amount);

    void build(const ext::shared_ptr<EngineFactory>& engineFactory) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void fromXMLString(const std::string& xmlString);
    std::string toXMLString();
    const std::string& index() const { return index_; }

    const std::map<std::string,QuantLib::ext::any>& additionalData() const override;
};

class Swaption : public Trade {
public:
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend Swaption {
    Swaption(const Envelope& env, const OptionData& optionData, const std::vector<ext::shared_ptr<LegData>>& legData) {
        return new ore::data::Swaption(env, optionData, VECTOR_SWIG_TO_ORE(legData));
    }
}

class VanillaOptionTrade : public Trade { };

class FxOption : public VanillaOptionTrade {
public:
    FxOption(const Envelope& env, const OptionData& option, const std::string& boughtCurrency, double boughtAmount,
             const std::string& soldCurrency, double soldAmount, const std::string& fxIndex = "", double delta = 0.0);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class FxForward : public Trade {
public:
    FxForward();
    FxForward(const Envelope& env, const std::string& maturityDate, const std::string& boughtCurrency, double boughtAmount,
              const std::string& soldCurrency, double soldAmount, const std::string& settlement = "Physical",
              const std::string& fxIndex = "", const std::string& payDate = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityOption : public VanillaOptionTrade {
public:
    EquityOption(Envelope& env, OptionData option, EquityUnderlying equityUnderlying, std::string currency,
        QuantLib::Real quantity, TradeStrike tradeStrike);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class EquityForward : public Trade {
public:
    EquityForward();
    EquityForward(Envelope& env, const std::string& longShort, EquityUnderlying equityUnderlying, const std::string& currency,
                  QuantLib::Real quantity, const std::string& maturityDate, QuantLib::Real strike,
                  const string& strikeCurrency = "", const std::string& fxIndex = "", const std::string& payDate = "",
                  std::string payLag = "", const std::string& payCalendar = "" , const std::string& payConvention = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class CapFloor : public Trade {
public:
    CapFloor();
    CapFloor(const Envelope& env, const std::string& longShort, const LegData& leg, const std::vector<double>& caps,
             const std::vector<double>& floors, const PremiumData& premiumData = {});
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class BondData : public XMLSerializable {
public:
    BondData(std::string issuerId, std::string creditCurveId, std::string securityId, std::string referenceCurveId, std::string settlementDays,
             std::string calendar, Real faceAmount, std::string maturityDate, std::string currency, std::string issueDate,
             bool hasCreditRisk = true);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

class Bond : public Trade {
public:
    Bond();
    Bond(Envelope env, const ore::data::BondData& bondData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class CommodityForward : public Trade {
public:
    CommodityForward();
    CommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike);
    CommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike, const QuantLib::Date& futureExpiryDate,
                     const QuantLib::ext::optional<bool>& physicallySettled = true,
                     const QuantLib::Date& paymentDate = QuantLib::Date());
    CommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike, const QuantLib::Period& futureExpiryOffset,
                     const QuantLib::Calendar& offsetCalendar,
                     const QuantLib::ext::optional<bool>& physicallySettled = true,
                     const QuantLib::Date& paymentDate = QuantLib::Date());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class CommoditySwap : public Trade {
public:
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CommoditySwap {
    CommoditySwap(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& legs) {
        return new ore::data::CommoditySwap(env, VECTOR_SWIG_TO_ORE(legs));
    }
}

} // namespace data
} // namespace ore

%shared_ptr(FxBarrierOption)
class FxBarrierOption : public Trade {
public:
    FxBarrierOption(Envelope& env, OptionData option, BarrierData barrier, QuantLib::Date startDate,
                    std::string calendar, std::string boughtCurrency, double boughtAmount,
                    std::string soldCurrency, double soldAmount, std::string fxIndex = "");

    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FxTouchOption)
class FxTouchOption : public Trade {
public:
    FxTouchOption(Envelope& env, OptionData option, BarrierData barrier, const std::string& foreignCurrency,
                  const std::string& domesticCurrency, const std::string& payoffCurrency, double payoffAmount,
                  const std::string& startDate = "", const std::string& calendar = "", const std::string& fxIndex = "",
                  const std::string& fxIndexDailyLows = "", const std::string& fxIndexDailyHighs = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ForwardBond)
class ForwardBond : public Trade {
public:
    ForwardBond();
    ForwardBond(Envelope env, const ore::data::BondData& bondData, std::string fwdMaturityDate, std::string fwdSettlementDate,
                std::string settlement, std::string amount, std::string lockRate, std::string lockRateDayCounter,
                std::string settlementDirty, std::string compensationPayment, std::string compensationPaymentDate,
                std::string longInForward, std::string dv01 = std::string(), std::string knockOut = std::string());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(BondOption)
class BondOption : public Trade {
public:
    BondOption();
    BondOption(Envelope env, const ore::data::BondData& bondData, const OptionData& optionData, TradeStrike strike, bool knocksOut);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// TRS inner classes extracted as top-level to work around SWIG flatnested limitations
%{
using ReturnData = ore::data::TRS::ReturnData;
using FundingData = ore::data::TRS::FundingData;
using AdditionalCashflowData = ore::data::TRS::AdditionalCashflowData;
%}

%shared_ptr(ReturnData)
class ReturnData : public XMLSerializable {
public:
    ReturnData();
    ReturnData(const bool payer, const std::string& currency, const ScheduleData& scheduleData,
               const std::string& observationLag, const std::string& observationConvention,
               const std::string& observationCalendar, const std::string& paymentLag,
               const std::string& paymentConvention, const std::string& paymentCalendar,
               const std::vector<std::string>& paymentDates, const QuantLib::Real initialPrice,
               const std::string& initialPriceCurrency, const std::vector<std::string>& fxTerms,
               const QuantLib::ext::optional<bool> payUnderlyingCashFlowsImmediately,
               const QuantLib::ext::optional<TRS::FXConversion> fxConversion);
    bool payer() const;
    const std::string& currency() const;
    const ScheduleData& scheduleData() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FundingData)
class FundingData : public XMLSerializable {
public:
    enum class NotionalType { PeriodReset, DailyReset, Fixed };
    FundingData();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend FundingData {
    FundingData(const std::vector<ext::shared_ptr<LegData>>& legData) {
        return new FundingData(VECTOR_SWIG_TO_ORE(legData));
    }
    FundingData(const std::vector<ext::shared_ptr<LegData>>& legData,
                const std::vector<FundingData::NotionalType>& notionalType,
                const QuantLib::Size fundingResetGracePeriod = 0) {
        return new FundingData(VECTOR_SWIG_TO_ORE(legData), notionalType, fundingResetGracePeriod);
    }
}

%shared_ptr(AdditionalCashflowData)
class AdditionalCashflowData : public XMLSerializable {
public:
    AdditionalCashflowData();
    AdditionalCashflowData(const LegData& legData);
    const LegData& legData() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(TRS)
class TRS : public Trade {
public:
    enum class FXConversion { Start, End };
    TRS();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend TRS {
    TRS(const Envelope& env, const std::vector<ext::shared_ptr<Trade>>& underlying,
        const std::vector<std::string>& underlyingDerivativeId, const ReturnData& returnData,
        const FundingData& fundingData,
        const AdditionalCashflowData& additionalCashflowData) {
        return new TRS(env, underlying, underlyingDerivativeId, returnData, fundingData,
                       additionalCashflowData);
    }
}

FundingData::NotionalType parseTrsFundingNotionalType(const std::string& s);

%feature("flatnested") CallableBondData;
%rename(CallableBondCallabilityData) CallableBondData::CallabilityData;
%shared_ptr(CallableBondData)
%shared_ptr(CallableBondData::CallabilityData)
class CallableBondData : public XMLSerializable {
public:
    class CallabilityData : public XMLSerializable {
    public:
        explicit CallabilityData(const std::string& nodeName);
        bool initialised() const;
        const ScheduleData& dates() const;
        const std::vector<std::string>& styles() const;
        const std::vector<std::string>& styleDates() const;
        const std::vector<double>& prices() const;
        const std::vector<std::string>& priceDates() const;
        const std::vector<std::string>& priceTypes() const;
        const std::vector<std::string>& priceTypeDates() const;
        const std::vector<bool>& includeAccrual() const;
        const std::vector<std::string>& includeAccrualDates() const;
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
    };

    CallableBondData();
    explicit CallableBondData(const ore::data::BondData& bondData);
    const ore::data::BondData& bondData() const;
    const CallabilityData& callData() const;
    const CallabilityData& putData() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void populateFromBondReferenceData(const ext::shared_ptr<ReferenceDataManager>& referenceData);
};

%rename(ORECallableBond) ore::data::CallableBond;
%shared_ptr(ore::data::CallableBond)

namespace ore {
namespace data {

class CallableBond : public Trade {
public:
    CallableBond();
    CallableBond(const Envelope& env, const CallableBondData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
    const CallableBondData& data() const;
    const ore::data::BondData& bondData() const;
};

} // namespace data
} // namespace ore

%feature("flatnested") ConvertibleBondData;
%rename(ConvertibleBondCallabilityData) ConvertibleBondData::CallabilityData;
%rename(ConvertibleBondConversionData) ConvertibleBondData::ConversionData;
%rename(ConvertibleBondDividendProtectionData) ConvertibleBondData::DividendProtectionData;
%feature("flatnested") ConvertibleBondData::CallabilityData;
%rename(ConvertibleBondMakeWholeData) ConvertibleBondData::CallabilityData::MakeWholeData;
%feature("flatnested") ConvertibleBondData::CallabilityData::MakeWholeData;
%rename(ConvertibleBondConversionRatioIncreaseData)
    ConvertibleBondData::CallabilityData::MakeWholeData::ConversionRatioIncreaseData;
%feature("flatnested") ConvertibleBondData::ConversionData;
%rename(ConvertibleBondContingentConversionData)
    ConvertibleBondData::ConversionData::ContingentConversionData;
%rename(ConvertibleBondMandatoryConversionData)
    ConvertibleBondData::ConversionData::MandatoryConversionData;
%rename(ConvertibleBondConversionResetData)
    ConvertibleBondData::ConversionData::ConversionResetData;
%rename(ConvertibleBondExchangeableData)
    ConvertibleBondData::ConversionData::ExchangeableData;
%rename(ConvertibleBondFixedAmountConversionData)
    ConvertibleBondData::ConversionData::FixedAmountConversionData;
%feature("flatnested") ConvertibleBondData::ConversionData::MandatoryConversionData;
%rename(ConvertibleBondPepsData)
    ConvertibleBondData::ConversionData::MandatoryConversionData::PepsData;
%shared_ptr(ConvertibleBondData)
%shared_ptr(ConvertibleBondData::CallabilityData)
%shared_ptr(ConvertibleBondData::CallabilityData::MakeWholeData)
%shared_ptr(ConvertibleBondData::CallabilityData::MakeWholeData::ConversionRatioIncreaseData)
%shared_ptr(ConvertibleBondData::ConversionData)
%shared_ptr(ConvertibleBondData::ConversionData::ContingentConversionData)
%shared_ptr(ConvertibleBondData::ConversionData::MandatoryConversionData)
%shared_ptr(ConvertibleBondData::ConversionData::MandatoryConversionData::PepsData)
%shared_ptr(ConvertibleBondData::ConversionData::ConversionResetData)
%shared_ptr(ConvertibleBondData::ConversionData::ExchangeableData)
%shared_ptr(ConvertibleBondData::ConversionData::FixedAmountConversionData)
%shared_ptr(ConvertibleBondData::DividendProtectionData)
class ConvertibleBondData : public XMLSerializable {
public:
    class CallabilityData : public XMLSerializable {
    public:
        class MakeWholeData : public XMLSerializable {
        public:
            class ConversionRatioIncreaseData : public XMLSerializable {
            public:
                ConversionRatioIncreaseData();
                bool initialised() const;
                const std::string& cap() const;
                const std::vector<double>& stockPrices() const;
                const std::vector<std::vector<double>>& crIncrease() const;
                const std::vector<std::string>& crIncreaseDates() const;
                void fromXML(XMLNode* node) override;
                XMLNode* toXML(XMLDocument& doc) const override;
            };

            MakeWholeData();
            bool initialised() const;
            const ConversionRatioIncreaseData& conversionRatioIncreaseData() const;
            void fromXML(XMLNode* node) override;
            XMLNode* toXML(XMLDocument& doc) const override;
        };

        explicit CallabilityData(const std::string& nodeName);
        bool initialised() const;
        const ScheduleData& dates() const;
        const std::vector<std::string>& styles() const;
        const std::vector<std::string>& styleDates() const;
        const std::vector<double>& prices() const;
        const std::vector<std::string>& priceDates() const;
        const std::vector<std::string>& priceTypes() const;
        const std::vector<std::string>& priceTypeDates() const;
        const std::vector<bool>& includeAccrual() const;
        const std::vector<std::string>& includeAccrualDates() const;
        const std::vector<bool>& isSoft() const;
        const std::vector<std::string>& isSoftDates() const;
        const std::vector<double>& triggerRatios() const;
        const std::vector<std::string>& triggerRatioDates() const;
        const std::vector<std::string>& nOfMTriggers() const;
        const std::vector<std::string>& nOfMTriggerDates() const;
        const MakeWholeData& makeWholeData() const;
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
    };

    class ConversionData : public XMLSerializable {
    public:
        class ContingentConversionData : public XMLSerializable {
        public:
            ContingentConversionData();
            bool initialised() const;
            const std::vector<std::string>& observations() const;
            const std::vector<std::string>& observationDates() const;
            const std::vector<double>& barriers() const;
            const std::vector<std::string>& barrierDates() const;
            void fromXML(XMLNode* node) override;
            XMLNode* toXML(XMLDocument& doc) const override;
        };

        class MandatoryConversionData : public XMLSerializable {
        public:
            class PepsData : public XMLSerializable {
            public:
                PepsData();
                bool initialised() const;
                double upperBarrier() const;
                double lowerBarrier() const;
                double upperConversionRatio() const;
                double lowerConversionRatio() const;
                void fromXML(XMLNode* node) override;
                XMLNode* toXML(XMLDocument& doc) const override;
            };

            MandatoryConversionData();
            bool initialised() const;
            const std::string& date() const;
            const std::string& type() const;
            const PepsData& pepsData() const;
            void fromXML(XMLNode* node) override;
            XMLNode* toXML(XMLDocument& doc) const override;
        };

        class ConversionResetData : public XMLSerializable {
        public:
            ConversionResetData();
            bool initialised() const;
            const ScheduleData& dates() const;
            const std::vector<std::string>& references() const;
            const std::vector<std::string>& referenceDates() const;
            const std::vector<double>& thresholds() const;
            const std::vector<std::string>& thresholdDates() const;
            const std::vector<double>& gearings() const;
            const std::vector<std::string>& gearingDates() const;
            const std::vector<double>& floors() const;
            const std::vector<std::string>& floorDates() const;
            const std::vector<double>& globalFloors() const;
            const std::vector<std::string>& globalFloorDates() const;
            void fromXML(XMLNode* node) override;
            XMLNode* toXML(XMLDocument& doc) const override;
        };

        class ExchangeableData : public XMLSerializable {
        public:
            ExchangeableData();
            bool initialised() const;
            bool isExchangeable() const;
            const std::string& equityCreditCurve() const;
            bool secured() const;
            void fromXML(XMLNode* node) override;
            XMLNode* toXML(XMLDocument& doc) const override;
        };

        class FixedAmountConversionData : public XMLSerializable {
        public:
            FixedAmountConversionData();
            bool initialised() const;
            const std::string& currency() const;
            const std::vector<double>& amounts() const;
            const std::vector<std::string>& amountDates() const;
            void fromXML(XMLNode* node) override;
            XMLNode* toXML(XMLDocument& doc) const override;
        };

        ConversionData();
        bool initialised() const;
        const ScheduleData& dates() const;
        const std::vector<std::string>& styles() const;
        const std::vector<std::string>& styleDates() const;
        const std::vector<double>& conversionRatios() const;
        const std::vector<std::string>& conversionRatioDates() const;
        const ContingentConversionData& contingentConversionData() const;
        const MandatoryConversionData& mandatoryConversionData() const;
        const ConversionResetData& conversionResetData() const;
        const EquityUnderlying equityUnderlying() const;
        const std::string fxIndex() const;
        const ExchangeableData& exchangeableData() const;
        const FixedAmountConversionData& fixedAmountConversionData() const;
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
    };

    class DividendProtectionData : public XMLSerializable {
    public:
        DividendProtectionData();
        bool initialised() const;
        const ScheduleData& dates() const;
        const std::vector<std::string>& adjustmentStyles() const;
        const std::vector<std::string>& adjustmentStyleDates() const;
        const std::vector<std::string>& dividendTypes() const;
        const std::vector<std::string>& dividendTypeDates() const;
        const std::vector<double>& thresholds() const;
        const std::vector<std::string>& thresholdDates() const;
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
    };

    ConvertibleBondData();
    explicit ConvertibleBondData(const ore::data::BondData& bondData);
    const ore::data::BondData& bondData() const;
    const CallabilityData& callData() const;
    const CallabilityData& putData() const;
    const ConversionData& conversionData() const;
    const DividendProtectionData& dividendProtectionData() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void populateFromBondReferenceData(const ext::shared_ptr<ReferenceDataManager>& referenceData);
};

%rename(OREConvertibleBond) ore::data::ConvertibleBond;
%shared_ptr(ore::data::ConvertibleBond)

namespace ore {
namespace data {

class ConvertibleBond : public Trade {
public:
    ConvertibleBond();
    ConvertibleBond(const Envelope& env, const ConvertibleBondData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
    const ConvertibleBondData& data() const;
    const ore::data::BondData& bondData() const;
};

} // namespace data
} // namespace ore
%pythoncode %{ ConvertibleBond = OREConvertibleBond %}

%shared_ptr(FxDoubleBarrierOption)
class FxDoubleBarrierOption : public FxOptionWithBarrier {
public:
    FxDoubleBarrierOption();
    FxDoubleBarrierOption(Envelope& env, OptionData option, BarrierData barrier, QuantLib::Date startDate,
    std::string calendar, std::string boughtCurrency, QuantLib::Real boughtAmount, std::string soldCurrency,
    QuantLib::Real soldAmount, std::string fxIndex = "");
    void checkBarriers() override;
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    vanillaPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    barrierPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(FxEuropeanBarrierOption)
class FxEuropeanBarrierOption : public Trade {
public:
    FxEuropeanBarrierOption();
    FxEuropeanBarrierOption(Envelope& env, OptionData option, BarrierData barrier, std::string boughtCurrency,
                            double boughtAmount, std::string soldCurrency, double soldAmount,
                            std::string startDate = "", std::string calendar = "", std::string fxIndex = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/balanceguaranteedswap.hpp

%{
using ore::data::FxOptionWithBarrier;
using ore::data::EquityOptionWithBarrier;
using ore::data::BGSTrancheData;
using OREBalanceGuaranteedSwap = ore::data::BalanceGuaranteedSwap;
using ore::data::BondFuture;
using ore::data::BondPosition;
using ore::data::BondRepo;
using ore::data::Ascot;
using ore::data::CashPosition;
using ore::data::CBO;
using ore::data::EquityCliquetOption;
using ore::data::CompositeTrade;
using ore::data::CrossCurrencySwap;
using ore::data::EquityBarrierOption;
using ore::data::EquityDigitalOption;
using ore::data::EquityDoubleBarrierOption;
using ore::data::EquityDoubleTouchOption;
using ore::data::EquityEuropeanBarrierOption;
using ore::data::EquityFutureOption;
using ore::data::EquityOptionPosition;
using ore::data::EquityOutperformanceOption;
using ore::data::EquityPosition;
using ore::data::EquityTouchOption;
using ore::data::FailedTrade;
using ore::data::FlexiSwap;
using ore::data::FxAverageForward;
using ore::data::FxDigitalBarrierOption;
using ore::data::FxDigitalOption;
using ore::data::FxDoubleTouchOption;
using ore::data::FxKIKOBarrierOption;
using ore::data::FxSwap;
using ore::data::PairwiseVarSwap;
using ore::data::EqPairwiseVarSwap;
using ore::data::FxPairwiseVarSwap;
using ore::data::CFD;
using ore::data::SwaptionStraddle;
using ore::data::VarSwap;
using ore::data::EqVarSwap;
using ore::data::FxVarSwap;
using ore::data::ComVarSwap;
using OREBalanceGuaranteedSwap = ore::data::BalanceGuaranteedSwap;
using OREMultiLegOption = ore::data::MultiLegOption;
using ORERiskParticipationAgreement = ore::data::RiskParticipationAgreement;
%}

%rename(OREBondTRS) ore::data::BondTRS;
%rename(OREBarrierOption) ore::data::BarrierOption;
%rename(ORECallableSwap) ore::data::CallableSwap;
%rename(ORECliquetOption) ore::data::CliquetOption;
%rename(OREAscot) ore::data::Ascot;

%shared_ptr(BGSTrancheData)
class BGSTrancheData : public ore::data::XMLSerializable {
public:
    BGSTrancheData();
    BGSTrancheData(const std::string& description, const std::string& securityId, const int seniority,
                   const std::vector<QuantLib::Real>& notionals, const std::vector<std::string>& notionalDates);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(BGSTrancheDataVector) std::vector<ext::shared_ptr<BGSTrancheData>>;

%shared_ptr(OREBalanceGuaranteedSwap)
class OREBalanceGuaranteedSwap : public ore::data::Trade {
public:
    OREBalanceGuaranteedSwap();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend OREBalanceGuaranteedSwap {
    OREBalanceGuaranteedSwap(const Envelope& env, const std::string& referenceSecurity,
                          const std::vector<ext::shared_ptr<BGSTrancheData>>& tranches, const Schedule schedule,
                          const std::vector<ext::shared_ptr<LegData>>& swap) {
        return new OREBalanceGuaranteedSwap(env, referenceSecurity,
            VECTOR_SWIG_TO_ORE(tranches), schedule, VECTOR_SWIG_TO_ORE(swap));
    }
}
%pythoncode %{ BalanceGuaranteedSwap = OREBalanceGuaranteedSwap %}

// ore/OREData/ored/portfolio/barrieroption.hpp

%shared_ptr(ore::data::BarrierOption)
%shared_ptr(FxOptionWithBarrier)
%shared_ptr(EquityOptionWithBarrier)
%nodefaultctor FxOptionWithBarrier;
%nodefaultctor EquityOptionWithBarrier;

namespace ore {
namespace data {

class BarrierOption : virtual public Trade {
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

} // namespace data
} // namespace ore

class FxOptionWithBarrier : public OREBarrierOption {
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

class EquityOptionWithBarrier : public OREBarrierOption {
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

// ore/OREData/ored/portfolio/bondposition.hpp

%shared_ptr(BondPosition)
class BondPosition : public Trade {
public:
    BondPosition();
    BondPosition(const Envelope& env, const BondPositionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
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
// Renamed OREBondTRS to avoid clash with QuantExt::BondTRS instrument

%shared_ptr(ore::data::BondTRS)

namespace ore {
namespace data {

class BondTRS : public Trade {
public:
    BondTRS();
    BondTRS(Envelope env, const ore::data::BondData& bondData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore
%pythoncode %{ BondTRS = OREBondTRS %}

// ore/OREData/ored/portfolio/ascot.hpp

%shared_ptr(ore::data::Ascot)

namespace ore {
namespace data {

class Ascot : public Trade {
public:
    Ascot();
    Ascot(const Envelope& env, const ConvertibleBond& bond, const OptionData& optionData,
          const LegData& fundingLegData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore
%pythoncode %{ Ascot = OREAscot %}

// ore/OREData/ored/portfolio/callableswap.hpp

%shared_ptr(ore::data::CallableSwap)

namespace ore {
namespace data {

class CallableSwap : public Trade {
public:
    CallableSwap();
    CallableSwap(const Envelope& env, const Swap& swap, const Swaption& swaption);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore
%pythoncode %{ CallableSwap = ORECallableSwap %}

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

%shared_ptr(ore::data::CliquetOption)

namespace ore {
namespace data {

class CliquetOption : public Trade {
public:
    CliquetOption(const std::string& tradeType);
    CliquetOption(const std::string& tradeType, Envelope& env,
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

} // namespace data
} // namespace ore

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

// ore/OREData/ored/portfolio/compositetrade.hpp

%shared_ptr(CompositeTrade)
class CompositeTrade : public Trade {
public:
    CompositeTrade(const Envelope& env = Envelope(), const TradeActions& ta = TradeActions());
    CompositeTrade(const std::string currency, const std::vector<ext::shared_ptr<Trade>>& trades,
                   const std::string notionalCalculation = "", const Real notionalOverride = 0.0,
                   const Envelope& env = Envelope(), const TradeActions& ta = TradeActions(), const double indexQuantity=Null<Real>());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/crosscurrencyswap.hpp

%shared_ptr(CrossCurrencySwap)
class CrossCurrencySwap : public ORESwap {
public:
    CrossCurrencySwap();
    CrossCurrencySwap(const Envelope& env, const LegData& leg0, const LegData& leg1);
};
%extend CrossCurrencySwap {
    CrossCurrencySwap(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& legData) {
        return new CrossCurrencySwap(env, VECTOR_SWIG_TO_ORE(legData));
    }
}

// ore/OREData/ored/portfolio/equitybarrieroption.hpp

%shared_ptr(EquityBarrierOption)
class EquityBarrierOption : public EquityOptionWithBarrier {
public:
    EquityBarrierOption();
    EquityBarrierOption(Envelope& env, OptionData option, BarrierData barrier, QuantLib::Date startDate,
                        std::string calendar, EquityUnderlying equityUnderlying, QuantLib::Currency currency,
                        QuantLib::Real quantity, TradeStrike strike);
    void checkBarriers() override;
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    vanillaPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    barrierPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
};

%shared_ptr(EquityDigitalOption)
class EquityDigitalOption : public ore::data::Trade {
public:
    EquityDigitalOption();
    EquityDigitalOption(Envelope& env, OptionData option, double strike, const std::string& payoffCurrency, double payoffAmount,
                    const EquityUnderlying& equityUnderlying, double quantity);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equitydoublebarrieroption.hpp

%shared_ptr(EquityDoubleBarrierOption)
class EquityDoubleBarrierOption : public ore::data::Trade {
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
class EquityDoubleTouchOption : public ore::data::Trade {
public:
    EquityDoubleTouchOption();
    EquityDoubleTouchOption(Envelope& env, OptionData option, BarrierData barrier,
                            const EquityUnderlying& equityUnderlying, std::string payoffCurrency, double payoffAmount,
                            std::string startDate = "", std::string calendar = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityeuropeanbarrieroption.hpp

%shared_ptr(EquityEuropeanBarrierOption)
class EquityEuropeanBarrierOption : public ore::data::Trade {
public:
    EquityEuropeanBarrierOption();
    EquityEuropeanBarrierOption(Envelope& env, OptionData option, BarrierData barrier,
                                EquityUnderlying equityUnderlying, std::string currency, TradeStrike strike, double quantity);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityfuturesoption.hpp

%shared_ptr(EquityFutureOption)
class EquityFutureOption : public VanillaOptionTrade {
public:
    EquityFutureOption();
    EquityFutureOption(Envelope& env, OptionData option, const std::string& currency, Real quantity,
        const ext::shared_ptr<Underlying>& underlying, TradeStrike strike, Date forwardDate,
        const ext::shared_ptr<Index>& index = nullptr, const std::string& indexName = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityoptionposition.hpp

%shared_ptr(EquityOptionPosition)
class EquityOptionPosition : public Trade {
public:
    EquityOptionPosition();
    EquityOptionPosition(const Envelope& env, const EquityOptionPositionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityoutperformanceoption.hpp

%shared_ptr(EquityOutperformanceOption)
class EquityOutperformanceOption : public ore::data::Trade {
public:
    EquityOutperformanceOption();
    EquityOutperformanceOption(Envelope& env, OptionData option, const std::string& currency, Real notional,
        const ext::shared_ptr<Underlying>& underlying1,
        const ext::shared_ptr<Underlying>& underlying2,
        Real initialPrice1, Real initialPrice2, Real strike, const std::string& initialPriceCurrency1 = "",
        const std::string& initialPriceCurrency2 = "",
        Real knockInPrice = Null<Real>(), Real knockOutPrice = Null<Real>(), std::string fxIndex1 = "", std::string fxIndex2 = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equityposition.hpp

%shared_ptr(EquityPosition)
class EquityPosition : public Trade {
public:
    EquityPosition();
    EquityPosition(const Envelope& env, const EquityPositionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/equitytouchoption.hpp

%shared_ptr(EquityTouchOption)
class EquityTouchOption : public EquitySingleAssetDerivative {
public:
    EquityTouchOption();
    EquityTouchOption(Envelope& env, OptionData option, BarrierData barrier, const EquityUnderlying& equityUnderlying,
                      std::string payoffCurrency, double payoffAmount, std::string startDate = "", std::string calendar = "",
                      std::string eqIndex = "");
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
%extend FlexiSwap {
    FlexiSwap(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& swap,
              const std::vector<double>& lowerNotionalBounds, const std::vector<std::string>& lowerNotionalBoundsDates,
              const std::string& optionLongShort) {
        std::map<std::string, std::pair<std::vector<double>, std::vector<std::string>>> boundsMap;
        boundsMap[""] = std::make_pair(lowerNotionalBounds, lowerNotionalBoundsDates);
        return new FlexiSwap(env, VECTOR_SWIG_TO_ORE(swap), boundsMap, optionLongShort);
    }
}

// ore/OREData/ored/portfolio/fxaverageforward.hpp

%shared_ptr(FxAverageForward)
class FxAverageForward : public Trade {
public:
    FxAverageForward();
    FxAverageForward(const Envelope& env, const ScheduleData& observationDates, const std::string& paymentDate,
             bool fixedPayer,
             const std::string& referenceCurrency, double referenceNotional,
             const std::string settlementCurrency, double settlementNotional,
             const std::string& fxIndex, const std::string& settlement = "Cash");
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
                           const std::string& foreignCurrency, const std::string& domesticCurrency, const std::string& startDate = "",
                           const std::string& calendar = "", const std::string& fxIndex = "", const std::string& payoffCurrency = "",
                           const std::string& fxIndexDailyLows = "", const std::string& fxIndexDailyHighs = "");
};

// ore/OREData/ored/portfolio/fxdigitaloption.hpp

%shared_ptr(FxDigitalOption)
class FxDigitalOption : public FxSingleAssetDerivative {
public:
    FxDigitalOption();
    FxDigitalOption(Envelope& env, OptionData option, double strike, const std::string& payoffCurrency, double payoffAmount,
                    const std::string& foreignCurrency, const std::string& domesticCurrency);
    FxDigitalOption(Envelope& env, OptionData option, double strike, double payoffAmount, const std::string& foreignCurrency,
                    const std::string& domesticCurrency);
};

// ore/OREData/ored/portfolio/fxdoubletouchoption.hpp

%shared_ptr(FxDoubleTouchOption)
class FxDoubleTouchOption : public FxSingleAssetDerivative {
public:
    FxDoubleTouchOption();
    FxDoubleTouchOption(Envelope& env, OptionData option, BarrierData barrier, std::string foreignCurrency,
                        std::string domesticCurrency, std::string payoffCurrency, double payoffAmount, std::string startDate = "",
                        std::string calendar = "", std::string fxIndex = "");
};

// ore/OREData/ored/portfolio/fxkikobarrieroption.hpp

%shared_ptr(FxKIKOBarrierOption)
class FxKIKOBarrierOption : public FxSingleAssetDerivative {
public:
    FxKIKOBarrierOption();
};
%extend FxKIKOBarrierOption {
    FxKIKOBarrierOption(Envelope& env, OptionData option, std::vector<ext::shared_ptr<BarrierData>> barriers, std::string boughtCurrency,
                        double boughtAmount, std::string soldCurrency, double soldAmount, std::string startDate = "",
                        std::string calendar = "", std::string fxIndex = "") {
        return new FxKIKOBarrierOption(env, option, VECTOR_SWIG_TO_ORE(barriers), boughtCurrency, boughtAmount,
            soldCurrency, soldAmount, startDate, calendar, fxIndex);
    }
}

// ore/OREData/ored/portfolio/fxswap.hpp

%shared_ptr(FxSwap)
class FxSwap : public Trade {
public:
    FxSwap();
    FxSwap(const Envelope& env, const std::string& nearDate, const std::string& farDate, const std::string& nearBoughtCurrency,
           double nearBoughtAmount, const std::string& nearSoldCurrency, double nearSoldAmount, double farBoughtAmount,
           double farSoldAmount, const std::string& settlement = "Physical");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/multilegoption.hpp
// Renamed OREMultiLegOption to avoid clash with QuantExt::MultiLegOption instrument
// Kept as an alias wrapper because direct %rename with the vector-converting %extend helper
// emits duplicate constructor entry points in generated SWIG code.

%shared_ptr(OREMultiLegOption)
class OREMultiLegOption : public Trade {
public:
    OREMultiLegOption();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend OREMultiLegOption {
    OREMultiLegOption(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& underlyingData) {
        return new OREMultiLegOption(env, VECTOR_SWIG_TO_ORE(underlyingData));
    }
    OREMultiLegOption(const Envelope& env, const OptionData& optionData, const std::vector<ext::shared_ptr<LegData>>& underlyingData) {
        return new OREMultiLegOption(env, optionData, VECTOR_SWIG_TO_ORE(underlyingData));
    }
}
%pythoncode %{ MultiLegOption = OREMultiLegOption %}

// ore/OREData/ored/portfolio/pairwisevarianceswap.hpp

%shared_ptr(PairwiseVarSwap)
%nodefaultctor PairwiseVarSwap;
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
    EqPairwiseVarSwap(Envelope& env, std::string longShort,
                      const std::vector<ext::shared_ptr<Underlying>>& underlyings, std::vector<double> underlyingStrikes,
                      std::vector<double> underlyingNotionals, double basketNotional, double basketStrike,
                      ScheduleData valuationSchedule, std::string currency, std::string settlementDate,
                      ScheduleData laggedValuationSchedule, double payoffLimit = 0.0, double cap = 0.0,
                      double floor = 0.0, int accrualLag = 1);
};

%shared_ptr(FxPairwiseVarSwap)
class FxPairwiseVarSwap : public PairwiseVarSwap {
public:
    FxPairwiseVarSwap();
    FxPairwiseVarSwap(Envelope& env, std::string longShort, const std::vector<ext::shared_ptr<Underlying>>& underlyings,
                      std::vector<double> underlyingStrikes, std::vector<double> underlyingNotionals, double basketNotional,
                      double basketStrike, ScheduleData valuationSchedule, std::string currency, std::string settlementDate,
                      ScheduleData laggedValuationSchedule, double payoffLimit = 0.0, double cap = 0.0,
                      double floor = 0.0, int accrualLag = 1);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/riskparticipationagreement.hpp
// Renamed ORERiskParticipationAgreement to avoid clash with QuantExt::RiskParticipationAgreement instrument
// Kept as an alias wrapper because direct %rename with the vector-converting %extend helper
// emits duplicate constructor entry points in generated SWIG code.

%shared_ptr(ORERiskParticipationAgreement)
class ORERiskParticipationAgreement : public ore::data::Trade {
public:
    ORERiskParticipationAgreement();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend ORERiskParticipationAgreement {
    ORERiskParticipationAgreement(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& underlying,
                               const std::vector<ext::shared_ptr<LegData>>& protectionFee, const Real participationRate,
                               const Date& protectionStart, const Date& protectionEnd, const std::string& creditCurveId,
                               const std::string& issuerId = "", const bool settlesAccrual = true,
                               const Real fixedRecoveryRate = Null<Real>(),
                               const ext::optional<OptionData>& optionData = ext::nullopt) {
        return new ORERiskParticipationAgreement(env, VECTOR_SWIG_TO_ORE(underlying), VECTOR_SWIG_TO_ORE(protectionFee),
            participationRate, protectionStart, protectionEnd, creditCurveId, issuerId, settlesAccrual,
            fixedRecoveryRate, optionData);
    }
    ORERiskParticipationAgreement(const Envelope& env, const TreasuryLockData& tlockData,
                               const std::vector<ext::shared_ptr<LegData>>& protectionFee, const Real participationRate,
                               const Date& protectionStart, const Date& protectionEnd, const std::string& creditCurveId,
                               const std::string& issuerId = "", const bool settlesAccrual = true,
                               const Real fixedRecoveryRate = Null<Real>()) {
        return new ORERiskParticipationAgreement(env, tlockData, VECTOR_SWIG_TO_ORE(protectionFee),
            participationRate, protectionStart, protectionEnd, creditCurveId, issuerId, settlesAccrual, fixedRecoveryRate);
    }
}

// ore/OREData/ored/portfolio/swaptionstraddle.hpp

%shared_ptr(SwaptionStraddle)
class SwaptionStraddle : public Trade {
public:
    SwaptionStraddle();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend SwaptionStraddle {
    SwaptionStraddle(const Envelope& env, const OptionData& optionData, const std::vector<ext::shared_ptr<LegData>>& legData) {
        return new SwaptionStraddle(env, optionData, VECTOR_SWIG_TO_ORE(legData));
    }
}

// ore/OREData/ored/portfolio/trs.hpp (CFD extends TRS)

%shared_ptr(CFD)
class CFD : public TRS {
public:
    CFD();
    CFD(const Envelope& env, const std::vector<ext::shared_ptr<Trade>>& underlying,
        const std::vector<std::string>& underlyingDerivativeId, const TRS::ReturnData& returnData,
        const TRS::FundingData& fundingData, const TRS::AdditionalCashflowData& additionalCashflowData);
};

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

#endif
