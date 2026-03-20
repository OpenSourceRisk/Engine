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
using ORESwap = ore::data::Swap;
using OREForwardRateAgreement = ore::data::ForwardRateAgreement;
using ORESwaption = ore::data::Swaption;
using ore::data::VanillaOptionTrade;
using ore::data::FxOption;
using OREFxForward = ore::data::FxForward;
using ore::data::EquityOption;
using OREEquityForward = ore::data::EquityForward;
using ORECapFloor = ore::data::CapFloor;
using ore::data::BondData;
using OREBond = ore::data::Bond;
using ORECommodityForward = ore::data::CommodityForward;
using ORECommoditySwap = ore::data::CommoditySwap;
using ORECommodityOption = ore::data::CommodityOption;
using ore::data::ForwardBond;
using ore::data::BondOption;
using ore::data::TRS;
using ore::data::CallableBondData;
using ORECallableBond = ore::data::CallableBond;
using ore::data::ConvertibleBondData;
using OREConvertibleBond = ore::data::ConvertibleBond;
using ore::data::parseTrsFundingNotionalType;
using ore::data::FxDoubleBarrierOption;
using ore::data::FxEuropeanBarrierOption;
using ore::data::FxBarrierOption;
using ore::data::FxTouchOption;
%}

%shared_ptr(ORESwap)
class ORESwap : public Trade {
public:
    ORESwap(const std::string swapType = "Swap");
    ORESwap(const Envelope& env, const std::string swapType = "Swap");
    ORESwap(const Envelope& env, const std::vector<LegData>& legData, const std::string swapType = "Swap",
         const std::string settlement = "Physical");
    ORESwap(const Envelope& env, const LegData& leg0, const LegData& leg1, const std::string swapType = "Swap",
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

%shared_ptr(OREForwardRateAgreement)
class OREForwardRateAgreement : public ORESwap {
public:
    OREForwardRateAgreement();
    OREForwardRateAgreement(Envelope& env, std::string longShort, std::string currency, std::string startDate, std::string endDate,
                         std::string index, double strike, double amount);

    void build(const ext::shared_ptr<EngineFactory>& engineFactory) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void fromXMLString(const std::string& xmlString);
    std::string toXMLString();
    const std::string& index() const { return index_; }

    const std::map<std::string,QuantLib::ext::any>& additionalData() const override;
};

%shared_ptr(ORESwaption)
class ORESwaption : public Trade {
public:
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend ORESwaption {
    ORESwaption(const Envelope& env, const OptionData& optionData, const std::vector<ext::shared_ptr<LegData>>& legData) {
        return new ORESwaption(env, optionData, VECTOR_SWIG_TO_ORE(legData));
    }
}

%shared_ptr(VanillaOptionTrade)
class VanillaOptionTrade : public Trade { };

%shared_ptr(FxOption)
class FxOption : public VanillaOptionTrade {
public:
    FxOption(const Envelope& env, const OptionData& option, const std::string& boughtCurrency, double boughtAmount,
             const std::string& soldCurrency, double soldAmount, const std::string& fxIndex = "", double delta = 0.0);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OREFxForward)
class OREFxForward : public Trade {
public:
    OREFxForward();
    OREFxForward(const Envelope& env, const std::string& maturityDate, const std::string& boughtCurrency, double boughtAmount,
              const std::string& soldCurrency, double soldAmount, const std::string& settlement = "Physical",
              const std::string& fxIndex = "", const std::string& payDate = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(EquityOption)
class EquityOption : public VanillaOptionTrade {
public:
    EquityOption(Envelope& env, OptionData option, EquityUnderlying equityUnderlying, std::string currency,
        QuantLib::Real quantity, TradeStrike tradeStrike);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OREEquityForward)
class OREEquityForward : public Trade {
public:
    OREEquityForward();
    OREEquityForward(Envelope& env, const std::string& longShort, EquityUnderlying equityUnderlying, const std::string& currency,
                  QuantLib::Real quantity, const std::string& maturityDate, QuantLib::Real strike,
                  const string& strikeCurrency = "", const std::string& fxIndex = "", const std::string& payDate = "",
                  std::string payLag = "", const std::string& payCalendar = "" , const std::string& payConvention = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECapFloor)
class ORECapFloor : public Trade {
public:
    ORECapFloor();
    ORECapFloor(const Envelope& env, const std::string& longShort, const LegData& leg, const std::vector<double>& caps,
             const std::vector<double>& floors, const PremiumData& premiumData = {});
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(BondData)
class BondData : public XMLSerializable {
public:
    BondData(std::string issuerId, std::string creditCurveId, std::string securityId, std::string referenceCurveId, std::string settlementDays,
             std::string calendar, Real faceAmount, std::string maturityDate, std::string currency, std::string issueDate,
             bool hasCreditRisk = true);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(OREBond)
class OREBond : public Trade {
public:
    OREBond();
    OREBond(Envelope env, const BondData& bondData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECommodityForward)
class ORECommodityForward : public Trade {
public:
    ORECommodityForward();
    ORECommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike);
    ORECommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike, const QuantLib::Date& futureExpiryDate,
                     const QuantLib::ext::optional<bool>& physicallySettled = true,
                     const QuantLib::Date& paymentDate = QuantLib::Date());
    ORECommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike, const QuantLib::Period& futureExpiryOffset,
                     const QuantLib::Calendar& offsetCalendar,
                     const QuantLib::ext::optional<bool>& physicallySettled = true,
                     const QuantLib::Date& paymentDate = QuantLib::Date());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECommoditySwap)
class ORECommoditySwap : public Trade {
public:
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend ORECommoditySwap {
    ORECommoditySwap(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& legs) {
        return new ORECommoditySwap(env, VECTOR_SWIG_TO_ORE(legs));
    }
}

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
    ForwardBond(Envelope env, const BondData& bondData, std::string fwdMaturityDate, std::string fwdSettlementDate,
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
    BondOption(Envelope env, const BondData& bondData, const OptionData& optionData, TradeStrike strike, bool knocksOut);
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
    explicit CallableBondData(const BondData& bondData);
    const BondData& bondData() const;
    const CallabilityData& callData() const;
    const CallabilityData& putData() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void populateFromBondReferenceData(const ext::shared_ptr<ReferenceDataManager>& referenceData);
};

%shared_ptr(ORECallableBond)
class ORECallableBond : public Trade {
public:
    ORECallableBond();
    ORECallableBond(const Envelope& env, const CallableBondData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
    const CallableBondData& data() const;
    const BondData& bondData() const;
};

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
    explicit ConvertibleBondData(const BondData& bondData);
    const BondData& bondData() const;
    const CallabilityData& callData() const;
    const CallabilityData& putData() const;
    const ConversionData& conversionData() const;
    const DividendProtectionData& dividendProtectionData() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void populateFromBondReferenceData(const ext::shared_ptr<ReferenceDataManager>& referenceData);
};

%shared_ptr(OREConvertibleBond)
class OREConvertibleBond : public Trade {
public:
    OREConvertibleBond();
    OREConvertibleBond(const Envelope& env, const ConvertibleBondData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
    const ConvertibleBondData& data() const;
    const BondData& bondData() const;
};
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
using OREBondTRS = ore::data::BondTRS;
using OREMultiLegOption = ore::data::MultiLegOption;
using ORERiskParticipationAgreement = ore::data::RiskParticipationAgreement;
using OREBarrierOption = ore::data::BarrierOption;
using ore::data::FxOptionWithBarrier;
using ore::data::EquityOptionWithBarrier;
using ore::data::BGSTrancheData;
using OREBalanceGuaranteedSwap = ore::data::BalanceGuaranteedSwap;
using ore::data::BondFuture;
using ore::data::BondPosition;
using ore::data::BondRepo;
using ore::data::Ascot;
using ORECallableSwap = ore::data::CallableSwap;
using ore::data::CashPosition;
using ore::data::CBO;
using ORECliquetOption = ore::data::CliquetOption;
using ore::data::EquityCliquetOption;
using ore::data::CompositeTrade;
using OREAscot = ore::data::Ascot;
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
%}

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

%shared_ptr(OREBondTRS)
class OREBondTRS : public Trade {
public:
    OREBondTRS();
    OREBondTRS(Envelope env, const BondData& bondData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%pythoncode %{ BondTRS = OREBondTRS %}

// ore/OREData/ored/portfolio/ascot.hpp

%shared_ptr(OREAscot)
class OREAscot : public Trade {
public:
    OREAscot();
    OREAscot(const Envelope& env, const OREConvertibleBond& bond, const OptionData& optionData,
          const LegData& fundingLegData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%pythoncode %{ Ascot = OREAscot %}

// ore/OREData/ored/portfolio/callableswap.hpp

%shared_ptr(ORECallableSwap)
class ORECallableSwap : public ore::data::Trade {
public:
    ORECallableSwap();
    ORECallableSwap(const Envelope& env, const ORESwap& swap, const ORESwaption& swaption);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
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

// ore/OREData/ored/portfolio/crosscurrencyswap.hpp

%shared_ptr(CrossCurrencySwap)
class CrossCurrencySwap : public ORESwap {
public:
    CrossCurrencySwap();
    CrossCurrencySwap(const Envelope& env, const LegData& leg0, const LegData& leg1);
};
%extend CrossCurrencySwap {
    CrossCurrencySwap(const Envelope& env, const vector<ext::shared_ptr<LegData>>& legData) {
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
    EquityDigitalOption(Envelope& env, OptionData option, double strike, const string& payoffCurrency, double payoffAmount,
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
                            const EquityUnderlying& equityUnderlying, string payoffCurrency, double payoffAmount,
                            string startDate = "", string calendar = "");
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
    EquityOutperformanceOption(Envelope& env, OptionData option, const string& currency, Real notional,
        const ext::shared_ptr<Underlying>& underlying1,
        const ext::shared_ptr<Underlying>& underlying2,
        Real initialPrice1, Real initialPrice2, Real strike, const string& initialPriceCurrency1 = "",
        const string& initialPriceCurrency2 = "",
        Real knockInPrice = Null<Real>(), Real knockOutPrice = Null<Real>(), string fxIndex1 = "", string fxIndex2 = "");
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
                      string payoffCurrency, double payoffAmount, string startDate = "", string calendar = "",
                      string eqIndex = "");
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
        return new FlexiSwap(env, VECTOR_SWIG_TO_ORE(swap), lowerNotionalBounds, lowerNotionalBoundsDates, optionLongShort);
    }
    FlexiSwap(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& swap,
              const std::string& noticePeriod, const std::string& noticeCalendar, const std::string& noticeConvention,
              const std::vector<std::string>& exerciseDates, const std::vector<std::string>& exerciseTypes,
              const std::vector<double>& exerciseValues, const std::string& optionLongShort) {
        return new FlexiSwap(env, VECTOR_SWIG_TO_ORE(swap), noticePeriod, noticeCalendar, noticeConvention,
            exerciseDates, exerciseTypes, exerciseValues, optionLongShort);
    }
}

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

// ore/OREData/ored/portfolio/fxdoubletouchoption.hpp

%shared_ptr(FxDoubleTouchOption)
class FxDoubleTouchOption : public FxSingleAssetDerivative {
public:
    FxDoubleTouchOption();
    FxDoubleTouchOption(Envelope& env, OptionData option, BarrierData barrier, string foreignCurrency,
                        string domesticCurrency, string payoffCurrency, double payoffAmount, string startDate = "",
                        string calendar = "", string fxIndex = "");
};

// ore/OREData/ored/portfolio/fxkikobarrieroption.hpp

%shared_ptr(FxKIKOBarrierOption)
class FxKIKOBarrierOption : public FxSingleAssetDerivative {
public:
    FxKIKOBarrierOption();
};
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
    FxSwap(const Envelope& env, const string& nearDate, const string& farDate, const string& nearBoughtCurrency,
           double nearBoughtAmount, const string& nearSoldCurrency, double nearSoldAmount, double farBoughtAmount,
           double farSoldAmount, const string& settlement = "Physical");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/multilegoption.hpp
// Renamed OREMultiLegOption to avoid clash with QuantExt::MultiLegOption instrument

%shared_ptr(OREMultiLegOption)
class OREMultiLegOption : public Trade {
public:
    OREMultiLegOption();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend OREMultiLegOption {
    OREMultiLegOption(const Envelope& env, const vector<ext::shared_ptr<LegData>>& underlyingData) {
        return new OREMultiLegOption(env, VECTOR_SWIG_TO_ORE(underlyingData));
    }
    OREMultiLegOption(const Envelope& env, const OptionData& optionData, const vector<ext::shared_ptr<LegData>>& underlyingData) {
        return new OREMultiLegOption(env, optionData, VECTOR_SWIG_TO_ORE(underlyingData));
    }
}
%pythoncode %{ MultiLegOption = OREMultiLegOption %}

// ore/OREData/ored/portfolio/pairwisevarianceswap.hpp

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

// ore/OREData/ored/portfolio/riskparticipationagreement.hpp
// Renamed ORERiskParticipationAgreement to avoid clash with QuantExt::RiskParticipationAgreement instrument

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
