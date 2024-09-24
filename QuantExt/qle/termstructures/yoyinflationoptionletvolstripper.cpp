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

#include <boost/make_shared.hpp>
#include <iostream>
#include <ql/experimental/inflation/interpolatedyoyoptionletstripper.hpp>
#include <ql/instruments/makeyoyinflationcapfloor.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/interpolatedyoycapfloortermpricesurface.hpp>
#include <qle/termstructures/kinterpolatedyoyoptionletvolatilitysurface.hpp>
#include <qle/termstructures/yoyinflationoptionletvolstripper.hpp>
#include <qle/termstructures/yoyoptionletsolver.hpp>
#include <qle/termstructures/yoyoptionletsurfacestripper.hpp>
#include <qle/termstructures/yoypricesurfacefromvols.hpp>
using namespace QuantLib;
using QuantLib::ext::shared_ptr;

namespace QuantExt {

YoYInflationOptionletVolStripper::YoYInflationOptionletVolStripper(
    const QuantLib::ext::shared_ptr<CapFloorTermVolSurface>& volSurface,
    const QuantLib::ext::shared_ptr<YoYInflationIndex>& index, const Handle<YieldTermStructure>& nominalTs,
    VolatilityType type, Real displacement, QuantLib::Real accuracy, QuantLib::Real globalAccuracy, bool dontThrow,
    QuantLib::Size maxAttempts, QuantLib::Real maxFactor, QuantLib::Real minFactor, QuantLib::Size dontThrowSteps)
    : volSurface_(volSurface), yoyIndex_(index), nominalTs_(nominalTs), type_(type), displacement_(displacement),
      dontThrow_(dontThrow), dontThrowSteps_(dontThrowSteps) {
    performCalculations();
}

void YoYInflationOptionletVolStripper::performCalculations() {

    YoYPriceSurfaceFromVolatilities volToPriceConverter;

    auto priceSurface = volToPriceConverter(volSurface_, yoyIndex_, nominalTs_, type_, displacement_);

    YoYOptionletSurfaceStripper optionletStripper;
    yoyOptionletVolSurface_ = optionletStripper(priceSurface, yoyIndex_, nominalTs_, accuracy_, globalAccuracy_,
                                                maxAttempts_, maxFactor_, minFactor_, dontThrow_, dontThrowSteps_);
}

} // namespace QuantExt
