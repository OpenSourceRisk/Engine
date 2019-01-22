/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2001, 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2015 Peter Caspers

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

#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <qle/models/cmscaphelper.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/experimental/coupons/swapspreadindex.hpp>
#include <ql/experimental/coupons/cmsspreadcoupon.hpp>
#include <ql/time/daycounters/actual360.hpp>

#include <iostream>
namespace QuantExt {

    QuantLib::Real CmsCapHelper::modelValue() const {
        calculate();
        return cap_->NPV();
    }

    void CmsCapHelper::performCalculations() const {

        Date startDate; 
        Date endDate;

        //Compute ATM swap rates
        Real spread, rate1, rate2;

        std::vector<Real> nominals(1,1.0);

        {   
        boost::shared_ptr<PricingEngine> swapEngine(
                             new DiscountingSwapEngine(index1_->discountingTermStructure(), false));
     
        Calendar calendar = index1_->fixingCalendar();
        
        boost::shared_ptr<IborIndex> index = index1_->iborIndex();
        // FIXME, reduce length by forward start
        startDate = calendar.advance(calendar.advance(asof_, spotDays_),forwardStart_);
        endDate = calendar.advance(startDate, length_ - forwardStart_,
                                      index->businessDayConvention());
        

        
        Schedule cmsSchedule(startDate, endDate, cmsTenor_, calendar,
                               index->businessDayConvention(),
                               index->businessDayConvention(),
                               DateGeneration::Forward, false);

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
        boost::shared_ptr<QuantLib::Swap> s = boost::make_shared<QuantLib::Swap>(cmsLegs, cmsPayers);
        s->setPricingEngine(swapEngine);


        rate1 = s->NPV()/(s->legBPS(0)/1.0e-4);

     }
        
        boost::shared_ptr<PricingEngine> swapEngine(
                             new DiscountingSwapEngine(index2_->discountingTermStructure(), false));
             
        Calendar calendar = index2_->fixingCalendar();
        
        boost::shared_ptr<IborIndex> index = index2_->iborIndex();
        // FIXME, reduce length by forward start
        //startDate = calendar.advance(calendar.advance(asof_, spotDays_),forwardStart_);
        endDate = calendar.advance(startDate, length_-forwardStart_,
                                       index->businessDayConvention());
        

        
        Schedule cmsSchedule(startDate, endDate, cmsTenor_, calendar,
                               index->businessDayConvention(),
                               index->businessDayConvention(),
                               DateGeneration::Forward, false);
        
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
        boost::shared_ptr<QuantLib::Swap> s = boost::make_shared<QuantLib::Swap>(cmsLegs, cmsPayers);
        s->setPricingEngine(swapEngine);


        rate2 = s->NPV()/(s->legBPS(0)/1.0e-4);

    
        spread = rate2 - rate1;

        //Construct CMS Spread CapFloor
        
        std::vector<Leg> legs;
        std::vector<bool> legPayers;
        
        boost::shared_ptr<QuantLib::SwapSpreadIndex> spreadIndex = 
                                boost::make_shared<QuantLib::SwapSpreadIndex>(
                                    "CMSSpread_" + index1_->familyName() + "_" + index2_->familyName(), index2_, index1_);


        //startDate = calendar.advance(calendar.advance(asof_, spotDays_),forwardStart_);
        endDate = calendar.advance(startDate, length_-forwardStart_,
                                       index->businessDayConvention());

        Schedule cmsSpreadSchedule(startDate, endDate, cmsTenor_, calendar,
                               index->businessDayConvention(),
                               index->businessDayConvention(),
                               DateGeneration::Forward, false);
        Leg cmsLeg1 = CmsSpreadLeg(cmsSpreadSchedule, spreadIndex)
                    .withNotionals(nominals)
                    .withSpreads(std::vector<Rate>(1, 0))
                    .withPaymentAdjustment(index->businessDayConvention())
                    .withPaymentDayCounter(Actual360())
                    .withFixingDays(fixingDays_)
                    .inArrears(true)
                    .withCaps(std::vector<Rate>(1, spread));

        QuantLib::setCouponPricer(cmsLeg1, pricer_);

        Leg cmsLeg1b = StrippedCappedFlooredCouponLeg(cmsLeg1);

        legs.push_back(cmsLeg1b);
        legPayers.push_back(false);

        cap_ = boost::make_shared<QuantLib::Swap>(legs, legPayers);

        cap_->setPricingEngine(swapEngine);

        std::cout<<"performCalc, noCoupons " << cmsLeg1b.size() << " spread " << spread << " marketVal "<<marketValue()<<", modelVal " << cap_->NPV()<<std::endl;
    }

}

