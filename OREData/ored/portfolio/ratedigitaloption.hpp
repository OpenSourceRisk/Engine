/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/ratedigitaloption.hpp
    \brief European digital (binary) option on an interest rate fixing
    \ingroup portfolio

    This trade type is designed for portfolio-replication validation of
    range accruals: a range accrual coupon can be decomposed into a sum
    of RateDigitalOptions, one per observation date, whose aggregate NPV
    should match the range accrual leg NPV priced by RangeAccrualPricerByBgm.

    XML structure:
    \code
    <RateDigitalOptionData>
      <OptionData>
        <LongShort>Long</LongShort>
        <OptionType>Call</OptionType>
        <ExerciseDates>
          <ExerciseDate>2027-06-20</ExerciseDate>
        </ExerciseDates>
      </OptionData>
      <Index>EUR-EURIBOR-6M</Index>
      <Strike>0.02</Strike>
      <PayoffAmount>10000.0</PayoffAmount>
      <PayoffCurrency>EUR</PayoffCurrency>
      <FixingDate>2027-06-18</FixingDate>
      <PaymentDate>2027-06-20</PaymentDate>
    </RateDigitalOptionData>
    \endcode
*/

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable European digital (cash-or-nothing) option on an interest rate fixing
/*!
  Pays a fixed cash amount if the rate fixing on FixingDate is above (Call)
  or below (Put) the Strike.  Uses the optionlet (cap/floor) volatility
  surface so that replication of a RangeAccrual leg is consistent.
  Supports both IBOR and OIS (e.g. SOFR, ESTR) indices.

  \ingroup tradedata
*/
class RateDigitalOption : public ore::data::Trade {
public:
    //! Default constructor
    RateDigitalOption() : ore::data::Trade("RateDigitalOption") {}

    //! Constructor
    RateDigitalOption(const Envelope& env, const OptionData& option, const std::string& index,
                      QuantLib::Real strike, QuantLib::Real payoffAmount,
                      const std::string& payoffCurrency, const std::string& fixingDate,
                      const std::string& paymentDate)
        : ore::data::Trade("RateDigitalOption", env), option_(option), index_(index),
          strike_(strike), payoffAmount_(payoffAmount), payoffCurrency_(payoffCurrency),
          fixingDate_(fixingDate), paymentDate_(paymentDate) {}

    //! Build QuantLib/QuantExt instrument and link pricing engine
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const OptionData& option() const { return option_; }
    const std::string& indexName() const { return index_; }
    QuantLib::Real strike() const { return strike_; }
    QuantLib::Real payoffAmount() const { return payoffAmount_; }
    const std::string& payoffCurrency() const { return payoffCurrency_; }
    const std::string& fixingDate() const { return fixingDate_; }
    const std::string& paymentDate() const { return paymentDate_; }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    OptionData option_;
    std::string index_;
    QuantLib::Real strike_ = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real payoffAmount_ = QuantLib::Null<QuantLib::Real>();
    std::string payoffCurrency_;
    std::string fixingDate_;
    std::string paymentDate_;
};

} // namespace data
} // namespace ore
