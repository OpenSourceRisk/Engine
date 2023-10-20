/*
 Copyright (C) 2009 Chris Kenyon

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

/*! \file kinterpolatedyoyoptionletvolatilitysurface.hpp
    \brief fixed version of ql class (see patch 1,2,3 in the comments below)
*/

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd

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

#pragma once

#include <ql/experimental/inflation/yoyoptionletstripper.hpp>

namespace QuantExt {
    using namespace QuantLib;

    //! K-interpolated YoY optionlet volatility
    /*! The stripper provides curves in the T direction along each K.
        We don't know whether this is interpolating or fitting in the
        T direction.  Our K direction interpolations are not model
        fitting.

        An alternative design would be a
        FittedYoYOptionletVolatilitySurface taking a model, e.g. SABR
        in the interest rate world.  This could use the same stripping
        in the T direction along each K.

        \bug Tests currently fail.
    */
    template<class Interpolator1D>
    class KInterpolatedYoYOptionletVolatilitySurface
        : public YoYOptionletVolatilitySurface {
      public:
        //! \name Constructor
        //! calculate the reference date based on the global evaluation date
        KInterpolatedYoYOptionletVolatilitySurface(
           const Natural settlementDays,
           const Calendar&,
           const BusinessDayConvention bdc,
           const DayCounter& dc,
           const Period& lag,
           const ext::shared_ptr<YoYCapFloorTermPriceSurface>& capFloorPrices,
           const ext::shared_ptr<QuantLib::YoYInflationCapFloorEngine>& pricer,
           const ext::shared_ptr<YoYOptionletStripper>& yoyOptionletStripper,
           const Real slope,
           const Interpolator1D& interpolator = Interpolator1D(),
           VolatilityType volType = ShiftedLognormal,
           Real displacement = 0.0);

        virtual Real minStrike() const override;
        virtual Real maxStrike() const override;
        virtual Date maxDate() const override;
        std::pair<std::vector<Rate>, std::vector<Volatility> > Dslice(
                                                         const Date &d) const;

      protected:
        virtual Volatility volatilityImpl(const Date &d,
                                          Rate strike) const;
        virtual Volatility volatilityImpl(Time length,
                                          Rate strike) const override;
        virtual void performCalculations() const;

        ext::shared_ptr<YoYCapFloorTermPriceSurface> capFloorPrices_;
        ext::shared_ptr<QuantLib::YoYInflationCapFloorEngine> yoyInflationCouponPricer_;
        ext::shared_ptr<YoYOptionletStripper> yoyOptionletStripper_;

        mutable Interpolator1D factory1D_;
        mutable Real slope_;
        mutable bool lastDateisSet_;
        mutable Date lastDate_;
        mutable Interpolation tempKinterpolation_;
        mutable std::pair<std::vector<Rate>, std::vector<Volatility> > slice_;
      private:
        void updateSlice(const Date &d) const;
    };


    // template definitions

    template<class Interpolator1D>
    KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::
    KInterpolatedYoYOptionletVolatilitySurface(
         const Natural settlementDays,
         const Calendar& cal,
         const BusinessDayConvention bdc,
         const DayCounter& dc,
         const Period &lag,
         const ext::shared_ptr<YoYCapFloorTermPriceSurface> &capFloorPrices,
         const ext::shared_ptr<QuantLib::YoYInflationCapFloorEngine> &pricer,
         const ext::shared_ptr<YoYOptionletStripper> &yoyOptionletStripper,
         const Real slope,
         const Interpolator1D &interpolator,
         VolatilityType volType,
         Real displacement)
    : YoYOptionletVolatilitySurface(settlementDays, cal, bdc, dc, lag,
                                    capFloorPrices->yoyIndex()->frequency(),
                                    capFloorPrices->yoyIndex()->interpolated(),
                                    volType, displacement),
      capFloorPrices_(capFloorPrices), yoyInflationCouponPricer_(pricer),
      yoyOptionletStripper_(yoyOptionletStripper),
      factory1D_(interpolator), slope_(slope), lastDateisSet_(false) {
        performCalculations();
    }


    template<class Interpolator1D>
    Date KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::
    maxDate() const {
        Size n = capFloorPrices_->maturities().size();
        return referenceDate()+capFloorPrices_->maturities()[n-1];
    }


    template<class Interpolator1D>
    Real KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::
    minStrike() const {
        return capFloorPrices_->strikes().front();
    }


    template<class Interpolator1D>
    Real KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::
    maxStrike() const {
        return capFloorPrices_->strikes().back();
    }


    template<class Interpolator1D>
    void KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::
    performCalculations() const {

        // slope is the assumption on the initial caplet volatility change
        yoyOptionletStripper_->initialize(capFloorPrices_,
                                          yoyInflationCouponPricer_,
                                          slope_);
    }


    template<class Interpolator1D>
    Volatility KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::
    volatilityImpl(const Date &d, Rate strike) const {
        updateSlice(d);
        // patch 1 for QL class:
        // extrapolation on interpolator (if enabled in this class)
        if (this->allowsExtrapolation()) {
            this->tempKinterpolation_.enableExtrapolation();
        }
        return tempKinterpolation_(strike);
    }


    template<class Interpolator1D>
    std::pair<std::vector<Rate>, std::vector<Volatility> >
    KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::
    Dslice(const Date &d) const {
        updateSlice(d);
        return slice_;
    }


    template<class Interpolator1D>
    Volatility KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::
    volatilityImpl(Time length,  Rate strike) const {

        Natural years = (Natural)floor(length);
        Natural days = (Natural)floor((length - years) * 365.0);
        Date d = referenceDate() + Period(years, Years) + Period(days, Days);

        return this->volatilityImpl(d, strike);
    }

    template<class Interpolator1D>
    void KInterpolatedYoYOptionletVolatilitySurface<Interpolator1D>::
    updateSlice(const Date &d) const {

        if (!lastDateisSet_ || d != lastDate_ ) {
            // patch 2 for QL class:
            // add observation lag, this is subtracted again in the stripper
            Date d_eff = d + capFloorPrices_->observationLag();
            // patch 3 for QL class:
            // flat extrapolation in date direction, if extrapolation is enabled
            if (this->allowsExtrapolation())
                d_eff = std::min(d_eff, maxDate());
            slice_ = yoyOptionletStripper_->slice(d_eff);

            tempKinterpolation_ =
                factory1D_.interpolate( slice_.first.begin(),
                                        slice_.first.end(),
                                        slice_.second.begin() );
            lastDateisSet_ = true;
            lastDate_ = d;
        }
    }

}

