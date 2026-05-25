/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/vanillaoption.hpp
    \brief Vanilla Option data model
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradestrike.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable Vanilla Option
/*!
  \ingroup tradedata
*/
class VanillaOptionTrade : public Trade {
public:
    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void setNotionalAndCurrencies();

    //! Trade interface
    QuantLib::Real notional(NotionalType type = NotionalType::Default) const override;
    string notionalCurrency() const override;

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    const string& asset() const { return assetName_; }
    const string& currency() const { return currency_; }
    TradeStrike strike() const { return strike_; }
    double quantity() const { return quantity_; }
    const QuantLib::Date forwardDate() const { return forwardDate_; }
    const QuantLib::Date paymentDate() const { return paymentDate_; }
    //@}

protected:
    VanillaOptionTrade(std::string tradeType, AssetClass assetClassUnderlying)
        : Trade(std::move(tradeType)), assetClassUnderlying_(std::move(assetClassUnderlying)), quantity_(0) {}
    VanillaOptionTrade(std::string tradeType, const Envelope& env, AssetClass assetClassUnderlying, OptionData option,
                       string assetName, string currency, double quantity, TradeStrike strike,
                       const QuantLib::ext::shared_ptr<QuantLib::Index>& index = nullptr,
                       const std::string& indexName = "",
                       QuantLib::Date forwardDate = QuantLib::Date())
        : Trade(std::move(tradeType), env), assetClassUnderlying_(std::move(assetClassUnderlying)),
            option_(std::move(option)), assetName_(std::move(assetName)), currency_(std::move(currency)),
            quantity_(quantity), strike_(std::move(strike)), index_(index), indexName_(indexName),
            forwardDate_(std::move(forwardDate)) {}

    std::pair<QuantLib::Exercise::Type, QuantLib::ext::shared_ptr<QuantLib::Exercise>> exerciseDetails();

    std::pair<QuantLib::Option::Type, QuantLib::ext::shared_ptr<QuantLib::StrikedTypePayoff>> payoffDetails() const;

    void setInstrumentWrapper(const QuantLib::ext::shared_ptr<Instrument>& mainInstrument,
        const std::string& discountCurve, const QuantLib::Currency& npvCurrency,
        const std::string& configuration, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory);

    AssetClass assetClassUnderlying_;
    OptionData option_;
    string assetName_;
    string currency_;
    Currency underlyingCurrency_;
    double quantity_;
    TradeStrike strike_;

    //! An index is needed if the option is to be automatically exercised on expiry.
    QuantLib::ext::shared_ptr<QuantLib::Index> index_;

    //! Hold the external index name if needed e.g. in the case of an FX index.
    std::string indexName_;

    //! Store the option expiry date.
    QuantLib::Date expiryDate_;

    //! Store the (optional) forward date.
    QuantLib::Date forwardDate_;

    //! Store the (optional) payment date.
    QuantLib::Date paymentDate_;

    //! Delegating engine builder
    QuantLib::ext::shared_ptr<Trade> delegatingBuilderTrade_;
};
} // namespace data
} // namespace ore
