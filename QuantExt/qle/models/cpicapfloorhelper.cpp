/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/models/cpicapfloorhelper.hpp>

#include <ql/quotes/simplequote.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

CpiCapFloorHelper::CpiCapFloorHelper(Option::Type type, Real baseCPI, const Date& maturity, const Calendar& fixCalendar,
                                     BusinessDayConvention fixConvention, const Calendar& payCalendar,
                                     BusinessDayConvention payConvention, Real strike,
                                     const Handle<ZeroInflationIndex>& infIndex, const Period& observationLag,
                                     Real marketPremium, CPI::InterpolationType observationInterpolation,
                                     CalibrationHelper::CalibrationErrorType errorType)
    : CalibrationHelper(Handle<Quote>(boost::make_shared<SimpleQuote>(0.0)), Handle<YieldTermStructure>(), errorType),
      // start date does not really matter ?
      instrument_(boost::shared_ptr<CPICapFloor>(new CPICapFloor(
          type, 1.0, Settings::instance().evaluationDate(), baseCPI, maturity, fixCalendar, fixConvention, payCalendar,
          payConvention, strike, infIndex, observationLag, observationInterpolation))) {
    QL_REQUIRE(errorType == CalibrationHelper::PriceError || errorType == CalibrationHelper::RelativePriceError,
               "CpiCapFloorHelper supports only PriceError and "
               "RelativePriceError error types");
    QL_REQUIRE(marketPremium > 0.0 && !close_enough(marketPremium, 0.0),
               "can not calibrate to market premium " << marketPremium);
    marketValue_ = marketPremium;
}

Real CpiCapFloorHelper::modelValue() const {
    calculate();
    instrument_->setPricingEngine(engine_);
    return instrument_->NPV();
}

Real CpiCapFloorHelper::blackPrice(Volatility) const {
    calculate();
    return marketValue_;
}

} // namespace QuantExt
