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

/*! \file ored/portfolio/compositetrade.hpp
    \brief Composite trades operate as a mini portfolio.
    Their intended use is for strategies like straddles.
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradefactory.hpp>
#include <ostream>

namespace ore {
namespace data {
using QuantLib::Date;
using std::string;

//! Composite Trade class
/*! CompositeTrades are single currency strategies consisting of independent financial instruments but regarded as a
 single position n the portfolio. Examples include straddles, butterflies, iron condors. The class can also be used to
 create representations of single contracts which can be replicated by linear combinations of other positions. E.g. Bond
 + Bond Option = Callable Bond. \ingroup portfolio
*/
class CompositeTrade : public Trade {
public:
    /// This enum decalres how the notional of the CompositeTrade should be calculated
    enum class NotionalCalculation {
        Sum,     ///< The notional is calculated as the sum of notionals of subtrades
        Mean,    ///< The notional is calculated as the average of notionals of subtrades
        First,   ///< The notional is taken as the first subtrade notional
        Last,    ///< The notional is taken as the last subtrade notional
        Min,     ///< The notional is taken as the minimum subtrade notional
        Max,     ///< The notional is taken as the maximum subtrade notional
        Override ///< The notional is explicitly overridden
    };

    //! Constructor requires a trade factory so that subtrades can be built
    CompositeTrade(const boost::weak_ptr<TradeFactory>& tf, const Envelope& env = Envelope(),
                   const TradeActions& ta = TradeActions())
        : Trade("CompositeTrade", env, ta), tradeFactory_(tf) {
        reset();
    }

    //! Fully-specified Constructor
    CompositeTrade(const string currency, const vector<boost::shared_ptr<Trade>>& trades,
                   const string notionalCalculation = "", const Real notionalOverride = 0.0,
                   const Envelope& env = Envelope(), const TradeActions& ta = TradeActions(),
                   const boost::weak_ptr<TradeFactory>& tf = boost::make_shared<TradeFactory>())
        : Trade("CompositeTrade", env, ta), tradeFactory_(tf), currency_(currency), notionalOverride_(notionalOverride),
          notionalCalculation_(notionalCalculation), trades_(trades) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    virtual void build(const boost::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;

    //! \name Inspectors
    //@{
    const boost::weak_ptr<TradeFactory>& tradeFactory() const { return tradeFactory_; }
    const string& currency() const { return currency_; }
    const string& notionalCalculation() const { return notionalCalculation_; }
    const vector<boost::shared_ptr<Trade>>& trades() const { return trades_; }
    //@}

    //! \name Utility functions
    //@{
    //! returns the number of subtrades in the strategy
    Size size() const { return trades_.size(); }

    //! calculates the CompositeTrade notional, when supplied with the notionals of the subtrades
    Real calculateNotional(const vector<Real>& tradeNotionals) const;
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name trade overrides
    //@{
    std::map<std::string, std::set<QuantLib::Date>> fixings(const QuantLib::Date& settlementDate) const override;
    std::map<AssetClass, std::set<std::string>> underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) const override;;
    const std::map<std::string,boost::any>& additionalData() const override;
    //@}

private:
    const boost::weak_ptr<TradeFactory>& tradeFactory_;
    string currency_;
    Real notionalOverride_;
    string notionalCalculation_;
    vector<boost::shared_ptr<Trade>> trades_;
    vector<Handle<Quote>> fxRates_, fxRatesNotional_;
};

} // namespace data
} // namespace oreplus
