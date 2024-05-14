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

/*! \file ored/portfolio/commodityoptionstrip.hpp
    \brief Commodity option strip data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/commoditylegdata.hpp>
#include <ql/position.hpp>

namespace ore {
namespace data {

/*! Serializable Commodity option strip
    \ingroup tradedata
*/
class CommodityOptionStrip : public ore::data::Trade {
public:
    CommodityOptionStrip() : ore::data::Trade("CommodityOptionStrip") {}

    CommodityOptionStrip(const ore::data::Envelope& envelope, const ore::data::LegData& legData,
                         const std::vector<QuantLib::Position::Type>& callPositions,
                         const std::vector<QuantLib::Real>& callStrikes,
                         const std::vector<QuantLib::Position::Type>& putPositions,
                         const std::vector<QuantLib::Real>& putStrikes, QuantLib::Real premium = 0.0,
                         const std::string& premiumCurrency = "",
                         const QuantLib::Date& premiumPayDate = QuantLib::Date(), const std::string& style = "",
                         const std::string& settlement = "", const BarrierData& callBarrierData = {},
                         const BarrierData& putBarrierData = {},
                         const std::string& fxIndex = "", 
                         const bool isDigital = false, 
                         Real payoffPerUnit = 0.0);

    //! Implement the build method
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) override;

    //! Add underlying Commodity names
    std::map<ore::data::AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! \name Inspectors
    //@{
    const ore::data::LegData& legData() const { return legData_; }
    const std::vector<QuantLib::Position::Type>& callPositions() const { return callPositions_; }
    const std::vector<QuantLib::Real>& callStrikes() const { return callStrikes_; }
    const std::vector<QuantLib::Position::Type>& putPositions() const { return putPositions_; }
    const std::vector<QuantLib::Real>& putStrikes() const { return putStrikes_; }
    const PremiumData& premiumDate() const { return premiumData_; }
    const std::string& style() const { return style_; }
    const std::string& settlement() const { return settlement_; }
    const std::string& fxIndex() const { return fxIndex_; }
    const BarrierData& callBarrierData() const { return callBarrierData_; }
    const BarrierData& putBarrierData() const { return putBarrierData_; }
    const bool isDigital() const { return isDigital_; }
    const Real payoffPerUnit() const { return unaryPayoff_; }
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
    ore::data::LegData legData_;
    std::vector<QuantLib::Position::Type> callPositions_;
    std::vector<QuantLib::Real> callStrikes_;
    std::vector<QuantLib::Position::Type> putPositions_;
    std::vector<QuantLib::Real> putStrikes_;
    PremiumData premiumData_;
    std::string style_;
    std::string settlement_;
    BarrierData callBarrierData_;
    BarrierData putBarrierData_;
    std::string fxIndex_;
    bool isDigital_;
    Real unaryPayoff_;

    QuantLib::ext::shared_ptr<CommodityFloatingLegData> commLegData_;

    //! Build an average price option strip
    void buildAPOs(const QuantLib::Leg& leg, const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory);

    //! Build a standard option strip
    void buildStandardOptions(const QuantLib::Leg& leg,
                              const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory);

    //! Perform checks before building
    void check(QuantLib::Size numberPeriods) const;
};

} // namespace data
} // namespace ore
