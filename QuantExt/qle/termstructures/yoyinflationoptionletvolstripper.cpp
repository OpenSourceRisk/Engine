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

#include <ql/experimental/inflation/interpolatedyoyoptionletstripper.hpp>
#include <ql/instruments/makeyoyinflationcapfloor.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <qle/termstructures/interpolatedyoycapfloortermpricesurface.hpp>
#include <qle/termstructures/kinterpolatedyoyoptionletvolatilitysurface.hpp>
#include <qle/termstructures/yoyinflationoptionletvolstripper.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using boost::shared_ptr;

namespace QuantExt {

YoYInflationOptionletVolStripper::YoYInflationOptionletVolStripper(
    const boost::shared_ptr<CapFloorTermVolSurface>& volSurface, const boost::shared_ptr<YoYInflationIndex>& index,
    const Handle<YieldTermStructure>& nominalTs, VolatilityType type, Real displacement)
    : volSurface_(volSurface), yoyIndex_(index), nominalTs_(nominalTs), type_(type), displacement_(displacement) {

    performCalculations();
}

void YoYInflationOptionletVolStripper::performCalculations() {

    Calendar cal = yoyIndex_->yoyInflationTermStructure()->calendar();
    Period obsLag = yoyIndex_->yoyInflationTermStructure()->observationLag();
    Size settDays = volSurface_->settlementDays();
    DayCounter dc = yoyIndex_->yoyInflationTermStructure()->dayCounter();
    BusinessDayConvention bdc = volSurface_->businessDayConvention();

    Frequency frequency = yoyIndex_->frequency();

    std::vector<Rate> strikes = volSurface_->strikes();
    std::vector<Period> terms = volSurface_->optionTenors();
    std::vector<Time> times = volSurface_->optionTimes();

    std::vector<Period> optionletTerms;
    optionletTerms.push_back(terms.front());
    while (optionletTerms.back() != terms.back()) {
        optionletTerms.push_back(optionletTerms.back() + Period(1, Years));
    }

    Matrix cPrice(strikes.size(), strikes.size() == 0 ? 0 : optionletTerms.size(), Null<Real>()),
        fPrice(strikes.size(), strikes.size() == 0 ? 0 : optionletTerms.size(), Null<Real>());

    // populate cPrice
    for (Size i = 0; i < optionletTerms.size(); i++) {
        for (Size j = 0; j < strikes.size(); j++) {
            Time t = volSurface_->timeFromReference(volSurface_->optionDateFromTenor(optionletTerms[i]));

            Real vol = volSurface_->volatility(t, strikes[j]);
            boost::shared_ptr<ConstantYoYOptionletVolatility> ovs(
                new ConstantYoYOptionletVolatility(vol, settDays, cal, bdc, dc, obsLag, frequency, false, -1.0, 3.0));
            boost::shared_ptr<PricingEngine> pe;
            Handle<QuantLib::YoYOptionletVolatilitySurface> hovs(ovs);
            if (type_ == ShiftedLognormal) {
                if (displacement_ == 0.0) {
                    pe = boost::make_shared<YoYInflationBlackCapFloorEngine>(yoyIndex_, hovs, nominalTs_);
                } else {
                    pe = boost::make_shared<YoYInflationUnitDisplacedBlackCapFloorEngine>(yoyIndex_, hovs, nominalTs_);
                }
            } else if (type_ == Normal) {
                pe = boost::make_shared<YoYInflationBachelierCapFloorEngine>(yoyIndex_, hovs, nominalTs_);
            } else {
                QL_FAIL("unknown volatility type: " << type_);
            }
            // calculate the cap price
            YoYInflationCapFloor cap = YoYInflationCapFloor(
                MakeYoYInflationCapFloor(YoYInflationCapFloor::Cap, yoyIndex_, optionletTerms[i].length(), cal, obsLag)
                    .withStrike(strikes[j])
                    .withPricingEngine(pe)
                    .withNominal(10000));
            cPrice[j][i] = cap.NPV();
            // floor price
            YoYInflationCapFloor floor =
                YoYInflationCapFloor(MakeYoYInflationCapFloor(YoYInflationCapFloor::Floor, yoyIndex_,
                                                              optionletTerms[i].length(), cal, obsLag)
                                         .withStrike(strikes[j])
                                         .withPricingEngine(pe)
                                         .withNominal(10000));
            fPrice[j][i] = floor.NPV();
        }
    }

    // switch between floors and caps using the last option maturity, but keep one floor and one cap strike at least
    // this is the best we can do to use OTM instruments with the original QL yoy vol bootstrapper
    int jCritical = 0;
    while (jCritical < static_cast<int>(strikes.size()) &&
           fPrice[jCritical][optionletTerms.size() - 1] < cPrice[jCritical][optionletTerms.size() - 1])
        ++jCritical;
    int numberOfFloors = std::max(1, jCritical - 1);
    int numberOfCaps = std::max(1, static_cast<int>(strikes.size()) - jCritical + 1);
    Matrix cPriceFinal(numberOfCaps, strikes.size() == 0 ? 0 : optionletTerms.size());
    Matrix fPriceFinal(numberOfFloors, strikes.size() == 0 ? 0 : optionletTerms.size());
    std::vector<Real> fStrikes, cStrikes;
    for (int j = 0; j < numberOfCaps; ++j) {
        for (int i = 0; i < static_cast<int>(optionletTerms.size()); ++i) {
            cPriceFinal(j, i) = cPrice[strikes.size() - numberOfCaps + j][i];
        }
        cStrikes.push_back(strikes[strikes.size() - numberOfCaps + j]);
    }
    for (int j = 0; j < numberOfFloors; ++j) {
        for (int i = 0; i < static_cast<int>(optionletTerms.size()); ++i) {
            fPriceFinal(j, i) = fPrice[j][i];
        }
        fStrikes.push_back(strikes[j]);
    }

    Rate baseRate = yoyIndex_->yoyInflationTermStructure()->baseRate();
    QuantExt::InterpolatedYoYCapFloorTermPriceSurface<Bilinear, Linear> ys(settDays, obsLag, yoyIndex_, baseRate,
                                                                           nominalTs_, dc, cal, bdc, cStrikes, fStrikes,
                                                                           optionletTerms, cPriceFinal, fPriceFinal);

    boost::shared_ptr<QuantExt::InterpolatedYoYCapFloorTermPriceSurface<Bilinear, Linear> > yoySurface =
        boost::make_shared<QuantExt::InterpolatedYoYCapFloorTermPriceSurface<Bilinear, Linear> >(ys);
    yoySurface->enableExtrapolation();

    boost::shared_ptr<InterpolatedYoYOptionletStripper<Linear> > yoyStripper =
        boost::make_shared<InterpolatedYoYOptionletStripper<Linear> >();

    // Create an empty volatlity surface to pass to the engine
    boost::shared_ptr<QuantLib::YoYOptionletVolatilitySurface> ovs =
        boost::dynamic_pointer_cast<QuantLib::YoYOptionletVolatilitySurface>(
            boost::make_shared<QuantLib::ConstantYoYOptionletVolatility>(0.0, settDays, cal, bdc, dc, obsLag, frequency,
                                                                         yoySurface->indexIsInterpolated()));
    Handle<QuantLib::YoYOptionletVolatilitySurface> hovs(ovs);

    boost::shared_ptr<YoYInflationBachelierCapFloorEngine> cfEngine =
        boost::make_shared<YoYInflationBachelierCapFloorEngine>(yoyIndex_, hovs, nominalTs_);

    boost::shared_ptr<QuantExt::KInterpolatedYoYOptionletVolatilitySurface<Linear> > interpVolSurface =
        boost::make_shared<QuantExt::KInterpolatedYoYOptionletVolatilitySurface<Linear> >(
            settDays, cal, bdc, dc, obsLag, yoySurface, cfEngine, yoyStripper, 0);
    interpVolSurface->enableExtrapolation();
    boost::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> newSurface =
        boost::make_shared<QuantExt::YoYOptionletVolatilitySurface>(interpVolSurface, type_, displacement_);
    yoyOptionletVolSurface_ = newSurface;
}

} // namespace QuantExt
