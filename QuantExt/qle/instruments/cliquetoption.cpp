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

#include <qle/instruments/cliquetoption.hpp>
#include <ql/settings.hpp>

using namespace QuantLib;

namespace QuantExt {

CliquetOption::CliquetOption(const QuantLib::ext::shared_ptr<PercentageStrikePayoff>& payoff,
                             const QuantLib::ext::shared_ptr<EuropeanExercise>& maturity, const std::set<Date>& valuationDates,
                             const Date& paymentDate, const Real notional, const Position::Type& longShort,
                             const Real localCap, const Real localFloor, const Real globalCap, const Real globalFloor,
                             const Real premium, const Date& premiumPayDate, const std::string& premiumCurrency)
    : OneAssetOption(payoff, maturity), valuationDates_(valuationDates), paymentDate_(paymentDate), notional_(notional),
      longShort_(longShort), localCap_(localCap), localFloor_(localFloor), globalCap_(globalCap),
      globalFloor_(globalFloor), premium_(premium), premiumPayDate_(premiumPayDate), premiumCurrency_(premiumCurrency) {

    QL_REQUIRE(valuationDates.size() > 0, "cliquet option: at least one valuation date must be given");
    QL_REQUIRE(paymentDate >= *valuationDates.rbegin(), "cliquet option: payment date ("
                                                            << paymentDate << ") must be after last valuation date ("
                                                            << *valuationDates.rbegin() << ")");
}

bool CliquetOption::isExpired() const { return Settings::instance().evaluationDate() >= paymentDate_; }

void CliquetOption::setupArguments(PricingEngine::arguments* args) const {
    OneAssetOption::setupArguments(args);
    CliquetOption::arguments* moreArgs = dynamic_cast<CliquetOption::arguments*>(args);
    QL_REQUIRE(moreArgs != 0, "cliquet option: wrong engine type");

    moreArgs->notional = notional_;
    moreArgs->longShort = longShort_;
    moreArgs->moneyness = QuantLib::ext::dynamic_pointer_cast<PercentageStrikePayoff>(payoff_)->strike();
    moreArgs->type = QuantLib::ext::dynamic_pointer_cast<PercentageStrikePayoff>(payoff_)->optionType();
    moreArgs->valuationDates = valuationDates_;
    moreArgs->paymentDate = paymentDate_;
    moreArgs->localCap = localCap_;
    moreArgs->localFloor = localFloor_;
    moreArgs->globalCap = globalCap_;
    moreArgs->globalFloor = globalFloor_;
    moreArgs->premium = premium_;
    moreArgs->premiumPayDate = premiumPayDate_;
    moreArgs->premiumCurrency = premiumCurrency_;
}

void CliquetOption::arguments::validate() const {

    QuantLib::ext::shared_ptr<PercentageStrikePayoff> moneyness = QuantLib::ext::dynamic_pointer_cast<PercentageStrikePayoff>(payoff);
    QL_REQUIRE(moneyness, "wrong payoff type");
    QL_REQUIRE(moneyness->strike() > 0.0, "negative or zero moneyness given");
    QL_REQUIRE(!valuationDates.empty(), "no reset dates given");
}

} // namespace QuantExt
