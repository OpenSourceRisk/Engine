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

%shared_ptr(TRS)
class TRS : public Trade {
public:
    TRS();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
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
