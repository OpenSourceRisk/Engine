/*
  Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/equityeuropeanbarrieroption.hpp
   \brief EQ European Barrier Option data model and serialization
   \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/barrierdata.hpp>

#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ql/instruments/barriertype.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable EQ European Barrier Option
/*!
  \ingroup tradedata
*/
class EquityEuropeanBarrierOption : public ore::data::EquityOption {
public:
    //! Default constructor
    EquityEuropeanBarrierOption() : EquityOption() {}
    //! Constructor
    EquityEuropeanBarrierOption(Envelope& env, OptionData option, BarrierData barrier,
                                EquityUnderlying equityUnderlying, string currency, TradeStrike strike, double quantity)
        : EquityOption(env, option, equityUnderlying, currency, quantity, strike), barrier_(barrier) {}

    //! Build QuantLib/QuantExt instrument, link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const BarrierData& barrier() const { return barrier_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    BarrierData barrier_;
};
} // namespace data
} // namespace oreplus
