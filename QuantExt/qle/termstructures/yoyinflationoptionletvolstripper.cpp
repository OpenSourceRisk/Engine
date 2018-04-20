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

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/experimental/inflation/interpolatedyoyoptionletstripper.hpp>
#include <ql/experimental/inflation/kinterpolatedyoyoptionletvolatilitysurface.hpp>
#include <ql/instruments/makeyoyinflationcapfloor.hpp>
#include <qle/termstructures/yoyinflationoptionletvolstripper.hpp>
#include <qle/termstructures/interpolatedyoycapfloortermpricesurface.hpp>

#include <boost/make_shared.hpp>

using boost::shared_ptr;

namespace QuantExt {

    YoYInflationOptionletVolStripper::YoYInflationOptionletVolStripper(
        const boost::shared_ptr<CapFloorTermVolSurface>& volSurface,
        const boost::shared_ptr<YoYInflationIndex>& index,
        const Handle<YieldTermStructure>& nominalTs,
        const VolatilityType type, const Real displacement) :
    volSurface_(volSurface), yoyIndex_(index), nominalTs_(nominalTs), 
        type_(type), displacement_(displacement){
        performCalculations();
    }

    void YoYInflationOptionletVolStripper::performCalculations() {

        Calendar cal = yoyIndex_->yoyInflationTermStructure()->calendar();
        Period obsLag = yoyIndex_->yoyInflationTermStructure()->observationLag();
        Size settDays = volSurface_->settlementDays();
        DayCounter dc = yoyIndex_->yoyInflationTermStructure()->dayCounter();
        BusinessDayConvention bdc = volSurface_->businessDayConvention();

        Frequency frequency = yoyIndex_->frequency();

        std::vector<Real> strikes = volSurface_->strikes();
        std::vector<Period> terms = volSurface_->optionTenors();
        std::vector<Time> times = volSurface_->optionTimes();
        Matrix cPrice(strikes.size(), strikes.size() == 0 ? 0 : terms.size(), Null<Real>()), 
               fPrice(strikes.size(), strikes.size() == 0 ? 0 : terms.size(), Null<Real>());

        // populate cPrice
        for (Size i = 0; i < terms.size(); i++) {
            for (Size j = 0; j < strikes.size(); j++) {
                Real vol = volSurface_->volatility(times[i], strikes[j]);
                boost::shared_ptr<ConstantYoYOptionletVolatility> ovs (
                    new ConstantYoYOptionletVolatility(vol, settDays,
                        cal, bdc, dc, obsLag, frequency, false, -1.0, 3.0));
                boost::shared_ptr<PricingEngine> pe;
                Handle<QuantLib::YoYOptionletVolatilitySurface> hovs(ovs);
                if (type_ == ShiftedLognormal) {
                    if (displacement_ == 0.0) {
                        pe = boost::make_shared<YoYInflationBlackCapFloorEngine>(yoyIndex_, hovs);
                    }
                    else {
                        pe = boost::make_shared<YoYInflationUnitDisplacedBlackCapFloorEngine>(yoyIndex_, hovs);
                    }
                }
                else if (type_ == Normal) {
                    pe = boost::make_shared<YoYInflationBachelierCapFloorEngine>(yoyIndex_, hovs);
                } else {
                    QL_FAIL("unknown volatility type: " << type_);
                }
                // calculate the cap price
                YoYInflationCapFloor cap(MakeYoYInflationCapFloor(
                    YoYInflationCapFloor::Type::Cap, terms[i].length(), cal, 
                    yoyIndex_, obsLag, strikes[j])
                    .withPricingEngine(pe));
                cPrice[j][i] = cap.NPV();
                // floor price
                YoYInflationCapFloor floor(MakeYoYInflationCapFloor(
                    YoYInflationCapFloor::Type::Floor, terms[i].length(), cal,
                    yoyIndex_, obsLag, strikes[j])
                    .withPricingEngine(pe));
                fPrice[j][i] = floor.NPV();
            }
        }

        boost::shared_ptr<QuantExt::InterpolatedYoYCapFloorTermPriceSurface
            <QuantLib::Bilinear, QuantLib::Linear>> yoySurface =
            boost::make_shared<QuantExt::InterpolatedYoYCapFloorTermPriceSurface<QuantLib::Bilinear, QuantLib::Linear>>(
                0, obsLag, yoyIndex_, yoyIndex_->yoyInflationTermStructure()->baseRate(), nominalTs_,
                dc, cal, bdc, strikes, strikes, terms, cPrice, fPrice);

        std::vector<Period> optionletTerms = { yoySurface->maturities().front() };
        while (optionletTerms.back() != terms.back()) {
            optionletTerms.push_back(optionletTerms.back() + Period(1, Years));
        }
        yoySurface->setMaturities(optionletTerms);

        boost::shared_ptr<InterpolatedYoYOptionletStripper<QuantLib::Linear>> yoyStripper =
            boost::make_shared<InterpolatedYoYOptionletStripper<QuantLib::Linear>>();

        // Create an empty volatlity surface to pass to the engine
        boost::shared_ptr<QuantLib::YoYOptionletVolatilitySurface> ovs =
            boost::dynamic_pointer_cast<QuantLib::YoYOptionletVolatilitySurface>(
                boost::make_shared<QuantLib::ConstantYoYOptionletVolatility>(
                    0.0, settDays, cal, bdc, dc, obsLag, frequency,
                    yoySurface->indexIsInterpolated()));
        Handle<QuantLib::YoYOptionletVolatilitySurface> hovs(ovs);
        
        boost::shared_ptr<YoYInflationBachelierCapFloorEngine> cfEngine =
            boost::make_shared<YoYInflationBachelierCapFloorEngine>(yoyIndex_, hovs);

        boost::shared_ptr<KInterpolatedYoYOptionletVolatilitySurface<Linear>> interpVolSurface =
            boost::make_shared<KInterpolatedYoYOptionletVolatilitySurface<Linear>>(
                settDays, cal, bdc, dc, obsLag, yoySurface, cfEngine, yoyStripper, 0);

        boost::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> newSurface =
            boost::make_shared<QuantExt::YoYOptionletVolatilitySurface>(interpVolSurface, type_, displacement_);
        yoyOptionletVolSurface_ = newSurface;
       
    }

} // namespace QuantExt
