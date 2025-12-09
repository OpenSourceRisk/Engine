/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#pragma once

#include <ql/cashflows/iborcoupon.hpp>

namespace QuantExt {

//! utility to locally set usingAtParCoupons in IborCoupon::Settings, using RAII to restore original state
class LocalIborCouponSettings {
public:
    explicit LocalIborCouponSettings(const bool usingAtParCoupons) {
        usingAtParCouponsSaved_ = QuantLib::IborCoupon::Settings::instance().usingAtParCoupons();
        updateGlobalSetting(usingAtParCoupons);
    }
    ~LocalIborCouponSettings() { updateGlobalSetting(usingAtParCouponsSaved_); }

private:
    void updateGlobalSetting(const bool usingAtParCoupons) {
        if (usingAtParCoupons)
            QuantLib::IborCoupon::Settings::instance().createAtParCoupons();
        else
            QuantLib::IborCoupon::Settings::instance().createIndexedCoupons();
    }
    bool usingAtParCouponsSaved_;
};

} // namespace QuantExt
