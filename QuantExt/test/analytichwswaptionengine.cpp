/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include "utilities.hpp"

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <qle/models/cdsoptionhelper.hpp>
#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/crossassetanalytics.hpp>
#include <qle/models/crossassetanalyticsbase.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/crossassetmodelimpliedeqvoltermstructure.hpp>
#include <qle/models/crossassetmodelimpliedfxvoltermstructure.hpp>
#include <qle/models/dkimpliedyoyinflationtermstructure.hpp>
#include <qle/models/dkimpliedzeroinflationtermstructure.hpp>
#include <qle/models/eqbsconstantparametrization.hpp>
#include <qle/models/eqbsparametrization.hpp>
#include <qle/models/eqbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/models/gaussian1dcrossassetadaptor.hpp>
#include <qle/models/infdkparametrization.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>
#include <qle/models/parametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>
#include <qle/models/pseudoparameter.hpp>
#include <qle/pricingengines/blackcdsoptionengine.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/pricingengines/depositengine.hpp>
#include <qle/pricingengines/discountingcommodityforwardengine.hpp>
#include <qle/pricingengines/discountingcurrencyswapengine.hpp>
#include <qle/pricingengines/discountingequityforwardengine.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <qle/pricingengines/discountingswapenginemulticurve.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/pricingengines/oiccbasisswapengine.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>
#include <qle/models/hwconstantparametrization.hpp>
#include <qle/pricingengines/analytichwswaptionengine.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makeswaption.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/fdhullwhiteswaptionengine.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include <boost/make_shared.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>


using namespace QuantLib;
using namespace QuantExt;

namespace {
struct F : public qle::test::TopLevelFixture {
    F() { Settings::instance().evaluationDate() = Date(1, March, 2016); }
    ~F() {}
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_FIXTURE_TEST_SUITE(AnalyticHwSwaptionEngineTest, F)

BOOST_AUTO_TEST_CASE(testHwSwaptionPricing) {
    BOOST_TEST_MESSAGE("Testing analytic HW swaption engine in basic setup...");

    Date today = Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;
    
    Handle<YieldTermStructure> flatCurve(boost::make_shared<FlatForward>(today, 0.02, Actual360()));

    Date start = today + 1 * Years;
    Date end = start + 19 * Years;
    Schedule schedule(start, end, Period(Annual), NullCalendar(), Unadjusted, Unadjusted, DateGeneration::Forward, false);
    Real nominal = 10000000;
    Rate fixedRate = 0.01;
    ext::shared_ptr<IborIndex> euriborIndex = boost::make_shared<Euribor6M>(flatCurve);
    VanillaSwap::Type swapType = VanillaSwap::Payer;
    ext::shared_ptr<VanillaSwap> underlyingSwap = boost::make_shared<VanillaSwap>(swapType, nominal, schedule, fixedRate, Actual360(), schedule, euriborIndex, 0.0, euriborIndex->dayCounter());
    Date expiry = start - 1 * Days;
    ext::shared_ptr<Exercise> europeanExercise(new EuropeanExercise(expiry));
    Swaption swaption(underlyingSwap, europeanExercise);

    Matrix sigma(1, 1, 0.01);  
    Array kappa(1, 0.01);  
    ext::shared_ptr<IrHwParametrization> irhw = boost::make_shared<IrHwConstantParametrization>(EURCurrency(), flatCurve, sigma, kappa);
    ext::shared_ptr<HwModel> hwModel = boost::make_shared<HwModel>(irhw);

    Array times(1000);
    for (int i = 0; i < 1000; i++) {
        times[i] = (i+1) * 0.001;
    }

    ext::shared_ptr<PricingEngine> hw_engine = boost::make_shared<AnalyticHwSwaptionEngine>(times, swaption, hwModel, flatCurve);
    swaption.setPricingEngine(hw_engine);

    // check fixedLeg_
    const auto& fixedLeg = underlyingSwap->fixedLeg();
    for (const auto& cf : fixedLeg) {
        auto fixedCoupon = boost::dynamic_pointer_cast<FixedRateCoupon>(cf);
        if (fixedCoupon) {
            //std::cout << "This is in the test unit!!" << std::endl;
            BOOST_CHECK_EQUAL(fixedCoupon->nominal(), nominal);
            BOOST_CHECK_CLOSE(fixedCoupon->rate(), fixedRate, 1e-12);
            BOOST_TEST_MESSAGE("Coupon date: " << fixedCoupon->date() << ", rate: " << fixedCoupon->rate());
            //std::cout << "Payment Date: " << cf->date() << std::endl;
        } else {
            BOOST_ERROR("Expected a FixedRateCoupon but got something else.");
        }
    }

    Real npvAnalyticHw = swaption.NPV();

    BOOST_TEST_MESSAGE("NPV: " << npvAnalyticHw);    
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
