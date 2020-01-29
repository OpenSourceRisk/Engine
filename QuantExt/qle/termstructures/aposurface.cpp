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

#include <qle/termstructures/aposurface.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

using namespace QuantLib;
using std::vector;

namespace QuantExt {

ApoFutureSurface::ApoFutureSurface(const Date& referenceDate,
    const vector<Real>& moneynessLevels,
    const Handle<PriceTermStructure>& pts,
    const Handle<YieldTermStructure>& yts,
    const boost::shared_ptr<FutureExpiryCalculator>& expCalc,
    const Handle<BlackVolTermStructure>& baseVts,
    const Handle<PriceTermStructure>& basePts,
    const boost::shared_ptr<FutureExpiryCalculator>& baseExpCalc,
    bool flatStrikeExtrapolation,
    const boost::optional<Period>& maxTenor)
    : BlackVolatilityTermStructure(referenceDate, baseVts->calendar(), baseVts->businessDayConvention(),
        baseVts->dayCounter()), pts_(pts), yts_(yts), expCalc_(expCalc), baseVts_(baseVts), basePts_(basePts),
        baseExpCalc_(baseExpCalc), maxTenor_(maxTenor), vols_(moneynessLevels.size()) {

    // Set up the underlying forward moneyness variance surface

    // Determine the maximum expiry of the APO surface that we will build
    Date maxDate;
    if (maxTenor_) {
        // If maxTenor is provided, use it.
        maxDate = referenceDate + *maxTenor_;
    } else {
        maxDate = baseVts->maxDate();
        if (maxDate == Date::maxDate() || maxDate == Date()) {
            maxDate = pts->maxDate();
            QL_REQUIRE(maxDate != Date::maxDate() && maxDate != Date(),
                "Could not determine a maximum date for the ApoFutureSurface");
        }
    }
    QL_REQUIRE(maxDate > referenceDate, "Expected the max date, " << io::iso_date(maxDate) <<
        ", to be greater than the reference date, " << io::iso_date(referenceDate) << ".");

    // Get the start and end dates of each APO that will be used to create the APO surface in the expiry direction. 
    // Here, we use the expiry calculator, expCalc_. This expiry calculator will generally be from the corresponding
    // averaging future contracts. We will be using this APO surface to price these averaging futures as standard
    // non-averaging commodity options. The averaging future contract expiry will equal the APO expiry.
    apoDates_ = { expCalc_->priorExpiry(true, referenceDate) };
    vector<Time> apoTimes;
    while (apoDates_.back() < maxDate) {
        apoDates_.push_back(expCalc_->nextExpiry(false, apoDates_.back()));
        apoTimes.push_back(timeFromReference(apoDates_.back()));
    }

    // Spot quote based on the price curve and adatped yield term structure
    Handle<Quote> spot(boost::make_shared<DerivedPriceQuote>(pts));
    Handle<YieldTermStructure> pyts = Handle<YieldTermStructure>(
        boost::make_shared<PriceTermStructureAdapter>(*pts_, *yts_));
    pyts->enableExtrapolation();

    // Hard-code this to false.
    bool stickyStrike = false;

    // Initialise the matrix of quotes for use in vts_. These will be populated/updated in performCalculations by 
    // pricing all of the APOs and extracting the volatility from each of them. Rows are moneyness levels and columns 
    // are expiry times in the matrix of quotes that are fed to vts_'s ctor.
    vector<vector<Handle<Quote>>> vols(moneynessLevels.size());
    for (Size i = 0; i < moneynessLevels.size(); i++) {
        for (Size j = 0; j < apoTimes.size(); j++) {
            vols_[i].push_back(boost::make_shared<SimpleQuote>(0.0));
            vols[i].push_back(Handle<Quote>(vols_[i].back()));
        }
    }

    // Initialise the underlying helping volatility structure.
    vts_ = boost::make_shared<BlackVarianceSurfaceMoneynessForward>(calendar_, spot, apoTimes,
        moneynessLevels, vols, baseVts->dayCounter(), pyts, yts_, stickyStrike, flatStrikeExtrapolation);
    
    vts_->enableExtrapolation();
}

Date ApoFutureSurface::maxDate() const {
    return vts_->maxDate();
}

Real ApoFutureSurface::minStrike() const {
    return vts_->minStrike();
}

Real ApoFutureSurface::maxStrike() const {
    return vts_->maxStrike();
}

void ApoFutureSurface::accept(AcyclicVisitor& v) {
    if (auto v1 = dynamic_cast<Visitor<ApoFutureSurface>*>(&v))
        v1->visit(*this);
    else
        QL_FAIL("Not an ApoFutureSurface visitor");
}

void ApoFutureSurface::update() {
    TermStructure::update();
    LazyObject::update();
}

void ApoFutureSurface::performCalculations() const {

}

Volatility ApoFutureSurface::blackVolImpl(Time t, Real strike) const {
    return vts_->blackVol(t, strike, true);
}

}

