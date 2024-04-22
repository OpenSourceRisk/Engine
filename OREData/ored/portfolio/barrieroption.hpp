/*
  Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/barrieroption.hpp
   \brief Barrier Option data model and serialization
   \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/fxderivative.hpp>
#include <ored/portfolio/equityderivative.hpp>
#include <ored/portfolio/tradestrike.hpp>

#include <ql/instruments/barriertype.hpp>

namespace ore {
namespace data {

//! Serializable FX Barrier Option
/*!
  \ingroup tradedata
*/
class BarrierOption : virtual public ore::data::Trade {
public:
    //! Constructor
    BarrierOption() {}

    BarrierOption(ore::data::OptionData option, BarrierData barrier, QuantLib::Date startDate = QuantLib::Date(),
                  const std::string& calendar = std::string())
        : option_(option), barrier_(barrier), startDate_(startDate), calendarStr_(calendar) {
        calendar_ = parseCalendar(calendarStr_);
    }

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    
    //! check validity of barriers
    virtual void checkBarriers() = 0;

    // index of the underlying
    virtual QuantLib::ext::shared_ptr<QuantLib::Index> getIndex() = 0;

    // strike of underlying option
    virtual const QuantLib::Real strike() = 0;

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

    //! \name Inspectors
    //@{
    const ore::data::OptionData& option() const { return option_; }
    const BarrierData& barrier() const { return barrier_; }
    const QuantLib::Date& startDate() const { return startDate_; }
    const QuantLib::Calendar& calendar() const { return calendar_; }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    ore::data::OptionData option_;
    BarrierData barrier_;
    QuantLib::Date startDate_;
    QuantLib::Calendar calendar_;

protected:
    std::string calendarStr_;
};

class FxOptionWithBarrier : public FxSingleAssetDerivative, public BarrierOption {
public:
    //! Default constructor
    FxOptionWithBarrier(const std::string& tradeType) : ore::data::Trade(tradeType), FxSingleAssetDerivative(tradeType), 
        boughtAmount_(0.0), soldAmount_(0.0) {}

    //! Constructor
    FxOptionWithBarrier(const std::string& tradeType, ore::data::Envelope& env, ore::data::OptionData option,
        BarrierData barrier, QuantLib::Date startDate, std::string calendar, std::string boughtCurrency,
        QuantLib::Real boughtAmount, std::string soldCurrency, QuantLib::Real soldAmount, 
        std::string fxIndex = std::string())
        : ore::data::Trade(tradeType, env), 
          FxSingleAssetDerivative(tradeType, env, boughtCurrency, soldCurrency),
          BarrierOption(option, barrier, startDate, calendar), 
          fxIndexStr_(fxIndex), boughtAmount_(boughtAmount), soldAmount_(soldAmount) {}

    //! \name Inspectors
    //@{
    QuantLib::Real boughtAmount() const { return boughtAmount_; }
    QuantLib::Real soldAmount() const { return soldAmount_; }
    //@}

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& ef) override;
    //! \name Serialisation
    //@{
    void additionalFromXml(ore::data::XMLNode* node) override;
    void additionalToXml(ore::data::XMLDocument& doc, ore::data::XMLNode* node) const override;
    //@}

    QuantLib::ext::shared_ptr<QuantLib::Index> getIndex() override { return QuantLib::ext::dynamic_pointer_cast<Index>(fxIndex_); }
    const QuantLib::Real strike() override { return soldAmount_ / boughtAmount_; }
    QuantLib::Real tradeMultiplier() override { return boughtAmount_; }
    Currency tradeCurrency() override { return parseCurrency(soldCurrency_); }
    const QuantLib::Handle<QuantLib::Quote>& spotQuote() override { return spotQuote_; }
    std::string indexFixingName() override;

    void fromXML(ore::data::XMLNode* node) override { BarrierOption::fromXML(node); }
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override { return BarrierOption::toXML(doc); }

private:
    std::string fxIndexStr_;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex_;
    QuantLib::Handle<QuantLib::Quote> spotQuote_;
    QuantLib::Real boughtAmount_;
    QuantLib::Real soldAmount_;
};


class EquityOptionWithBarrier : public EquitySingleAssetDerivative, public BarrierOption {
public:
    //! Default constructor
    EquityOptionWithBarrier(const std::string& tradeType)
        : ore::data::Trade(tradeType), EquitySingleAssetDerivative(tradeType), quantity_(0.0) {}

    //! Constructor
    EquityOptionWithBarrier(const std::string& tradeType, ore::data::Envelope& env, ore::data::OptionData option,
        BarrierData barrier, QuantLib::Date startDate, std::string calendar, const EquityUnderlying& equity,
        QuantLib::Currency currency, QuantLib::Real quantity, TradeStrike strike)
        : ore::data::Trade(tradeType, env), EquitySingleAssetDerivative(tradeType, env, equity),
          BarrierOption(option, barrier, startDate, calendar), currency_(currency), quantity_(quantity),
           tradeStrike_(strike) {}

    //! \name Inspectors
    //@{
    QuantLib::Real quantity() const { return quantity_; }
    //@}

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& ef) override;
    //! \name Serialisation
    //@{
    void additionalFromXml(ore::data::XMLNode* node) override;
    void additionalToXml(ore::data::XMLDocument& doc, ore::data::XMLNode* node) const override;
    //@}

    QuantLib::ext::shared_ptr<QuantLib::Index> getIndex() override { return QuantLib::ext::dynamic_pointer_cast<Index>(eqIndex_); }
    const QuantLib::Real strike() override { return tradeStrike_.value(); }
    QuantLib::Real tradeMultiplier() override { return quantity_; }
    Currency tradeCurrency() override { return currency_; }
    const QuantLib::Handle<QuantLib::Quote>& spotQuote() override { return eqIndex_->equitySpot(); }
    std::string indexFixingName() override { return "EQ-" + eqIndex_->name(); };

    void fromXML(ore::data::XMLNode* node) override { BarrierOption::fromXML(node); }
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override { return BarrierOption::toXML(doc); }

private:
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> eqIndex_;
    QuantLib::Currency currency_;
    std::string currencyStr_;
    QuantLib::Real quantity_;
    TradeStrike tradeStrike_;
};

} // namespace data
} // namespace oreplus
