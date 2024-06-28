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

#include <ql/experimental/coupons/cmsspreadcoupon.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/experimental/coupons/swapspreadindex.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/models/cmscaphelper.hpp>

namespace QuantExt {

QuantLib::Real CmsCapHelper::modelValue() const {
    calculate();
    return cap_->NPV();
}

void CmsCapHelper::performCalculations() const {

    Date startDate;
    Date endDate;

    // Compute ATM swap rates
    Real spread, rate1, rate2;

    std::vector<Real> nominals(1, 1.0);

    {
        QuantLib::ext::shared_ptr<PricingEngine> swapEngine(
            new DiscountingSwapEngine(index1_->discountingTermStructure(), false));

        Calendar calendar = index1_->fixingCalendar();

        QuantLib::ext::shared_ptr<IborIndex> index = index1_->iborIndex();

        // FIXME, reduce length by forward start
        startDate = calendar.advance(calendar.advance(asof_, spotDays_), forwardStart_);
        endDate = calendar.advance(startDate, length_ - forwardStart_, index->businessDayConvention());

        Schedule cmsSchedule(startDate, endDate, cmsTenor_, calendar, index->businessDayConvention(),
                             index->businessDayConvention(), DateGeneration::Forward, false);

        Leg cmsLeg = CmsLeg(cmsSchedule, index1_)
                         .withNotionals(nominals)
                         .withPaymentAdjustment(index1_->iborIndex()->businessDayConvention())
                         .withPaymentDayCounter(index1_->iborIndex()->dayCounter())
                         .withFixingDays(fixingDays_);
        QuantLib::setCouponPricer(cmsLeg, cmsPricer_);

        std::vector<Leg> cmsLegs;
        std::vector<bool> cmsPayers;
        cmsLegs.push_back(cmsLeg);
        cmsPayers.push_back(true);
        QuantLib::ext::shared_ptr<QuantLib::Swap> s = QuantLib::ext::make_shared<QuantLib::Swap>(cmsLegs, cmsPayers);
        s->setPricingEngine(swapEngine);

        rate1 = s->NPV() / (s->legBPS(0) / 1.0e-4);
    }

    QuantLib::ext::shared_ptr<PricingEngine> swapEngine(new DiscountingSwapEngine(index2_->discountingTermStructure(), false));

    Calendar calendar = index2_->fixingCalendar();

    QuantLib::ext::shared_ptr<IborIndex> index = index2_->iborIndex();
    // FIXME, reduce length by forward start
    startDate = calendar.advance(calendar.advance(asof_, spotDays_), forwardStart_);
    endDate = calendar.advance(startDate, length_ - forwardStart_, index->businessDayConvention());

    Schedule cmsSchedule(startDate, endDate, cmsTenor_, calendar, index->businessDayConvention(),
                         index->businessDayConvention(), DateGeneration::Forward, false);

    Leg cmsLeg = CmsLeg(cmsSchedule, index2_)
                     .withNotionals(nominals)
                     .withPaymentAdjustment(index2_->iborIndex()->businessDayConvention())
                     .withPaymentDayCounter(index2_->iborIndex()->dayCounter())
                     .withFixingDays(fixingDays_);
    QuantLib::setCouponPricer(cmsLeg, cmsPricer_);

    std::vector<Leg> cmsLegs;
    std::vector<bool> cmsPayers;
    cmsLegs.push_back(cmsLeg);
    cmsPayers.push_back(true);
    QuantLib::ext::shared_ptr<QuantLib::Swap> s = QuantLib::ext::make_shared<QuantLib::Swap>(cmsLegs, cmsPayers);
    s->setPricingEngine(swapEngine);

    rate2 = s->NPV() / (s->legBPS(0) / 1.0e-4);

    spread = rate1 - rate2;

    // Construct CMS Spread CapFloor

    std::vector<Leg> legs;
    std::vector<bool> legPayers;

    QuantLib::ext::shared_ptr<QuantLib::SwapSpreadIndex> spreadIndex = QuantLib::ext::make_shared<QuantLib::SwapSpreadIndex>(
        "CMSSpread_" + index1_->familyName() + "_" + index2_->familyName(), index1_, index2_);

    startDate = calendar_.advance(calendar_.advance(asof_, spotDays_), forwardStart_);
    endDate = calendar_.advance(startDate, length_ - forwardStart_, convention_);

    Schedule cmsSpreadSchedule(startDate, endDate, cmsTenor_, calendar_, convention_, convention_,
                               DateGeneration::Forward, false);
    Leg cmsLeg1 = CmsSpreadLeg(cmsSpreadSchedule, spreadIndex)
                      .withNotionals(nominals)
                      .withSpreads(std::vector<Rate>(1, 0))
                      .withPaymentAdjustment(convention_)
                      .withPaymentDayCounter(dayCounter_)
                      .withFixingDays(fixingDays_)
                      .inArrears(true)
                      .withCaps(std::vector<Rate>(1, spread));

    QuantLib::setCouponPricer(cmsLeg1, pricer_);

    Leg cmsLeg1b = StrippedCappedFlooredCouponLeg(cmsLeg1);

    legs.push_back(cmsLeg1b);
    legPayers.push_back(false);

    cap_ = QuantLib::ext::make_shared<QuantLib::Swap>(legs, legPayers);
    QuantLib::ext::shared_ptr<PricingEngine> swapEngine2(new DiscountingSwapEngine(discountCurve_, false));
    cap_->setPricingEngine(swapEngine2);
}

} // namespace QuantExt
