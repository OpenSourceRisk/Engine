/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*
 Copyright (C) 2011 Chris Kenyon


 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
 */

/*!
 \file cpicapfloorengines.cpp
 \brief  Extended version of the QuantLib engine, strike adjustment for seasoned CPI Cap/Floor pricing
 \ingroup PricingEngines
 */

#include <ql/time/daycounters/actualactual.hpp>

#include <ql/experimental/inflation/cpicapfloortermpricesurface.hpp>
#include <qle/pricingengines/cpicapfloorengines.hpp>

using namespace QuantLib;

namespace QuantExt {

InterpolatingCPICapFloorEngine::InterpolatingCPICapFloorEngine(const Handle<CPICapFloorTermPriceSurface>& priceSurf)
    : priceSurf_(priceSurf) {
    registerWith(priceSurf_);
}

void InterpolatingCPICapFloorEngine::calculate() const {
    Real npv = 0.0;

    // RL change, adjusted strike, this is shared code between cpiblackcapfloorengine and this engine
    Handle<ZeroInflationIndex> zii(arguments_.index);
    Handle<ZeroInflationTermStructure> zits = zii->zeroInflationTermStructure();
    Date baseDate = zits->baseDate();
    Real baseCPI = arguments_.baseCPI;
    Real baseFixing = zii->fixing(baseDate);
    Date adjustedMaturity = arguments_.payDate - arguments_.observationLag;
    if (!zii->interpolated()) {
        std::pair<Date, Date> ipm = inflationPeriod(adjustedMaturity, zii->frequency());
        adjustedMaturity = ipm.first;
    }
    Date adjustedStart = arguments_.startDate - arguments_.observationLag;
    if (!zii->interpolated()) {
        std::pair<Date, Date> ips = inflationPeriod(adjustedStart, zii->frequency());
        adjustedStart = ips.first;
    }
    Real timeFromStart = zits->dayCounter().yearFraction(adjustedStart, adjustedMaturity);
    Real timeFromBase = zits->dayCounter().yearFraction(baseDate, adjustedMaturity);
    Real strike = pow(baseCPI / baseFixing * pow(1.0 + arguments_.strike, timeFromStart), 1.0 / timeFromBase) - 1.0;
    // end of RL change

    // what is the difference between the observationLag of the surface
    // and the observationLag of the cap/floor?
    // \TODO next line will fail if units are different
    Period lagDiff = arguments_.observationLag - priceSurf_->observationLag();
    // next line will fail if units are different if Period() is not well written
    QL_REQUIRE(lagDiff >= Period(0, Months), "InterpolatingCPICapFloorEngine: "
                                             "lag difference must be non-negative: "
                                                 << lagDiff);

    // we now need an effective maturity to use in the price surface because this uses
    // maturity of calibration instruments as its time axis, N.B. this must also
    // use the roll because the surface does
    Date effectiveMaturity = arguments_.payDate - lagDiff;

    // what interpolation do we use? Index / flat / linear
    if (arguments_.observationInterpolation == CPI::AsIndex) {
        // same as index means we can just use the price surface
        // since this uses the index
        if (arguments_.type == Option::Call) {
            npv = priceSurf_->capPrice(effectiveMaturity, strike);
        } else {
            npv = priceSurf_->floorPrice(effectiveMaturity, strike);
        }

    } else {
        std::pair<Date, Date> dd = inflationPeriod(effectiveMaturity, arguments_.index->frequency());
        Real priceStart = 0.0;

        if (arguments_.type == Option::Call) {
            priceStart = priceSurf_->capPrice(dd.first, strike);
        } else {
            priceStart = priceSurf_->floorPrice(dd.first, strike);
        }

        // if we use a flat index vs the interpolated one ...
        if (arguments_.observationInterpolation == CPI::Flat) {
            // then use the price for the first day in the period because the value cannot change after then
            npv = priceStart;

        } else {
            // linear interpolation will be very close
            Real priceEnd = 0.0;
            if (arguments_.type == Option::Call) {
                priceEnd = priceSurf_->capPrice((dd.second + Period(1, Days)), strike);
            } else {
                priceEnd = priceSurf_->floorPrice((dd.second + Period(1, Days)), strike);
            }

            npv = priceStart + (priceEnd - priceStart) * (effectiveMaturity - dd.first) /
                                   ((dd.second + Period(1, Days)) - dd.first); // can't get to next period'
        }
    }
    results_.value = npv * arguments_.nominal;
}

} // namespace QuantExt
