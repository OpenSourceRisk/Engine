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


        Date startDate = asof_;
        Date endDate;

        //Compute ATM swap rates
        Real spread, rate1, rate2;

        std::vector<Real> nominals(1,1.0);

     {   
        boost::shared_ptr<PricingEngine> swapEngine(
                             new DiscountingSwapEngine(index1_->discountingTermStructure(), false));
     
        Calendar calendar = index1_->fixingCalendar();
        
        boost::shared_ptr<IborIndex> index = index1_->iborIndex();
        endDate = calendar.advance(startDate, length_,
                                      index->businessDayConvention());
        

        
        //endDate = index1_->maturityDate(asof_);
        Schedule cmsSchedule(startDate, endDate, index->tenor(), calendar,
                               index->businessDayConvention(),
                               index->businessDayConvention(),
                               DateGeneration::Forward, false);
        
        Leg cmsLeg = CmsLeg(cmsSchedule, index1_)
                    .withNotionals(nominals)
                    .withPaymentAdjustment(index1_->iborIndex()->businessDayConvention())
                    .withPaymentDayCounter(index1_->iborIndex()->dayCounter())
                    .withFixingDays(0);
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
        endDate = calendar.advance(startDate, length_,
                                       index->businessDayConvention());
        

        
        //endDate = index2_->maturityDate(asof_);
        Schedule cmsSchedule(startDate, endDate, index->tenor(), calendar,
                               index->businessDayConvention(),
                               index->businessDayConvention(),
                               DateGeneration::Forward, false);
        
        Leg cmsLeg = CmsLeg(cmsSchedule, index2_)
                    .withNotionals(nominals)
                    .withPaymentAdjustment(index1_->iborIndex()->businessDayConvention())
                    .withPaymentDayCounter(index1_->iborIndex()->dayCounter())
                    .withFixingDays(0);
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
                                    "CMSSpread_" + index1_->familyName() + "_" + index2_->familyName(), index1_, index2_);
        
        Schedule cmsSpreadSchedule(startDate, endDate, Period(3, Months), calendar,
                               index->businessDayConvention(),
                               index->businessDayConvention(),
                               DateGeneration::Forward, false);
        Leg cmsLeg1 = CmsSpreadLeg(cmsSpreadSchedule, spreadIndex)
                    .withNotionals(nominals)
                    .withSpreads(std::vector<Rate>(1, 0))
                    .withPaymentAdjustment(index->businessDayConvention())
                    .withPaymentDayCounter(Actual360())
                    .withFixingDays(0)
                    .inArrears(true)
                    .withCaps(std::vector<Rate>(1, spread));

        QuantLib::setCouponPricer(cmsLeg1, pricer_);

        /*cmsLeg1 = StrippedCappedFlooredCouponLeg(cmsLeg1);
        for(Size i = 0; i < cmsLeg1.size(); i++) {
            boost::shared_ptr<StrippedCappedFlooredCoupon> sc = boost::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(cmsLeg1[i]);
            if(sc != nullptr)
                sc->registerWith(sc->underlying());
        }*/

        legs.push_back(cmsLeg1);
        legPayers.push_back(false);
        
        Leg cmsLeg2 = CmsSpreadLeg(cmsSchedule, spreadIndex)
                    .withNotionals(nominals)
                    .withPaymentAdjustment(index->businessDayConvention())
                    .withPaymentDayCounter(Actual360())
                    .withFixingDays(0);

        QuantLib::setCouponPricer(cmsLeg2, pricer_);
        legs.push_back(cmsLeg2);
        legPayers.push_back(true);

        cap_ = boost::make_shared<QuantLib::Swap>(legs, legPayers);

        cap_->setPricingEngine(swapEngine);

        std::cout<<"performCalc, marketVal "<<marketValue()<<", modelVal " << cap_->NPV()<<std::endl;
    }

}
