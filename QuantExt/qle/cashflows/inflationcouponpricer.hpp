/*
Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/inflationcouponpricer.hpp
\brief Utility functions for setting inflation coupon pricers on capped/floored YoY legs
\ingroup cashflows
*/

#ifndef quantext_inflation_coupon_pricer_hpp
#define quantext_inflation_coupon_pricer_hpp

#include <boost/shared_ptr.hpp>
#include <ql/cashflow.hpp>
#include <ql/option.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>

namespace QuantExt {
    using namespace QuantLib;

    //! pricer for capped/floored YoY inflation coupons
    class CappedFlooredYoYCouponPricer : public QuantLib::YoYInflationCouponPricer {
    public:
        CappedFlooredYoYCouponPricer(const Handle<QuantLib::YoYOptionletVolatilitySurface>& vol
            = Handle<QuantLib::YoYOptionletVolatilitySurface>());
        virtual Handle<QuantLib::YoYOptionletVolatilitySurface> volatility() const {
            return vol_;
        }

        virtual void setVolatility(
            const Handle<QuantLib::YoYOptionletVolatilitySurface>& vol);

        //! \name InflationCouponPricer interface
        //@{
        virtual Real swapletPrice() const;
        virtual Rate swapletRate() const;
        virtual Real capletPrice(Rate effectiveCap) const;
        virtual Rate capletRate(Rate effectiveCap) const;
        virtual Real floorletPrice(Rate effectiveFloor) const;
        virtual Rate floorletRate(Rate effectiveFloor) const;
        virtual void initialize(const InflationCoupon&);
        //@}

    protected:
        virtual Real optionletPrice(Option::Type optionType,
            Real effStrike) const;
        virtual Rate adjustedFixing(Rate fixing = Null<Rate>()) const;

        //! data
        Handle<QuantLib::YoYOptionletVolatilitySurface> vol_;
        const QuantLib::YoYInflationCoupon* coupon_;
        Real gearing_;
        Spread spread_;
        Real discount_;
        Real spreadLegValue_;
    };

} // namespace QuantExt

#endif
