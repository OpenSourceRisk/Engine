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

/*! \file qle/models/representativefxoption.hpp
    \brief representative fx option matcher
    \ingroup models
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/date.hpp>

namespace QuantExt {

using namespace QuantLib;

/*! Given cashflows in two currencies and a reference date >= today, find amounts in the two currencies to be paid
    on the reference date such that the NPV and FX Delta of the original cashflows and the original cashflows
    as seen from the reference date are equal.

    The output amounts have a sign, i.e. they are received if positive and paid if negative.
*/

class RepresentativeFxOptionMatcher {
public:
    // the fx spot should be a FX Spot discounted to today
    RepresentativeFxOptionMatcher(const std::vector<Leg>& underlying, const std::vector<bool>& isPayer,
                                  const std::vector<std::string>& currencies, const Date& referenceDate,
                                  const std::string& forCcy, const std::string& domCcy,
                                  const Handle<YieldTermStructure>& forCurve,
                                  const Handle<YieldTermStructure>& domCurve, const Handle<Quote>& fxSpot,
                                  const bool includeRefDateFlows = false);

    std::string currency1() const { return ccy1_; }
    std::string currency2() const { return ccy2_; }
    Real amount1() const { return amount1_; }
    Real amount2() const { return amount2_; }

private:
    std::string ccy1_, ccy2_;
    Real amount1_, amount2_;
};

} // namespace QuantExt
