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

/*! \file qle/termstructures/interpolatedyoycapfloortermpricesurface.hpp
    \brief Interpolated YoY Inflation Cap floor term price surface - 
           extends QuantLib InterpolatedYoYCapFloorTermPriceSurface to allow
           choice of termstruture directly from YoY swap quotes or from 
           atm swap quotes stripped from cap/floor price surface
    \ingroup termstructures
*/

#ifndef quantext_interpolated_yoy_capfloor_term_pricesurface_hpp
#define quantext_interpolated_yoy_capfloor_term_pricesurface_hpp

#include <ql/experimental/inflation/yoycapfloortermpricesurface.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Interpolated YoY Inflation Cap floor term price surface
/*! \ingroup termstructures */
template<class Interpolator2D, class Interpolator1D>
class InterpolatedYoYCapFloorTermPriceSurface
    : public QuantLib::InterpolatedYoYCapFloorTermPriceSurface<Interpolator2D, Interpolator1D> {
public:
    InterpolatedYoYCapFloorTermPriceSurface(
        Natural fixingDays,
        const Period &yyLag,  // observation lag
        const boost::shared_ptr<YoYInflationIndex>& yii,
        Rate baseRate,
        const Handle<YieldTermStructure> &nominal,
        const DayCounter &dc,
        const Calendar &cal,
        const BusinessDayConvention &bdc,
        const std::vector<Rate> &cStrikes,
        const std::vector<Rate> &fStrikes,
        const std::vector<Period> &cfMaturities,
        const Matrix &cPrice,
        const Matrix &fPrice,
        bool useYoySwaps = false,
        const Interpolator2D &interpolator2d = Interpolator2D(),
        const Interpolator1D &interpolator1d = Interpolator1D()) :
        QuantLib::InterpolatedYoYCapFloorTermPriceSurface<Interpolator2D, Interpolator1D>(
            fixingDays, yyLag, yii, baseRate, nominal, dc, cal, bdc, cStrikes, fStrikes, cfMaturities,
            cPrice, fPrice, Interpolator2D(), Interpolator1D()),
        useYoySwaps_(useYoySwaps) {}

    boost::shared_ptr<YoYInflationTermStructure> YoYTS() const override {
        return useYoySwaps_ ? yoyIndex_->yoyInflationTermStructure().currentLink() : yoy_;
    };

private:
    bool useYoySwaps_;

};

} // namespace QuantExt

#endif