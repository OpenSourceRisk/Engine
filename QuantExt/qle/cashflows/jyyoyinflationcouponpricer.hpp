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

/*! \file qle/cashflows/jyyoyinflationcouponpricer.hpp
    \brief Jarrow Yildrim (JY) pricer for capped or floored year on year (YoY) inflation coupons
*/

#pragma once

#include <qle/models/crossassetmodel.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>

namespace QuantExt {

//! JY pricer for YoY inflation coupons.
class JyYoYInflationCouponPricer : public QuantLib::YoYInflationCouponPricer {
public:
    /*! Constructor
        \param model the cross asset model to be used in the valuation.
        \param index the index of the inflation component to use within the cross asset model.
    */
    JyYoYInflationCouponPricer(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
        QuantLib::Size index);

private:
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::Size index_;

    QuantLib::Real optionletRate(QuantLib::Option::Type optionType, QuantLib::Real effStrike) const override;

    QuantLib::Rate adjustedFixing(QuantLib::Rate fixing = QuantLib::Null<QuantLib::Rate>()) const override;
};

/*! Return the expected value of the inflation index ratio \f$I(T)/I(S)\f$ under Jarrow Yildrim where 
    \f$ 0 < S < T \f$. The value is given in Section 13 of <em>Modern Derivatives Pricing and Credit Exposure 
    Analysis, 2015</em> by the following:
    \f[
    \frac{P_r(0,T)}{P_n(0,T)} \frac{P_n(0,S)}{P_r(0,S)} e^{C(S,T)}
    \f]
*/
QuantLib::Real jyExpectedIndexRatio(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
    QuantLib::Size index, QuantLib::Time S, QuantLib::Time T, bool indexIsInterpolated);

}
