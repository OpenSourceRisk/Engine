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

#include <ored/marketdata/marketimpl.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/credit/piecewisedefaultcurve.hpp>
#include <ql/termstructures/voltermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/crossccybasisswaphelper.hpp>
#include <ql/termstructures/credit/defaultprobabilityhelpers.hpp>
#include <qle/termstructures/oisratehelper.hpp>

using namespace QuantLib;
using namespace ore::data;
using std::pair;
using std::vector;

//! Simple flat market setup to be used in the test suite, plain copy from OREAP test suite
/*!
  \ingroup tests
*/
class OredTestMarket : public ore::data::MarketImpl {
public:
    OredTestMarket(Date asof, bool swapVolCube = false);

private:
    Handle<YieldTermStructure> flatRateYts(Real forward);

    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward);
    Handle<YieldTermStructure> flatRateDiv(Real dividend);
    Handle<QuantLib::SwaptionVolatilityStructure> flatRateSvs(Volatility forward,
                                                              VolatilityType type = ShiftedLognormal, Real shift = 0.0);
    Handle<QuantExt::CreditCurve> flatRateDcs(Volatility forward);
    Handle<OptionletVolatilityStructure> flatRateCvs(Volatility vol, VolatilityType type = Normal, Real shift = 0.0);
};
