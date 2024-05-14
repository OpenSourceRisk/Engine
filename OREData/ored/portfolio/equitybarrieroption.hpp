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

/*! \file ored/portfolio/equitybarrieroption.hpp
   \brief Equity Barrier Option data model and serialization
   \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/barrierdata.hpp>
#include <ored/portfolio/barrieroption.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

#include <ql/instruments/barriertype.hpp>

namespace ore {
namespace data {
using std::string;

//! Serializable EQ Barrier Option
/*!
  \ingroup tradedata
*/
class EquityBarrierOption : public EquityOptionWithBarrier {
public:
    //! Default constructor
    EquityBarrierOption() : ore::data::Trade("EquityBarrierOption"), EquityOptionWithBarrier("") {}
    //! Constructor
    EquityBarrierOption(Envelope& env, OptionData option, BarrierData barrier, QuantLib::Date startDate,
                        std::string calendar, EquityUnderlying equityUnderlying, QuantLib::Currency currency,
                        QuantLib::Real quantity,TradeStrike strike)
        : ore::data::Trade("EquityBarrierOption", env),
          EquityOptionWithBarrier("",  env, option, barrier, startDate, calendar, equityUnderlying, currency,
                                  quantity, strike) {}

    void checkBarriers() override;

    //! create the pricing engines
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    vanillaPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    barrierPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                           const QuantLib::Date& paymentDate = QuantLib::Date()) override;
};

} // namespace data
} // namespace oreplus
