/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/cashflows/yoyinflationcoupon.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/cashflows/cashflowvectors.hpp>

namespace QuantExt {

    YoYInflationCoupon::YoYInflationCoupon(const ext::shared_ptr<QuantLib::YoYInflationCoupon> underlying,
                                       bool growthOnly = true)
    : QuantLib::YoYInflationCoupon(underlying->date(), 
        underlying->nominal(), underlying->accrualStartDate(),
                                   underlying->accrualEndDate(), underlying->fixingDays() , underlying->index(),
                                   underlying->observationLag(), underlying->dayCounter(), underlying->gearing(), underlying->spread(), underlying->referencePeriodStart(), 
        underlying->referencePeriodEnd()),
      underlying_(underlying),
        growthOnly_(growthOnly) {
    
        registerWith(underlying_);
    }


    void YoYInflationCoupon::accept(AcyclicVisitor& v) {
        Visitor<YoYInflationCoupon>* v1 =
        dynamic_cast<Visitor<YoYInflationCoupon>*>(&v);
        if (v1 != 0)
            v1->visit(*this);
        else
            QuantLib::YoYInflationCoupon::accept(v);
    }

    void YoYInflationCoupon::setPricer(const ext::shared_ptr<YoYInflationCouponPricer>& pricer) {

        QuantLib::YoYInflationCoupon::setPricer(pricer);
        underlying_->setPricer(pricer);
    }

    Rate YoYInflationCoupon::rate() const { 
        if (growthOnly_) {
            return underlying_->rate();
        } else {
            return gearing_ * (underlying_->adjustedFixing() + 1) + spread_;
        }
    }

    CappedFlooredYoYInflationCoupon::CappedFlooredYoYInflationCoupon(
        const ext::shared_ptr<QuantLib::CappedFlooredYoYInflationCoupon> underlying,
                                           bool growthOnly = true)
        : QuantLib::CappedFlooredYoYInflationCoupon(underlying->date(), underlying->nominal(), underlying->accrualStartDate(), underlying->accrualEndDate(),
            underlying->fixingDays(), underlying->index(), underlying->observationLag(), underlying->dayCounter(), underlying->gearing(), underlying->spread(), 
            underlying->cap(), underlying->floor(), underlying->referencePeriodStart(), underlying->referencePeriodEnd()),
          underlying_(underlying), growthOnly_(growthOnly) {

        registerWith(underlying_);
    }

    void CappedFlooredYoYInflationCoupon::accept(AcyclicVisitor& v) {
        Visitor<CappedFlooredYoYInflationCoupon>* v1 = dynamic_cast<Visitor<CappedFlooredYoYInflationCoupon>*>(&v);
        if (v1 != 0)
            v1->visit(*this);
        else
            QuantLib::CappedFlooredYoYInflationCoupon::accept(v);
    }

    void CappedFlooredYoYInflationCoupon::setPricer(const ext::shared_ptr<YoYInflationCouponPricer>& pricer) {

        QuantLib::CappedFlooredYoYInflationCoupon::setPricer(pricer);
        underlying_->setPricer(pricer);
    }

    Rate CappedFlooredYoYInflationCoupon::rate() const {
        if (growthOnly_) {
            return underlying_->rate();
        } else {
            return gearing_ * (underlying_->adjustedFixing() + 1) + spread_;
        }
    }




    YoYCouponLeg::YoYCouponLeg(const Leg& underlyingLeg, bool growthOnly)
        : underlyingLeg_(underlyingLeg), growthOnly_(growthOnly) {}

    YoYCouponLeg::operator Leg() const {
        Leg resultLeg;
        resultLeg.reserve(underlyingLeg_.size());
        ext::shared_ptr<QuantLib::YoYInflationCoupon> c;
        for (Leg::const_iterator i = underlyingLeg_.begin(); i != underlyingLeg_.end(); ++i) {
            if ((c = ext::dynamic_pointer_cast<QuantLib::YoYInflationCoupon>(*i)) != NULL) {
                resultLeg.push_back(ext::make_shared<YoYInflationCoupon>(c, growthOnly_));
            } else {
                resultLeg.push_back(*i);
            }
        }
        return resultLeg;
    }





} // namespace QuantExt

