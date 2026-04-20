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

/*! \file portfolio/equityautodeltahedgeoption.hpp
    \brief Equity Auto Delta Hedged Option data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradestrike.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {

//! Data for a single underlying option within the delta-hedged basket
struct UnderlyingOptionData {
    OptionData optionData;
    EquityUnderlying equityUnderlying;
    std::string currency;
    TradeStrike strike;
    std::string strikeCurrency;
    double quantity = 0.0;
};

//! Serializable Equity Auto Delta Hedged Option
/*!
  An EquityAutoDeltaHedgedOption consists of N batches of vanilla equity options.
  The Equity Amount is computed as:

    EqAmount = Sum_i [ DeltaHedge_Final^i + Premium^i - Option_Final^i ] x N^i

  where:
    - DeltaHedge_Final^i is the cumulative delta-hedge P&L for batch i,
      evolved as DeltaHedge_t = DeltaHedge_{t-1} + delta_{t-1} * (Fwd_t - Fwd_{t-1}),
      with delta_t = N(d1) computed using the specified hedging volatility.
    - Premium^i is the option premium for batch i.
    - Option_Final^i = max(0, S_Final - K^i) is the option payoff.
    - N^i is the number of underlying options in batch i.

  \ingroup tradedata
*/
class EquityAutoDeltaHedgedOption : public Trade {
public:
    //! Default constructor
    EquityAutoDeltaHedgedOption() : Trade("EquityAutoDeltaHedgedOption") {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! Add underlying Equity names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! \name Inspectors
    //@{
    const std::vector<UnderlyingOptionData>& underlyings() const { return underlyings_; }
    double hedgingVolatility() const { return hedgingVol_; }
    const QuantLib::Date& observationStartDate() const { return observationStartDate_; }
    double driftRate() const { return driftRate_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    std::vector<UnderlyingOptionData> underlyings_;
    double hedgingVol_ = 0.0;
    double driftRate_ = 0.0;
    QuantLib::Date observationStartDate_;
};

} // namespace data
} // namespace ore
