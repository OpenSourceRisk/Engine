/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file futureoptionhelper.hpp
    \brief calibration helper for Black-Scholes options
    \ingroup models
*/

#ifndef quantext_calibrationhelper_futureoption_hpp
#define quantext_calibrationhelper_futureoption_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/models/calibrationhelper.hpp>
#include <qle/termstructures/pricetermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Future Option Helper
/*! \ingroup models
 */
class FutureOptionHelper : public BlackCalibrationHelper {
public:
    FutureOptionHelper(
        const Period& maturity, const Calendar& calendar, const Real strike,
        const Handle<QuantExt::PriceTermStructure> priceCurve, const Handle<Quote> volatility, 
        BlackCalibrationHelper::CalibrationErrorType errorType = BlackCalibrationHelper::RelativePriceError);
    FutureOptionHelper(
        const Date& exerciseDate, const Real strike,
        const Handle<QuantExt::PriceTermStructure> priceCurve, const Handle<Quote> volatility,
        BlackCalibrationHelper::CalibrationErrorType errorType = BlackCalibrationHelper::RelativePriceError);
    void addTimesTo(std::list<Time>&) const override {}
    void performCalculations() const override;
    Real modelValue() const override;
    Real blackPrice(Real volatility) const override;
    QuantLib::ext::shared_ptr<VanillaOption> option() const { return option_; }
    Real strike() const {
        calculate();
        return effStrike_;
    }
    const Handle<QuantExt::PriceTermStructure>& priceCurve() const { return priceCurve_; }
    
private:
    Handle<QuantExt::PriceTermStructure> priceCurve_;
    const bool hasMaturity_;
    Period maturity_;
    mutable Date exerciseDate_;
    Calendar calendar_;
    const Real strike_;
    mutable Real tau_;
    mutable Real atm_;
    mutable Option::Type type_;
    mutable QuantLib::ext::shared_ptr<VanillaOption> option_;
    mutable Real effStrike_;
};

} // namespace QuantExt

#endif
