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

/*! \file fxeqoptionhelper.hpp
    \brief calibration helper for Black-Scholes options
    \ingroup models
*/

#ifndef quantext_calibrationhelper_fxeqoption_hpp
#define quantext_calibrationhelper_fxeqoption_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/models/calibrationhelper.hpp>

using namespace QuantLib;

namespace QuantExt {

//! FxEq Option Helper
/*! \ingroup models
 */
class FxEqOptionHelper : public CalibrationHelper {
public:
    /*! the spot is interpreted as of today (or discounted spot)
        if strike is null, an (fwd-) atm option is constructed,
        a slight approximation is introduced because there is no
        settlement lag, however this applies consistently to
        the black and the model pricing */
    FxEqOptionHelper(const Period& maturity, const Calendar& calendar, const Real strike, const Handle<Quote> spot,
                     const Handle<Quote> volatility, const Handle<YieldTermStructure>& domesticYield,
                     const Handle<YieldTermStructure>& foreignYield,
                     CalibrationHelper::CalibrationErrorType errorType = CalibrationHelper::RelativePriceError);
    FxEqOptionHelper(const Date& exerciseDate, const Real strike, const Handle<Quote> spot,
                     const Handle<Quote> volatility, const Handle<YieldTermStructure>& domesticYield,
                     const Handle<YieldTermStructure>& foreignYield,
                     CalibrationHelper::CalibrationErrorType errorType = CalibrationHelper::RelativePriceError);
    void addTimesTo(std::list<Time>&) const {}
    void performCalculations() const;
    Real modelValue() const;
    Real blackPrice(Real volatility) const;
    boost::shared_ptr<VanillaOption> option() const { return option_; }
    Real strike() const {
        calculate();
        return effStrike_;
    }

private:
    const bool hasMaturity_;
    Period maturity_;
    mutable Date exerciseDate_;
    Calendar calendar_;
    const Real strike_;
    const Handle<Quote> spot_;
    const Handle<YieldTermStructure> foreignYield_;
    mutable Real tau_;
    mutable Real atm_;
    mutable Option::Type type_;
    mutable boost::shared_ptr<VanillaOption> option_;
    mutable Real effStrike_;
};

} // namespace QuantExt

#endif
