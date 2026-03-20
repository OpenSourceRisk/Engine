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

#ifndef ored_portfolio_commodity_i
#define ored_portfolio_commodity_i

%{
using ore::data::CommodityDigitalOption;
using ore::data::CommoditySpreadOptionData;
using ore::data::CommoditySpreadOption;
using ore::data::CommodityAveragePriceOption;
%}

%shared_ptr(CommodityDigitalOption)
class CommodityDigitalOption : public Trade {
public:
    CommodityDigitalOption();
    CommodityDigitalOption(const Envelope& env, const OptionData& optionData, const std::string& commodityName,
                           const std::string& currency, QuantLib::Real strike, QuantLib::Real payoff,
                           const QuantLib::ext::optional<bool>& isFuturePrice = QuantLib::ext::nullopt,
                           const QuantLib::Date& futureExpiryDate = QuantLib::Date());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CommoditySpreadOptionData)
class CommoditySpreadOptionData : public XMLSerializable {
public:
    CommoditySpreadOptionData();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CommoditySpreadOptionData {
    CommoditySpreadOptionData(const std::vector<ext::shared_ptr<LegData>>& legData, const ore::data::OptionData& optionData,
                              QuantLib::Real strike) {
        return new CommoditySpreadOptionData(VECTOR_SWIG_TO_ORE(legData), optionData, strike);
    }
}

%shared_ptr(CommoditySpreadOption)
class CommoditySpreadOption : public Trade {
public:
    CommoditySpreadOption();
    CommoditySpreadOption(const CommoditySpreadOptionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CommodityAveragePriceOption)
class CommodityAveragePriceOption : public Trade {
public:
    CommodityAveragePriceOption();
    CommodityAveragePriceOption(
        const Envelope& envelope, const OptionData& optionData, QuantLib::Real quantity,
        QuantLib::Real strike, const std::string& currency, const std::string& name, CommodityPriceType priceType,
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

// Additional commodity trade types from ored_portfolio2.i

%{
using ore::data::CommodityDigitalAveragePriceOption;
using ore::data::CommodityOption;
using ore::data::CommodityOptionStrip;
using ore::data::CommodityPosition;
using ore::data::CommoditySwap;
using ore::data::CommoditySwaption;
%}

// ore/OREData/ored/portfolio/commoditydigitalaveragepriceoption.hpp

%shared_ptr(CommodityDigitalAveragePriceOption)
class CommodityDigitalAveragePriceOption : public Trade {
public:
    CommodityDigitalAveragePriceOption();
    CommodityDigitalAveragePriceOption(
        const Envelope& envelope, const OptionData& optionData,
        QuantLib::Real strike, QuantLib::Real digitalCashPayoff, const std::string& currency, const std::string& name,
        CommodityPriceType priceType, const std::string& startDate, const std::string& endDate,
        const std::string& paymentCalendar, const std::string& paymentLag, const std::string& paymentConvention,
        const std::string& pricingCalendar, const std::string& paymentDate = "", QuantLib::Real gearing = 1.0,
        QuantLib::Spread spread = 0.0,
        QuantExt::CommodityQuantityFrequency commodityQuantityFrequency =
            QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
        CommodityPayRelativeTo commodityPayRelativeTo = CommodityPayRelativeTo::CalculationPeriodEndDate,
        QuantLib::Integer futureMonthOffset = 0, QuantLib::Natural deliveryRollDays = 0,
        bool includePeriodEnd = true);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
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
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;
};
%pythoncode %{ ORECommodityOption = CommodityOption %}

// ore/OREData/ored/portfolio/commodityoptionstrip.hpp

%template(PositionTypeVector) std::vector<Position::Type>;
%shared_ptr(CommodityOptionStrip)
class CommodityOptionStrip : public Trade {
public:
    CommodityOptionStrip();
    CommodityOptionStrip(const Envelope& envelope, const LegData& legData,
                         const std::vector<Position::Type>& callPositions,
                         const std::vector<QuantLib::Real>& callStrikes,
                         const std::vector<Position::Type>& putPositions,
                         const std::vector<QuantLib::Real>& putStrikes, QuantLib::Real premium = 0.0,
                         const std::string& premiumCurrency = "",
                         const QuantLib::Date& premiumPayDate = QuantLib::Date(),
                         const std::string& style = "", const std::string& settlement = "",
                         const BarrierData& callBarrierData = {},
                         const BarrierData& putBarrierData = {},
                         const std::string& fxIndex = "",
                         const bool isDigital = false,
                         QuantLib::Real payoffPerUnit = 0.0);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commodityposition.hpp

%shared_ptr(CommodityPosition)
class CommodityPosition : public Trade {
public:
    CommodityPosition();
    CommodityPosition(const Envelope& env, const CommodityPositionData& data);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commodityswap.hpp

%shared_ptr(CommoditySwap)
class CommoditySwap : public Trade {
public:
    CommoditySwap();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CommoditySwap {
    CommoditySwap(const Envelope& env, const std::vector<ext::shared_ptr<LegData>>& legs) {
        return new CommoditySwap(env, VECTOR_SWIG_TO_ORE(legs));
    }
}

// ore/OREData/ored/portfolio/commodityswaption.hpp

%shared_ptr(CommoditySwaption)
class CommoditySwaption : public Trade {
public:
    CommoditySwaption();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CommoditySwaption {
    CommoditySwaption(const Envelope& env, const OptionData& optionData,
                      const std::vector<ext::shared_ptr<LegData>>& legs) {
        return new CommoditySwaption(env, optionData, VECTOR_SWIG_TO_ORE(legs));
    }
}

#endif
