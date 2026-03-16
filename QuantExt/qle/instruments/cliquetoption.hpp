/*
  Copyright (C) 2019 Quaternion Risk Management Ltd.
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

/*! \file qle/instruments/cliquetoption.hpp
    \brief Cliquet option
*/

#ifndef quantext_cliquet_option_hpp
#define quantext_cliquet_option_hpp

#include <ql/instruments/oneassetoption.hpp>
#include <ql/instruments/payoffs.hpp>

#include <ql/exercise.hpp>
#include <ql/option.hpp>
#include <ql/position.hpp>
#include <set>

namespace QuantExt {
using namespace QuantLib;

/*! The payoff on the payment date is given by
    min( max( sum min( max( S(t) / S(t-1) - moneyness, localFloor ), localCap ), globalFloor), globalCap)
*/
class CliquetOption : public OneAssetOption {
public:
    class arguments;
    class engine;
    CliquetOption(const QuantLib::ext::shared_ptr<PercentageStrikePayoff>& payoff,
                  const QuantLib::ext::shared_ptr<EuropeanExercise>& maturity, const std::set<Date>& valuationDates,
                  const Date& paymentDate, const Real notional, const Position::Type& longShort,
                  const Real localCap = Null<Real>(), const Real localFloor = Null<Real>(),
                  const Real globalCap = Null<Real>(), const Real globalFloor = Null<Real>(),
                  const Real premium = Null<Real>(), const Date& premiumPayDate = Date(),
                  const std::string& premiumCurrency = "");
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    //
    const std::set<Date>& valuationDates() const { return valuationDates_; }

private:
    const std::set<Date> valuationDates_;
    const Date paymentDate_;
    const Real notional_;
    const Position::Type longShort_;
    const Real localCap_, localFloor_, globalCap_, globalFloor_, premium_;
    const Date premiumPayDate_;
    const std::string premiumCurrency_;
};

class CliquetOption::arguments : public OneAssetOption::arguments {
public:
    arguments()
        : valuationDates(std::set<Date>()), paymentDate(Date()), localCap(Null<Real>()), localFloor(Null<Real>()),
          globalCap(Null<Real>()), globalFloor(Null<Real>()), premium(Null<Real>()), premiumPayDate(Date()) {}

    void validate() const override;

    Option::Type type;
    Position::Type longShort;
    Real notional, moneyness;
    std::set<Date> valuationDates;
    Date paymentDate;
    Real localCap, localFloor, globalCap, globalFloor, premium;
    Date premiumPayDate;
    std::string premiumCurrency;
};

//! Cliquet %engine base class
class CliquetOption::engine : public GenericEngine<CliquetOption::arguments, CliquetOption::results> {};

} // namespace QuantExt

#endif
