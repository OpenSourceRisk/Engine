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
    OREBond(Envelope env, const BondData& bondData);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECommodityForward)
class ORECommodityForward : public Trade {
public:
    ORECommodityForward(const Envelope& envelope, const std::string& position, const std::string& commodityName,
                     const std::string& currency, QuantLib::Real quantity, const std::string& maturityDate,
                     QuantLib::Real strike);
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

%shared_ptr(ORECommodityOption)
class ORECommodityOption : public VanillaOptionTrade {
public:
    ORECommodityOption(const Envelope& env, const OptionData& optionData, const std::string& commodityName,
                    const std::string& currency, QuantLib::Real quantity, TradeStrike strike,
                    const QuantLib::ext::optional<bool>& isFuturePrice = QuantLib::ext::nullopt,
                    const QuantLib::Date& futureExpiryDate = QuantLib::Date());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

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

%feature("flatnested") TRS;
%rename(TRSReturnData) TRS::ReturnData;
%rename(TRSFundingData) TRS::FundingData;
%rename(TRSAdditionalCashflowData) TRS::AdditionalCashflowData;
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
        ReturnData(const bool payer, const std::string& currency, const ScheduleData& scheduleData,
                   const std::string& observationLag, const std::string& observationConvention,
                   const std::string& observationCalendar, const std::string& paymentLag,
                   const std::string& paymentConvention, const std::string& paymentCalendar,
                   const std::vector<std::string>& paymentDates, const QuantLib::Real initialPrice,
                   const std::string& initialPriceCurrency, const std::vector<std::string>& fxTerms,
                   const QuantLib::ext::optional<bool> payUnderlyingCashFlowsImmediately,
                   const QuantLib::ext::optional<FXConversion> fxConversion);

        bool payer() const;
        const std::string& currency() const;
        const ScheduleData& scheduleData() const;
        const std::string& observationLag() const;
        const std::string& observationConvention() const;
        const std::string& observationCalendar() const;
        const std::string& paymentLag() const;
        const std::string& paymentConvention() const;
        const std::string& paymentCalendar() const;
        const std::vector<std::string>& paymentDates() const;
        QuantLib::Real initialPrice() const;
        const std::string& initialPriceCurrency() const;
        const std::vector<std::string>& fxTerms() const;
        QuantLib::ext::optional<bool> payUnderlyingCashFlowsImmediately() const;
        QuantLib::ext::optional<FXConversion> fxConversionAtPeriodEnd() const;
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        FXConversion parseFXConversion(std::string fxConv_);
    };

    class FundingData : public XMLSerializable {
    public:
        enum class NotionalType { PeriodReset, DailyReset, Fixed };

        FundingData();
        explicit FundingData(const std::vector<LegData>& legData,
                             const std::vector<NotionalType>& notionalType = {},
                             const QuantLib::Size fundingResetGracePeriod = 0);

        const std::vector<LegData>& legData() const;
        const std::vector<NotionalType>& notionalType() const;
        QuantLib::Size fundingResetGracePeriod() const;
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
    };

    class AdditionalCashflowData : public XMLSerializable {
    public:
        AdditionalCashflowData();
        AdditionalCashflowData(const LegData& legData);
        const LegData& legData() const;
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
    };

    TRS();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend TRS::FundingData {
    FundingData(const std::vector<ext::shared_ptr<LegData>>& legData,
                const std::vector<TRS::FundingData::NotionalType>& notionalType = {},
                const QuantLib::Size fundingResetGracePeriod = 0) {
        return new TRS::FundingData(VECTOR_SWIG_TO_ORE(legData), notionalType, fundingResetGracePeriod);
    }
}
%extend TRS {
    TRS(const Envelope& env, const std::vector<ext::shared_ptr<Trade>>& underlying,
        const std::vector<std::string>& underlyingDerivativeId, const TRS::ReturnData& returnData,
        const TRS::FundingData& fundingData,
        const TRS::AdditionalCashflowData& additionalCashflowData) {
        return new TRS(env, underlying, underlyingDerivativeId, returnData, fundingData,
                       additionalCashflowData);
    }
}

TRS::FundingData::NotionalType parseTrsFundingNotionalType(const std::string& s);

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

%shared_ptr(FxDoubleBarrierOption)
class FxDoubleBarrierOption : public Trade {
public:
    FxDoubleBarrierOption();
    FxDoubleBarrierOption(Envelope& env, OptionData option, BarrierData barrier, QuantLib::Date startDate,
    std::string calendar, std::string boughtCurrency, QuantLib::Real boughtAmount, std::string soldCurrency,
    QuantLib::Real soldAmount, std::string fxIndex = "");
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

#endif
