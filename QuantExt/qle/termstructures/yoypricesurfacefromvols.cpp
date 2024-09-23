/*
 Copyright (C) 2024 AcadiaSoft Inc.
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
#include <ql/instruments/makeyoyinflationcapfloor.hpp>
#include <ql/pricingengines/inflation/inflationcapfloorengines.hpp>
#include <qle/termstructures/interpolatedyoycapfloortermpricesurface.hpp>
#include <qle/termstructures/yoypricesurfacefromvols.hpp>
namespace QuantExt {
using QuantLib::Bilinear;
using QuantLib::Linear;

QuantLib::ext::shared_ptr<YoYCapFloorTermPriceSurface> YoYPriceSurfaceFromVolatilities::operator()(
    const QuantLib::ext::shared_ptr<QuantLib::CapFloorTermVolSurface>& volSurface,
    const QuantLib::ext::shared_ptr<YoYInflationIndex>& index,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& nominalTs, QuantLib::VolatilityType type,
    QuantLib::Real displacement) {

    Calendar cal = index->yoyInflationTermStructure()->calendar();
    Period obsLag = index->yoyInflationTermStructure()->observationLag();
    Size settDays = volSurface->settlementDays();
    DayCounter dc = index->yoyInflationTermStructure()->dayCounter();
    BusinessDayConvention bdc = volSurface->businessDayConvention();
    Frequency frequency = index->frequency();

    std::vector<Rate> strikes = volSurface->strikes();
    std::vector<Period> terms = volSurface->optionTenors();
    std::vector<Time> times = volSurface->optionTimes();

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
            Time t = volSurface->timeFromReference(volSurface->optionDateFromTenor(optionletTerms[i]));

            Real vol = volSurface->volatility(t, strikes[j]);
            QuantLib::ext::shared_ptr<ConstantYoYOptionletVolatility> ovs(
                new ConstantYoYOptionletVolatility(vol, settDays, cal, bdc, dc, obsLag, frequency, false, -1.0, 3.0));
            QuantLib::ext::shared_ptr<PricingEngine> pe;
            Handle<QuantLib::YoYOptionletVolatilitySurface> hovs(ovs);
            if (type == ShiftedLognormal) {
                if (displacement == 0.0) {
                    pe = QuantLib::ext::make_shared<QuantLib::YoYInflationBlackCapFloorEngine>(index, hovs, nominalTs);
                } else {
                    pe = QuantLib::ext::make_shared<QuantLib::YoYInflationUnitDisplacedBlackCapFloorEngine>(index, hovs,
                                                                                                            nominalTs);
                }
            } else if (type == Normal) {
                pe = QuantLib::ext::make_shared<QuantLib::YoYInflationBachelierCapFloorEngine>(index, hovs, nominalTs);
            } else {
                QL_FAIL("unknown volatility type: " << type);
            }
            // calculate the cap price
            YoYInflationCapFloor cap = YoYInflationCapFloor(
                MakeYoYInflationCapFloor(YoYInflationCapFloor::Cap, index, optionletTerms[i].length(), cal, obsLag)
                    .withStrike(strikes[j])
                    .withPricingEngine(pe)
                    .withNominal(10000));
            cPrice[j][i] = cap.NPV();
            // floor price
            YoYInflationCapFloor floor = YoYInflationCapFloor(
                MakeYoYInflationCapFloor(YoYInflationCapFloor::Floor, index, optionletTerms[i].length(), cal, obsLag)
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

    Rate baseRate = index->yoyInflationTermStructure()->baseRate();

    auto yoySurface = QuantLib::ext::make_shared<QuantExt::InterpolatedYoYCapFloorTermPriceSurface<Bilinear, Linear>>(
        settDays, obsLag, index, baseRate, nominalTs, dc, cal, bdc, cStrikes, fStrikes, optionletTerms, cPriceFinal,
        fPriceFinal);
    yoySurface->enableExtrapolation();
    return yoySurface;
}

} // namespace QuantExt