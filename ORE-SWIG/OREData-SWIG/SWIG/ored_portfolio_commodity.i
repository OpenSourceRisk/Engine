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
using namespace std;
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
    CommoditySpreadOptionData(const std::vector<ore::data::LegData>& legData, const ore::data::OptionData& optionData,
                              QuantLib::Real strike);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

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

#endif
