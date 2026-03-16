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

#include <qle/instruments/outperformanceoption.hpp>
#include <qle/pricingengines/analyticoutperformanceoptionengine.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

    OutperformanceOption::OutperformanceOption( const QuantLib::ext::shared_ptr<Exercise>& exercise, const Option::Type optionType, 
        const Real strikeReturn, const Real initialValue1, const Real initialValue2, const Real notional, const Real knockInPrice, const Real knockOutPrice, QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex1, QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex2 )
    : exercise_(exercise), optionType_(optionType), strikeReturn_(strikeReturn), initialValue1_(initialValue1), initialValue2_(initialValue2), notional_(notional),
        knockInPrice_(knockInPrice), knockOutPrice_(knockOutPrice), fxIndex1_(fxIndex1), fxIndex2_(fxIndex2) {}

    bool OutperformanceOption::isExpired() const {
        return detail::simple_event(exercise_->dates().back()).hasOccurred();
    }

    void OutperformanceOption::setupExpired() const {
        NPV_ =  0.0;
    }

    void OutperformanceOption::setupArguments(
                                       PricingEngine::arguments* args) const {

        OutperformanceOption::arguments* arguments =
            dynamic_cast<OutperformanceOption::arguments*>(args);

        QL_REQUIRE(arguments != 0, "wrong argument type");

        arguments->exercise = exercise_;
        arguments->optionType = optionType_;
        arguments->strikeReturn = strikeReturn_;
        arguments->initialValue1 = initialValue1_;
        arguments->initialValue2 = initialValue2_;
        arguments->notional = notional_;
        arguments->knockInPrice = knockInPrice_;
        arguments->knockOutPrice = knockOutPrice_;
        arguments->knockInPrice = knockInPrice_;
        arguments->fxIndex1 = fxIndex1_;
        arguments->fxIndex2 = fxIndex2_;

    }

    void OutperformanceOption::arguments::validate() const {
        QL_REQUIRE(exercise, "exercise not set");
    }
    
    void OutperformanceOption::fetchResults(const PricingEngine::results* r) const {
        Instrument::fetchResults(r);
        const OutperformanceOption::results* results =
            dynamic_cast<const OutperformanceOption::results*>(r);

        QL_ENSURE(results != 0,
                  "wrong result type");
    }
} // namespace QuantExt
