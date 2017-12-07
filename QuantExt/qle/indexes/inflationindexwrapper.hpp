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

/*! \file inflationindexwrapper.hpp
    \brief wrapper classes for inflation yoy and interpolation
    \ingroup indexes
*/

#ifndef quantext_inflation_index_wrapper_hpp
#define quantext_inflation_index_wrapper_hpp

#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <ql/indexes/inflationindex.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Wrapper that changes the interpolation of an underlying ZC inflation index
/*! The (possible) change in the interpolation is _not_ reflected in the index class itself,
  only the fixing methods behave consistently
 \ingroup indexes
 */
class ZeroInflationIndexWrapper : public ZeroInflationIndex {
public:
    ZeroInflationIndexWrapper(const boost::shared_ptr<ZeroInflationIndex> source,
                              const CPI::InterpolationType interpolation);
    /*! \warning the forecastTodaysFixing parameter (required by the Index interface) is currently ignored. */
    Rate fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const;

private:
    Rate forecastFixing(const Date& fixingDate) const;
    const boost::shared_ptr<ZeroInflationIndex> source_;
    const CPI::InterpolationType interpolation_;
};

//! Wrapper that creates a yoy from a zc index
/*! This creates a "ratio" - type YoYInflationIndex with the same family name as the zero
  index so that historical fixings can be reused. If a yoy ts is given, this is used to
  forecast fixings. If the ts is not given, the forecast falls back on the zero index, i.e.
  if the zero index has a curve attached a plain yoy rate without convexity adjustment
  is estimated using this index.
  The interpolation follows
  - the interpolated flag for historical fixings
  - the interpolated flag for forecasted fixings if a yoy ts is given
  - the underlying zero index behaviour for forecasted fixings if no yoy ts is given
\ingroup indexes
*/
class YoYInflationIndexWrapper : public YoYInflationIndex {
public:
    YoYInflationIndexWrapper(const boost::shared_ptr<ZeroInflationIndex> zeroIndex, const bool interpolated,
                             const Handle<YoYInflationTermStructure>& ts = Handle<YoYInflationTermStructure>());
    /*! \warning the forecastTodaysFixing parameter (required by the Index interface) is currently ignored. */
    Rate fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const;

private:
    Rate forecastFixing(const Date& fixingDate) const;
    const boost::shared_ptr<ZeroInflationIndex> zeroIndex_;
};

//! YY coupon pricer that takes the nominal ts directly instead of reading it from the yoy ts
/*! This is useful if no yoy ts is given, as it might be the case of the yoy inflation index wrapper
\ingroup indexes
*/
class YoYInflationCouponPricer2 : public YoYInflationCouponPricer {
public:
    YoYInflationCouponPricer2(
        const Handle<YieldTermStructure>& nominalTs,
        const Handle<YoYOptionletVolatilitySurface>& capletVol = Handle<YoYOptionletVolatilitySurface>())
        : YoYInflationCouponPricer(capletVol), nominalTs_(nominalTs) {}
    //! \name InflationCouponPricer interface
    //@{
    virtual void initialize(const InflationCoupon&);
    //@}

protected:
    const Handle<YieldTermStructure> nominalTs_;
};

} // namespace QuantExt

#endif
