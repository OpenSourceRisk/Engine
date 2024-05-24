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

/*! \file ored/portfolio/cliquetoption.hpp
    \brief Equity Cliquet Option
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {

//! Serializable Equity Cliquet Option
/*!
    \ingroup tradedata
*/
class CliquetOption : public ore::data::Trade {
public:
    //! Default constructor
    CliquetOption(const std::string& tradeType) : ore::data::Trade(tradeType) {}
    //! Constructor
    CliquetOption(const std::string& tradeType, ore::data::Envelope& env,
                  const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying, std::string currency,
                  QuantLib::Real notional, std::string longShort, std::string callPut,
                  ore::data::ScheduleData scheduleData, QuantLib::Real moneyness = 1.0,
                  QuantLib::Real localCap = QuantLib::Null<QuantLib::Real>(),
                  QuantLib::Real localFloor = QuantLib::Null<QuantLib::Real>(),
                  QuantLib::Real globalCap = QuantLib::Null<QuantLib::Real>(),
                  QuantLib::Real globalFloor = QuantLib::Null<QuantLib::Real>(), QuantLib::Size settlementDays = 0,
                  QuantLib::Real premium = 0.0, std::string premiumCcy = "", std::string premiumPayDate = "")
        : ore::data::Trade(tradeType, env), underlying_(underlying), currency_(currency), longShort_(longShort),
          callPut_(callPut), scheduleData_(scheduleData), moneyness_(moneyness), localCap_(localCap),
          localFloor_(localFloor), globalCap_(globalCap), globalFloor_(globalFloor), settlementDays_(settlementDays),
          premium_(premium), premiumCcy_(premiumCcy), premiumPayDate_(premiumPayDate) {
        notional_ = notional;
    }

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const std::string& name() const { return underlying_->name(); }
    const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying() const { return underlying_; }
    const std::string& currency() const { return currency_; }
    const std::string& longShort() const { return longShort_; }
    const std::string& callPut() const { return callPut_; }
    const ore::data::ScheduleData& scheduleData() const { return scheduleData_; }
    const QuantLib::Real& moneyness() const { return moneyness_; }
    const QuantLib::Real& localCap() const { return localCap_; }
    const QuantLib::Real& localFloor() const { return localFloor_; }
    const QuantLib::Real& globalCap() const { return globalCap_; }
    const QuantLib::Real& globalFloor() const { return globalFloor_; }
    QuantLib::Size settlementDays() const { return settlementDays_; }
    QuantLib::Real premium() const { return premium_; }
    const std::string& premiumCcy() const { return premiumCcy_; }
    const std::string& premiumPayDate() const { return premiumPayDate_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    QuantLib::ext::shared_ptr<ore::data::Underlying> underlying_;
    std::string currency_;
    QuantLib::Real cliquetNotional_;
    std::set<QuantLib::Date> valuationDates_;
    std::string longShort_; // long or short
    std::string callPut_;   // call or put
    ore::data::ScheduleData scheduleData_;
    QuantLib::Real moneyness_; // moneyness value
    QuantLib::Real localCap_;
    QuantLib::Real localFloor_;
    QuantLib::Real globalCap_;
    QuantLib::Real globalFloor_;
    QuantLib::Size settlementDays_;
    QuantLib::Real premium_;
    std::string premiumCcy_;
    std::string premiumPayDate_;
};

class EquityCliquetOption : public CliquetOption {
public:
    EquityCliquetOption() : CliquetOption("EquityCliquetOption") {}
    EquityCliquetOption(const std::string& tradeType, ore::data::Envelope& env,
                        const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying, std::string currency,
                        QuantLib::Real notional, std::string longShort, std::string callPut,
                        ore::data::ScheduleData scheduleData, QuantLib::Real moneyness = 1.0,
                        QuantLib::Real localCap = QuantLib::Null<QuantLib::Real>(),
                        QuantLib::Real localFloor = QuantLib::Null<QuantLib::Real>(),
                        QuantLib::Real globalCap = QuantLib::Null<QuantLib::Real>(),
                        QuantLib::Real globalFloor = QuantLib::Null<QuantLib::Real>(),
                        QuantLib::Size settlementDays = 0, QuantLib::Real premium = 0.0, std::string premiumCcy = "",
                        std::string premiumPayDate = "")
        : CliquetOption(tradeType, env, underlying, currency, notional, longShort, callPut, scheduleData, moneyness,
                        localCap, localFloor, globalCap, globalFloor, settlementDays, premium, premiumCcy,
                        premiumPayDate) {}

    //! Add underlying Equity names
    std::map<ore::data::AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceDataManager) const override {
        return {{ore::data::AssetClass::EQ, std::set<std::string>({name()})}};
    };
};

} // namespace data
} // namespace ore
