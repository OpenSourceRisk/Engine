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

%shared_ptr(ore::data::CommodityDigitalOption)
%shared_ptr(ore::data::CommoditySpreadOptionData)
%shared_ptr(ore::data::CommoditySpreadOption)
%shared_ptr(ore::data::CommodityAveragePriceOption)
%shared_ptr(ore::data::CommodityDigitalAveragePriceOption)
%shared_ptr(ore::data::CommodityOption)
%shared_ptr(ore::data::CommodityOptionStrip)
%shared_ptr(ore::data::CommodityPosition)
%shared_ptr(ore::data::CommoditySwap)
%shared_ptr(ore::data::CommoditySwaption)

namespace ore {
namespace data {

class CommodityDigitalOption : public Trade {
public:
    CommodityDigitalOption();
    CommodityDigitalOption(const ore::data::Envelope& env, const ore::data::OptionData& optionData, const std::string& commodityName,
                           const std::string& currency, QuantLib::Real strike, QuantLib::Real payoff,
                           const QuantLib::ext::optional<bool>& isFuturePrice = QuantLib::ext::nullopt,
                           const QuantLib::Date& futureExpiryDate = QuantLib::Date());
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class CommoditySpreadOptionData : public XMLSerializable {
public:
    CommoditySpreadOptionData();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CommoditySpreadOptionData {
    CommoditySpreadOptionData(const std::vector<ext::shared_ptr<ore::data::LegData>>& legData, const ore::data::OptionData& optionData,
                              QuantLib::Real strike) {
        return new ore::data::CommoditySpreadOptionData(VECTOR_SWIG_TO_ORE(legData), optionData, strike);
    }
}

class CommoditySpreadOption : public Trade {
public:
    CommoditySpreadOption();
    CommoditySpreadOption(const ore::data::CommoditySpreadOptionData& data);
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

class CommodityAveragePriceOption : public Trade {
public:
    CommodityAveragePriceOption();
    CommodityAveragePriceOption(
        const ore::data::Envelope& envelope, const ore::data::OptionData& optionData, QuantLib::Real quantity,
        QuantLib::Real strike, const std::string& currency, const std::string& name, CommodityPriceType priceType,
        const std::string& startDate, const std::string& endDate, const std::string& paymentCalendar,
        const std::string& paymentLag, const std::string& paymentConvention, const std::string& pricingCalendar,
        const std::string& paymentDate = "", QuantLib::Real gearing = 1.0, QuantLib::Spread spread = 0.0,
        QuantExt::CommodityQuantityFrequency commodityQuantityFrequency =
            QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
        CommodityPayRelativeTo commodityPayRelativeTo = CommodityPayRelativeTo::CalculationPeriodEndDate,
        QuantLib::Integer futureMonthOffset = 0, QuantLib::Natural deliveryRollDays = 0, bool includePeriodEnd = true,
        const ore::data::BarrierData& barrierData = {}, const std::string& fxIndex = "");
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// Additional commodity trade types from ored_portfolio2.i

// ore/OREData/ored/portfolio/commoditydigitalaveragepriceoption.hpp

class CommodityDigitalAveragePriceOption : public Trade {
public:
    CommodityDigitalAveragePriceOption();
    CommodityDigitalAveragePriceOption(
        const ore::data::Envelope& envelope, const ore::data::OptionData& optionData,
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
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commodityoption.hpp

class CommodityOption : public VanillaOptionTrade {
public:
    CommodityOption();
    CommodityOption(const ore::data::Envelope& env, const ore::data::OptionData& optionData, const std::string& commodityName,
                    const std::string& currency, QuantLib::Real quantity, TradeStrike strike,
                    const QuantLib::ext::optional<bool>& isFuturePrice = QuantLib::ext::nullopt,
                    const QuantLib::Date& futureExpiryDate = QuantLib::Date());
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;
};
%pythoncode %{ ORECommodityOption = CommodityOption %}

// ore/OREData/ored/portfolio/commodityoptionstrip.hpp

class CommodityOptionStrip : public Trade {
public:
    CommodityOptionStrip();
    CommodityOptionStrip(const ore::data::Envelope& envelope, const ore::data::LegData& legData,
                         const std::vector<Position::Type>& callPositions,
                         const std::vector<QuantLib::Real>& callStrikes,
                         const std::vector<Position::Type>& putPositions,
                         const std::vector<QuantLib::Real>& putStrikes, QuantLib::Real premium = 0.0,
                         const std::string& premiumCurrency = "",
                         const QuantLib::Date& premiumPayDate = QuantLib::Date(),
                         const std::string& style = "", const std::string& settlement = "",
                         const ore::data::BarrierData& callBarrierData = {},
                         const ore::data::BarrierData& putBarrierData = {},
                         const std::string& fxIndex = "",
                         const bool isDigital = false,
                         QuantLib::Real payoffPerUnit = 0.0);
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commodityposition.hpp

class CommodityPosition : public Trade {
public:
    CommodityPosition();
    CommodityPosition(const ore::data::Envelope& env, const ore::data::CommodityPositionData& data);
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/commodityswaption.hpp

class CommoditySwaption : public Trade {
public:
    CommoditySwaption();
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CommoditySwaption {
    CommoditySwaption(const ore::data::Envelope& env, const ore::data::OptionData& optionData,
                      const std::vector<ext::shared_ptr<ore::data::LegData>>& legs) {
        return new ore::data::CommoditySwaption(env, optionData, VECTOR_SWIG_TO_ORE(legs));
    }
}

} // namespace data
} // namespace ore

#endif
