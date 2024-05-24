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

#include <ql/exercise.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/instruments/commodityapo.hpp>
#include <qle/termstructures/aposurface.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

using namespace QuantLib;
using std::vector;

namespace QuantExt {

ApoFutureSurface::ApoFutureSurface(const Date& referenceDate, const vector<Real>& moneynessLevels,
                                   const QuantLib::ext::shared_ptr<CommodityIndex>& index,
                                   const Handle<PriceTermStructure>& pts, const Handle<YieldTermStructure>& yts,
                                   const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& expCalc,
                                   const Handle<BlackVolTermStructure>& baseVts,
                                   const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseExpCalc, Real beta,
                                   bool flatStrikeExtrapolation, const boost::optional<Period>& maxTenor)
    : BlackVolatilityTermStructure(referenceDate, baseVts->calendar(), baseVts->businessDayConvention(),
                                   baseVts->dayCounter()),
      index_(index), baseExpCalc_(baseExpCalc), vols_(moneynessLevels.size()) {

    // Checks.
    QL_REQUIRE(!pts.empty(), "The price term structure should not be empty.");
    QL_REQUIRE(!yts.empty(), "The yield term structure should not be empty.");
    QL_REQUIRE(expCalc, "The expiry calculator should not be null.");
    QL_REQUIRE(!baseVts.empty(), "The base volatility term structure should not be empty.");
    QL_REQUIRE(!index_->priceCurve().empty(), "The commodity index should have a base price curve.");
    QL_REQUIRE(baseExpCalc_, "The base expiry calculator should not be null.");

    // Register with dependent term structures.
    registerWith(pts);
    registerWith(yts);
    registerWith(baseVts);

    // Determine the maximum expiry of the APO surface that we will build
    Date maxDate;
    if (maxTenor) {
        // If maxTenor is provided, use it.
        maxDate = referenceDate + *maxTenor;
    } else {
        maxDate = baseVts->maxDate();
        if (maxDate == Date::maxDate() || maxDate == Date()) {
            maxDate = pts->maxDate();
            QL_REQUIRE(maxDate != Date::maxDate() && maxDate != Date(),
                       "Could not determine a maximum date for the ApoFutureSurface");
        }
    }
    QL_REQUIRE(maxDate > referenceDate, "Expected the max date, " << io::iso_date(maxDate)
                                                                  << ", to be greater than the reference date, "
                                                                  << io::iso_date(referenceDate) << ".");

    // Get the start and end dates of each APO that will be used to create the APO surface in the expiry direction.
    // Here, we use the expiry calculator, expCalc. This expiry calculator will generally be from the corresponding
    // averaging future contracts. We will be using this APO surface to price these averaging futures as standard
    // non-averaging commodity options. The averaging future contract expiry will equal the APO expiry.
    apoDates_ = { expCalc->priorExpiry(true, referenceDate) };
    vector<Time> apoTimes;
    while (apoDates_.back() < maxDate) {
        apoDates_.push_back(expCalc->nextExpiry(false, apoDates_.back()));
        apoTimes.push_back(timeFromReference(apoDates_.back()));
    }

    // Spot quote based on the price curve and adapted yield term structure
    Handle<Quote> spot(QuantLib::ext::make_shared<DerivedPriceQuote>(pts));
    Handle<YieldTermStructure> pyts =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<PriceTermStructureAdapter>(*pts, *yts));
    pyts->enableExtrapolation();

    // Hard-code this to false.
    bool stickyStrike = false;

    // Initialise the matrix of quotes for use in vts_. These will be populated/updated in performCalculations by
    // pricing all of the APOs and extracting the volatility from each of them. Rows are moneyness levels and columns
    // are expiry times in the matrix of quotes that are fed to vts_'s ctor.
    vector<vector<Handle<Quote> > > vols(moneynessLevels.size());
    for (Size i = 0; i < moneynessLevels.size(); i++) {
        for (Size j = 0; j < apoTimes.size(); j++) {
            vols_[i].push_back(QuantLib::ext::make_shared<SimpleQuote>(0.0));
            vols[i].push_back(Handle<Quote>(vols_[i].back()));
        }
    }

    // Initialise the underlying helping volatility structure.
    vts_ = QuantLib::ext::make_shared<BlackVarianceSurfaceMoneynessForward>(calendar_, spot, apoTimes, moneynessLevels, vols,
                                                                    baseVts->dayCounter(), pyts, yts, stickyStrike,
                                                                    flatStrikeExtrapolation);

    vts_->enableExtrapolation();

    // Initialise the engine for performing the APO valuations.
    apoEngine_ = QuantLib::ext::make_shared<CommodityAveragePriceOptionAnalyticalEngine>(yts, baseVts, beta);
}

Date ApoFutureSurface::maxDate() const { return vts_->maxDate(); }

Real ApoFutureSurface::minStrike() const { return vts_->minStrike(); }

Real ApoFutureSurface::maxStrike() const { return vts_->maxStrike(); }

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

    // Quantity is 1.0 everywhere below.
    Real quantity = 1.0;

    for (Size j = 1; j < apoDates_.size(); j++) {

        // Set up the APO cashflow with from apoDates_[j-1] to apoDates_[j]
        auto cf =
            QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(quantity, apoDates_[j - 1], apoDates_[j], apoDates_[j],
                                                                index_, Calendar(), 0.0, 1.0, true, 0, 0, baseExpCalc_);

        // The APO cashflow forward rate is the amount
        Real forward = cf->amount();

        // Some of the sigmas will be Null<Real>() in the first APO period if the accrued is already greater than the
        // strike. So store the sigmas in a vector in the first step and then update the quotes as a second step.
        // idx will point to the first element in sigmas not equal to Null<Real>().
        vector<Real> sigmas(vts_->moneyness().size());
        for (Size i = 0; i < vts_->moneyness().size(); i++) {

            // "exercise date" is just the last date of the APO cashflow.
            auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(apoDates_[j]);

            // Apply moneyness to forward rate to get current strike
            Real strike = vts_->moneyness()[i] * forward;

            // Create the APO and call NPV() to trigger population of results.
            CommodityAveragePriceOption apo(cf, exercise, 1.0, strike, Option::Call);
            apo.setPricingEngine(apoEngine_);
            apo.NPV();

            auto it = apo.additionalResults().find("sigma");
            if (it != apo.additionalResults().end())
                sigmas[i] = boost::any_cast<Real>(it->second);
            else
                sigmas[i] = Null<Real>();
        }

        QL_REQUIRE(sigmas.back() != Null<Real>(), "All of the sigmas are null.");

        for (Size i = vts_->moneyness().size(); i > 0; i--) {

            // Know already that the last sigma is not null.
            if (sigmas[i - 1] == Null<Real>())
                sigmas[i - 1] = sigmas[i];

            // Update the quote
            vols_[i - 1][j - 1]->setValue(sigmas[i - 1]);
        }
    }
}

const QuantLib::ext::shared_ptr<BlackVarianceSurfaceMoneyness>& ApoFutureSurface::vts() const { return vts_; }

Volatility ApoFutureSurface::blackVolImpl(Time t, Real strike) const {
    calculate();
    return vts_->blackVol(t, strike, true);
}

} // namespace QuantExt
