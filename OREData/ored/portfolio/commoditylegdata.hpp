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

/*! \file ored/portfolio/commoditylegdata.hpp
    \brief leg data for commodity leg types
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <qle/cashflows/commoditycashflow.hpp>
#include <qle/indexes/commodityindex.hpp>

namespace ore {
namespace data {

enum class CommodityPayRelativeTo {
    CalculationPeriodEndDate,
    CalculationPeriodStartDate,
    TerminationDate,
    FutureExpiryDate
};
CommodityPayRelativeTo parseCommodityPayRelativeTo(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CommodityPayRelativeTo& cprt);

enum class CommodityPriceType { Spot, FutureSettlement };
CommodityPriceType parseCommodityPriceType(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CommodityPriceType& cpt);

enum class CommodityPricingDateRule { FutureExpiryDate, None };
CommodityPricingDateRule parseCommodityPricingDateRule(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CommodityPricingDateRule& cpdr);

class CommodityFixedLegData : public ore::data::LegAdditionalData {

public:
    //! Default constructor
    CommodityFixedLegData();

    //! Detailed constructor
    CommodityFixedLegData(const std::vector<QuantLib::Real>& quantities, const std::vector<std::string>& quantityDates,
                          const std::vector<QuantLib::Real>& prices, const std::vector<std::string>& priceDates,
                          CommodityPayRelativeTo commodityPayRelativeTo, const std::string& tag = "");

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Real>& quantities() const { return quantities_; }
    const std::vector<std::string>& quantityDates() const { return quantityDates_; }
    const std::vector<QuantLib::Real>& prices() const { return prices_; }
    const std::vector<std::string>& priceDates() const { return priceDates_; }
    CommodityPayRelativeTo commodityPayRelativeTo() const { return commodityPayRelativeTo_; }
    const std::string& tag() const { return tag_; }
    //@}

    /*! \brief Set the fixed leg data quantities.

        For commodity swaps, there can be a number of conventions provided with the floating leg data quantities that 
        when taken together can be used to calculate the commodity quantity for the full calculation period. Instead 
        of duplicating that data here, we allow the fixed leg data quantities to be set using this method before being 
        passed to the commodity fixed leg builder. The idea is that the quantities will be set by referencing the 
        quantities from the corresponding floating leg of the swap after that leg has been built.
    */
    void setQuantities(const std::vector<QuantLib::Real>& quantities);

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::vector<QuantLib::Real> quantities_;
    std::vector<std::string> quantityDates_;
    std::vector<QuantLib::Real> prices_;
    std::vector<std::string> priceDates_;
    CommodityPayRelativeTo commodityPayRelativeTo_;
    std::string tag_;
};

class CommodityFloatingLegData : public ore::data::LegAdditionalData {

public:
    //! Default constructor
    CommodityFloatingLegData();

    //! Constructor
    CommodityFloatingLegData(
        const std::string& name, CommodityPriceType priceType, const std::vector<QuantLib::Real>& quantities,
        const std::vector<std::string>& quantityDates,
        QuantExt::CommodityQuantityFrequency commodityQuantityFrequency =
            QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
        CommodityPayRelativeTo commodityPayRelativeTo = CommodityPayRelativeTo::CalculationPeriodEndDate,
        const std::vector<QuantLib::Real>& spreads = {}, const std::vector<std::string>& spreadDates = {},
        const std::vector<QuantLib::Real>& gearings = {}, const std::vector<std::string>& gearingDates = {},
        CommodityPricingDateRule pricingDateRule = CommodityPricingDateRule::FutureExpiryDate,
        const std::string& pricingCalendar = "", QuantLib::Natural pricingLag = 0,
        const std::vector<std::string>& pricingDates = {}, bool isAveraged = false, bool isInArrears = true,
        QuantLib::Natural futureMonthOffset = 0, QuantLib::Natural deliveryRollDays = 0, bool includePeriodEnd = true,
        bool excludePeriodStart = true, QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
        bool useBusinessDays = true, const std::string& tag = "", QuantLib::Natural dailyExpiryOffset =
        QuantLib::Null<QuantLib::Natural>(), bool unrealisedQuantity = false,
        QuantLib::Natural lastNDays = QuantLib::Null<QuantLib::Natural>(), std::string fxIndex = "");

    //! \name Inspectors
    //@{
    const std::string& name() const { return name_; }
    CommodityPriceType priceType() const { return priceType_; }
    const std::vector<QuantLib::Real>& quantities() const { return quantities_; }
    const std::vector<std::string>& quantityDates() const { return quantityDates_; }
    QuantExt::CommodityQuantityFrequency commodityQuantityFrequency() const { return commodityQuantityFrequency_; }
    CommodityPayRelativeTo commodityPayRelativeTo() const { return commodityPayRelativeTo_; }
    const std::vector<QuantLib::Real>& spreads() const { return spreads_; }
    const std::vector<std::string>& spreadDates() const { return spreadDates_; }
    const std::vector<QuantLib::Real>& gearings() const { return gearings_; }
    const std::vector<std::string>& gearingDates() const { return gearingDates_; }
    CommodityPricingDateRule pricingDateRule() const { return pricingDateRule_; }
    const std::string& pricingCalendar() const { return pricingCalendar_; }
    QuantLib::Natural pricingLag() const { return pricingLag_; }
    const std::vector<std::string>& pricingDates() const { return pricingDates_; }
    bool isAveraged() const { return isAveraged_; }
    bool isInArrears() const { return isInArrears_; }
    QuantLib::Natural futureMonthOffset() const { return futureMonthOffset_; }
    QuantLib::Natural deliveryRollDays() const { return deliveryRollDays_; }
    bool includePeriodEnd() const { return includePeriodEnd_; }
    bool excludePeriodStart() const { return excludePeriodStart_; }
    QuantLib::Natural hoursPerDay() const { return hoursPerDay_; }
    bool useBusinessDays() const { return useBusinessDays_; }
    const std::string& tag() const { return tag_; }
    QuantLib::Natural dailyExpiryOffset() const { return dailyExpiryOffset_; }
    bool unrealisedQuantity() const { return unrealisedQuantity_; }
    QuantLib::Natural lastNDays() const { return lastNDays_; }
    std::string const& fxIndex() const { return fxIndex_; }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string name_;
    CommodityPriceType priceType_;
    std::vector<QuantLib::Real> quantities_;
    std::vector<std::string> quantityDates_;
    QuantExt::CommodityQuantityFrequency commodityQuantityFrequency_;
    CommodityPayRelativeTo commodityPayRelativeTo_;
    std::vector<QuantLib::Real> spreads_;
    std::vector<std::string> spreadDates_;
    std::vector<QuantLib::Real> gearings_;
    std::vector<std::string> gearingDates_;
    CommodityPricingDateRule pricingDateRule_;
    std::string pricingCalendar_;
    QuantLib::Natural pricingLag_;
    std::vector<std::string> pricingDates_;
    bool isAveraged_;
    bool isInArrears_;
    QuantLib::Natural futureMonthOffset_;
    QuantLib::Natural deliveryRollDays_;
    bool includePeriodEnd_;
    bool excludePeriodStart_;
    QuantLib::Natural hoursPerDay_;
    bool useBusinessDays_;
    std::string tag_;
    QuantLib::Natural dailyExpiryOffset_;
    bool unrealisedQuantity_;
    QuantLib::Natural lastNDays_;
    std::string fxIndex_;
};

} // namespace data
} // namespace ore
