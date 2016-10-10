/*
 Copyright (C) 2015 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program.
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

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

/*! \file fxoptionhelper.hpp
    \brief calibration helper for fx options
    \ingroup models
*/

#ifndef quantext_calibrationhelper_fxoption_hpp
#define quantext_calibrationhelper_fxoption_hpp

#include <ql/models/calibrationhelper.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace QuantExt {

//! FX Option Helper
/*! \ingroup models
*/
class FxOptionHelper : public CalibrationHelper {
public:
    /*! the fx spot is interpreted as of today (or discounted spot)
        if strike is null, an (fwd-) atm option is constructed,
        a slight approximation is introduced because there is no
        settlement lag, however this applies consistently to
        the black and the model pricing */
    FxOptionHelper(const Period& maturity, const Calendar& calendar, const Real strike, const Handle<Quote> fxSpot,
                   const Handle<Quote> volatility, const Handle<YieldTermStructure>& domesticYield,
                   const Handle<YieldTermStructure>& foreignYield,
                   CalibrationHelper::CalibrationErrorType errorType = CalibrationHelper::RelativePriceError);
    FxOptionHelper(const Date& exerciseDate, const Real strike, const Handle<Quote> fxSpot,
                   const Handle<Quote> volatility, const Handle<YieldTermStructure>& domesticYield,
                   const Handle<YieldTermStructure>& foreignYield,
                   CalibrationHelper::CalibrationErrorType errorType = CalibrationHelper::RelativePriceError);
    void addTimesTo(std::list<Time>&) const {}
    void performCalculations() const;
    Real modelValue() const;
    Real blackPrice(Real volatility) const;
    boost::shared_ptr<VanillaOption> option() const { return option_; }

private:
    const bool hasMaturity_;
    Period maturity_;
    mutable Date exerciseDate_;
    Calendar calendar_;
    const Real strike_;
    const Handle<Quote> fxSpot_;
    const Handle<YieldTermStructure> foreignYield_;
    mutable Real tau_;
    mutable Real atm_;
    mutable Option::Type type_;
    mutable boost::shared_ptr<VanillaOption> option_;
    mutable Real effStrike_;
};

} // namespace QuantLib

#endif
