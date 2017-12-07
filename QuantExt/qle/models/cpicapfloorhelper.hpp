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

/*! \file cpicapfloorhelper.hpp
    \brief CPI Cap Floor calibration helper
    \ingroup models
*/

#ifndef quantext_cpicapfloor_calibration_helper_hpp
#define quantext_cpicapfloor_calibration_helper_hpp

#include <ql/instruments/cpicapfloor.hpp>
#include <ql/models/calibrationhelper.hpp>

using namespace QuantLib;

namespace QuantExt {

/* Note that calibration helpers that are not based on a implied volatility but directly on
   a premium are part of QL PR 18 */
//! CPI cap floor helper
/*!
 \ingroup models
*/
class CpiCapFloorHelper : public CalibrationHelper {
public:
    CpiCapFloorHelper(Option::Type type, Real baseCPI, const Date& maturity, const Calendar& fixCalendar,
                      BusinessDayConvention fixConvention, const Calendar& payCalendar,
                      BusinessDayConvention payConvention, Real strike, const Handle<ZeroInflationIndex>& infIndex,
                      const Period& observationLag, Real marketPremium,
                      CPI::InterpolationType observationInterpolation = CPI::AsIndex,
                      CalibrationHelper::CalibrationErrorType errorType = CalibrationHelper::RelativePriceError);

    Real modelValue() const;
    Real blackPrice(Volatility volatility) const;
    void addTimesTo(std::list<Time>&) const {}

    boost::shared_ptr<CPICapFloor> instrument() const { return instrument_; }

private:
    boost::shared_ptr<CPICapFloor> instrument_;
};

} // namespace QuantExt

#endif
