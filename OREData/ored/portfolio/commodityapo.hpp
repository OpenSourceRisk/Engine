/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/commodityapo.hpp
    \brief Commodity Average Price Option data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/commoditylegdata.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradefactory.hpp>

#include <qle/indexes/commodityindex.hpp>

namespace ore {
namespace data {

/*! Serializable Commodity Average Price Option
    \ingroup tradedata
*/
class CommodityAveragePriceOption : public ore::data::Trade {
public:
    CommodityAveragePriceOption() : ore::data::Trade("CommodityAveragePriceOption") {}

    CommodityAveragePriceOption(
        const ore::data::Envelope& envelope, const ore::data::OptionData& optionData, QuantLib::Real quantity,
        QuantLib::Real strike, const std::string& currency, const std::string& name, CommodityPriceType priceType,
        const std::string& startDate, const std::string& endDate, const std::string& paymentCalendar,
        const std::string& paymentLag, const std::string& paymentConvention, const std::string& pricingCalendar,
        const std::string& paymentDate = "", QuantLib::Real gearing = 1.0, QuantLib::Spread spread = 0.0,
        QuantExt::CommodityQuantityFrequency commodityQuantityFrequency =
            QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
        CommodityPayRelativeTo commodityPayRelativeTo = CommodityPayRelativeTo::CalculationPeriodEndDate,
        QuantLib::Natural futureMonthOffset = 0, QuantLib::Natural deliveryRollDays = 0, bool includePeriodEnd = true,
        const BarrierData& barrierData = {}, const std::string& fxIndex = "");

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) override;

    //! Add underlying Commodity names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! \name Inspectors
    //@{
    const ore::data::OptionData& optionData() { return optionData_; }
    const ore::data::BarrierData& barrierData() { return barrierData_; }
    QuantLib::Real quantity() const { return quantity_; }
    QuantLib::Real strike() const { return strike_; }
    const std::string& currency() const { return currency_; }
    const std::string& name() const { return name_; }
    CommodityPriceType priceType() const { return priceType_; }
    const std::string& startDate() const { return startDate_; }
    const std::string& endDate() const { return endDate_; }
    const std::string& paymentCalendar() const { return paymentCalendar_; }
    const std::string& paymentLag() const { return paymentLag_; }
    const std::string& paymentConvention() const { return paymentConvention_; }
    const std::string& pricingCalendar() const { return pricingCalendar_; }
    const std::string& paymentDate() const { return paymentDate_; }
    QuantLib::Real gearing() const { return gearing_; }
    QuantLib::Spread spread() const { return spread_; }
    QuantExt::CommodityQuantityFrequency commodityQuantityFrequency() const { return commodityQuantityFrequency_; }
    CommodityPayRelativeTo commodityPayRelativeTo() const { return commodityPayRelativeTo_; }
    QuantLib::Natural futureMonthOffset() const { return futureMonthOffset_; }
    QuantLib::Natural deliveryRollDays() const { return deliveryRollDays_; }
    bool includePeriodEnd() const { return includePeriodEnd_; }
    const std::string& fxIndex() const { return fxIndex_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Trade
    //@{
    bool hasCashflows() const override { return false; }
    //@}

private:
    ore::data::OptionData optionData_;
    ore::data::BarrierData barrierData_;
    QuantLib::Real quantity_;
    QuantLib::Real strike_;
    std::string currency_;
    std::string name_;
    CommodityPriceType priceType_;
    std::string startDate_;
    std::string endDate_;
    std::string paymentCalendar_;
    std::string paymentLag_;
    std::string paymentConvention_;
    std::string pricingCalendar_;
    std::string paymentDate_;
    QuantLib::Real gearing_;
    QuantLib::Spread spread_;
    QuantExt::CommodityQuantityFrequency commodityQuantityFrequency_;
    CommodityPayRelativeTo commodityPayRelativeTo_;
    QuantLib::Natural futureMonthOffset_;
    QuantLib::Natural deliveryRollDays_;
    bool includePeriodEnd_;
    std::string fxIndex_;

    /*! Flag indicating if the commodity contract itself is averaging. This is used to decide if we build a
        standard non-averaging commodity option or an averaging commodity option.
    */
    bool allAveraging_;

    /*! Build a commodity floating leg to extract the single commodity averaging flow
     */
    QuantLib::Leg buildLeg(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory,
                           const std::string& configuration);

    //! Build a standard option
    void buildStandardOption(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory, const QuantLib::Leg& leg,
                             QuantLib::Date exerciseDate);

    //! Build an average price option
    void buildApo(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory, const QuantLib::Leg& leg,
                  QuantLib::Date exerciseDate, const QuantLib::ext::shared_ptr<ore::data::EngineBuilder>& builder);
};

} // namespace data
} // namespace ore
