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

/*! \file qle/utilities/inflation.hpp
    \brief some inflation related utilities.
*/

#ifndef quantext_inflation_hpp
#define quantext_inflation_hpp

#include <ql/indexes/inflationindex.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/time/period.hpp>
namespace QuantExt {

/*! Utility function for calculating the time to a given \p date based on a given inflation index,
    \p inflationIndex, and a given inflation term structure, \p inflationTs. An optional \p dayCounter can be 
    provided to use instead of the inflation term structure day counter.

    \ingroup utilities
*/
QuantLib::Time inflationTime(const QuantLib::Date& date,
    const QuantLib::ext::shared_ptr<QuantLib::InflationTermStructure>& inflationTs,
    bool indexIsInterpolated,
    const QuantLib::DayCounter& dayCounter = QuantLib::DayCounter());

/*! Utility for calculating the ratio \f$ \frac{P_r(0, t)}{P_n(0, t)} \f$ where \f$ P_r(0, t) \f$ is the real zero
    coupon bond price at time zero for maturity \f$ t \f$ and \f$ P_n(0, t) \f$ is the nominal zero coupon bond price.
*/
QuantLib::Real inflationGrowth(const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts,
    QuantLib::Time t,
    const QuantLib::DayCounter& dc,
    bool indexIsInterpolated);

/*! Utility for calculating the ratio \f$ \frac{P_r(0, t)}{P_n(0, t)} \f$ where \f$ P_r(0, t) \f$ is the real zero
    coupon bond price at time zero for maturity \f$ t \f$ and \f$ P_n(0, t) \f$ is the nominal zero coupon bond price.
*/
QuantLib::Real inflationGrowth(const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts,
    QuantLib::Time t, bool indexIsInterpolated);

/*! Calculate the Compound Factor to compute the nominal price from the real price
   I(t_s)/I(t_0) with I(t_s) the CPI at settlement date and I(t_0) the bond's base CPI
*/
QuantLib::Real inflationLinkedBondQuoteFactor(const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond);

/*! Iterates over all bond cashflows, and extract all inflation underlyings */
std::map<std::tuple<std::string, QuantLib::CPI::InterpolationType, QuantLib::Frequency, QuantLib::Period>,
         QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>>
extractAllInflationUnderlyingFromBond(const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond);

namespace ZeroInflation {

//! Check if today - availabilityLag is already known, otherwise return the fixingDate of the previous fixing
QuantLib::Date lastAvailableFixing(const QuantLib::ZeroInflationIndex& index, const QuantLib::Date& asof);


//! Computes a CPI fixing giving an zeroIndex, with interpolation if needed 
QuantLib::Rate cpiFixing(const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index, const QuantLib::Date& maturity,
                         const QuantLib::Period& obsLag, bool interpolated);


//! derives the zero inflation curve base date based on the useLastKnownFixing rule
QuantLib::Date curveBaseDate(const bool baseDateLastKnownFixing, const QuantLib::Date& refDate,
                             const QuantLib::Period obsLagCurve, const QuantLib::Frequency curveFreq,
                             const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index);


//! computes the fixingDate for ZC CPI Swap following the rule
//! for an interpolated index it is d - obsLag but 
//! for an interpolated index the fixing date is per definition on the start of the 
//! inflation period in which d - obsLag falls
QuantLib::Date fixingDate(const QuantLib::Date& d, const QuantLib::Period obsLag,
                                        const QuantLib::Frequency,
                                        bool interpolated);


/*! Computes the base rate for curve construction so that zero inflation rate is constant up to the first pillar
* Accounts for the acctual accrued inflation between the ZCIIS base date and the curve base date (e.g. last published fixing date)
* If curve base date and ZCIIS are the same, then the base rate is the ZCIIS rate
*/
QuantLib::Rate guessCurveBaseRate(const bool baseDateLastKnownFixing, const QuantLib::Date& swapStart,
                                  const QuantLib::Date& asof,
                                  const QuantLib::Period& swapTenor, const QuantLib::DayCounter& swapZCLegDayCounter,
                                  const QuantLib::Period& swapObsLag, const QuantLib::Rate zeroCouponRate, 
                                  const QuantLib::Period& curveObsLag, const QuantLib::DayCounter& curveDayCounter,
                                  const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index, const bool interpolated,
                                  const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality = nullptr);


//! checks if the vols are normal or lognormal 
//! if the volsurface is not derived from QuantExt::CPIVolatilitySurface we default to lognormal vols
bool isCPIVolSurfaceLogNormal(const QuantLib::ext::shared_ptr<QuantLib::CPIVolatilitySurface>& surface);


}

} // namespace QuantExt

#endif
