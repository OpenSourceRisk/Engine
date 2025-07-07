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

//#include "utilities.hpp"
//
//#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <qle/models/cdsoptionhelper.hpp>
#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/crlgm1fparametrization.hpp>
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
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/lgm.hpp>
#include <qle/models/lgmimplieddefaulttermstructure.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>
#include <qle/models/parametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>
#include <qle/models/pseudoparameter.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/pricingengines/analyticdkcpicapfloorengine.hpp>
#include <qle/pricingengines/analyticlgmcdsoptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/analyticxassetlgmeqoptionengine.hpp>
#include <qle/pricingengines/blackcdsoptionengine.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/pricingengines/depositengine.hpp>
#include <qle/pricingengines/discountingcommodityforwardengine.hpp>
#include <qle/pricingengines/discountingcurrencyswapengine.hpp>
#include <qle/pricingengines/discountingequityforwardengine.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/pricingengines/oiccbasisswapengine.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makeswaption.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/fdhullwhiteswaptionengine.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/math/randomnumbers/boxmullergaussianrng.hpp>
#include <qle/models/hwconstantparametrization.hpp>
#include <qle/pricingengines/analytichwswaptionengine.hpp>
#include <boost/make_shared.hpp>

#include <iostream>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <map>
#include <random>
#include <string>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/indexes/ibor/euribor.hpp>


using namespace QuantLib;
using namespace QuantExt;
using namespace std;

//namespace {
//struct F : public qle::test::TopLevelFixture {
//    F() { Settings::instance().evaluationDate() = Date(20, March, 2019); }
//    ~F() {}
//};
//} // namespace
//
//BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)
//
//BOOST_FIXTURE_TEST_SUITE(Anl, F)

BOOST_AUTO_TEST_CASE(testUri){
    
    BOOST_TEST_MESSAGE("Initializing MC Simulation");
    // model params
    Size nFactors = 2;
    Array kappa = {0.03, 0.05}; // mean reversion speed
    Array sigma = {0.01, 0.015};
    Real flatRate = 0.02;
    // swap params
    Real maturity = 1.0;
    Real tenor = 5.0;
    Real strike = 0.025;
    // simulation params
    Size nPaths = 1;
    Size nSteps = 100;
    Time dt = maturity / nSteps;

    Real sumPayoffs = 0.0;

    MersenneTwisterUniformRng uniRng(42);
    BoxMullerGaussianRng<MersenneTwisterUniformRng> gaussianRng(uniRng);
    
    // logging factors
    std::ofstream myfile;
    myfile.open("output.csv");

    // loop through the paths
    for (Size p = 0; p < nPaths; ++p) {
        Array factors(nFactors, 0.0); // keep track of each factor evolution
        // loop through the steps
        for (Size s = 0; s < nSteps; ++s) {
            // model each of the factors x each step
            for (Size f = 0; f < nFactors; ++f) {
                Real dw = sqrt(dt) * gaussianRng.next().value;
                Real dx = (-kappa[f] * factors[f] * dt + sigma[f] * dw);
                factors[f] += dx;
            }
            myfile << factors[0] << "," << factors[1] << "\n";
        }
        cout << "Hello wordl" << endl;

        myfile.close();

        // compute the short rate at maturity
        // todo: this can be merged with the previous step
        Real shortRate = 0.0;
        shortRate += flatRate;
        cout << "The short rate should be the flat rate " << flatRate << endl;
        for (Size f = 0; f < nFactors; ++f) {
            shortRate += factors[f];
        }

        // compute swap value
        Real annuity = 0.0;
        Real floatLeg = 0.0;

        cout << "Getting the deltas" << endl;
        for (Size i = 1; i <= tenor; ++i) { // todo: review this loop
            cout << "The delta is " << i << endl;
            Real df = exp(-shortRate * i);
            annuity += df;
            floatLeg += 0.02 * (i + maturity);
        }
        cout << tenor << endl;
        Real fixLeg = annuity * strike;
        Real payoff = max(floatLeg - fixLeg, 0.0);
        sumPayoffs += payoff * exp(-shortRate * maturity);
    }

    Real expectedPayoff = sumPayoffs / nPaths;
    cout << "The expected payoff is: " << expectedPayoff << endl;
    cout << "Sum of payoffs: " << sumPayoffs << endl;
};

BOOST_AUTO_TEST_CASE(testHWEngine) {
    Real nFactors = 4;

    Array kappa = {1.18575, 0.0189524, 0.0601251, 0.0709152};
    Matrix sigma = {{-0.0122469, 0.0105949, 0, 0}, {0, 0, -0.117401, 0.122529}};

    Handle<YieldTermStructure> flatCurve(ext::make_shared<FlatForward>(0,
        NullCalendar(), 0.02, Actual365Fixed()));

    Currency currency = EURCurrency();

    QuantLib::ext::shared_ptr<IrHwConstantParametrization> params =
        QuantLib::ext::make_shared<IrHwConstantParametrization>(currency, flatCurve, sigma, kappa);

    ext::shared_ptr<HwModel> model = ext::make_shared<HwModel>(params);

    // discounting curve
    Handle<YieldTermStructure> discCurve(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    // forward (+10bp)
    Handle<YieldTermStructure> forwardCurve1(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0210, Actual365Fixed()));
    // forward (-10bp)
    Handle<YieldTermStructure> forwardCurve2(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0190, Actual365Fixed()));

    /*const QuantLib::ext::shared_ptr<IrLgm1fConstantParametrization> irlgm1f =
        QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), discCurve, 0.01, 0.01);*/

    // forward curve attached
    QuantLib::ext::shared_ptr<SwapIndex> index1 =
        QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardCurve1);
    QuantLib::ext::shared_ptr<SwapIndex> index2 =
        QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, forwardCurve2);

    Array times = {1.0, 5.0};

    Swaption swaption1 = MakeSwaption(index1, 10 * Years, 0.02);
    QuantLib::ext::shared_ptr<AnalyticHwSwaptionEngine> engine =
    QuantLib::ext::make_shared<AnalyticHwSwaptionEngine>(times, swaption1, model);

    cout << nFactors << endl;
    //engine->calculate();
    swaption1.setPricingEngine(engine);
    swaption1.NPV();
    // set the engine
    // 
    // run the NPV
    // then adjust


}

BOOST_AUTO_TEST_CASE(testHWEngineTwo) {
    Real nFactors = 4;
    cout << nFactors << endl;
    Calendar calendar = TARGET();
    Date today(24, June, 2025);
    Settings::instance().evaluationDate() = today;
    DayCounter dc = Actual365Fixed();

    Array kappa = {1.18575, 0.0189524, 0.0601251, 0.0709152};
    Matrix sigma = {{-0.0122469, 0.0105949, 0, 0}, {0, 0, -0.117401, 0.122529}};

    // discounting curve
    Handle<YieldTermStructure> discCurve(
        QuantLib::ext::make_shared<FlatForward>(today, 0.02, dc));

    Currency currency = EURCurrency();

    QuantLib::ext::shared_ptr<IrHwConstantParametrization> params =
        QuantLib::ext::make_shared<IrHwConstantParametrization>(currency, discCurve, sigma, kappa);

    ext::shared_ptr<HwModel> model = ext::make_shared<HwModel>(params);

    Period swaptionMaturity = 1 * Years;
    Period swapTenor = 5 * Years;
    Real strike = 0.02;
    ext::shared_ptr<SwapIndex> index = QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(swapTenor, discCurve);
    Swaption swaption = MakeSwaption(index, swaptionMaturity, strike);
    Array times = {0.01, 0.5, 1};
    //Array times(500);
   /* for (int i = 0; i < 500; i++) {
        times[i] = (i + 1) * 0.001;
    }*/
    QuantLib::ext::shared_ptr<AnalyticHwSwaptionEngine> engine =
        QuantLib::ext::make_shared<AnalyticHwSwaptionEngine>(times, swaption, model);
    swaption.setPricingEngine(engine);
    Real price = swaption.NPV();
    cout << "Price: " << price << endl;
}

BOOST_AUTO_TEST_CASE(testIntern) {
    Date today = Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    Handle<YieldTermStructure> flatCurve(boost::make_shared<FlatForward>(today, 0.02, Actual360()));

    Date start = today + 1 * Years;
    Date end = start + 19 * Years;
    Schedule schedule(start, end, Period(Annual), NullCalendar(), Unadjusted, Unadjusted, DateGeneration::Forward,
                      false);
    Real nominal = 10000000;
    Rate fixedRate = 0.01;
    ext::shared_ptr<IborIndex> euriborIndex = boost::make_shared<Euribor6M>(flatCurve);
    VanillaSwap::Type swapType = VanillaSwap::Payer;
    ext::shared_ptr<VanillaSwap> underlyingSwap = boost::make_shared<VanillaSwap>(
        swapType, nominal, schedule, fixedRate, Actual360(), schedule, euriborIndex, 0.0, euriborIndex->dayCounter());
    Date expiry = start - 1 * Days;
    ext::shared_ptr<Exercise> europeanExercise(new EuropeanExercise(expiry));
    Swaption swaption(underlyingSwap, europeanExercise);

    Matrix sigma(1, 1, 0.01);
    Array kappa(1, 0.01);
    ext::shared_ptr<IrHwParametrization> irhw =
        boost::make_shared<IrHwConstantParametrization>(EURCurrency(), flatCurve, sigma, kappa);
    ext::shared_ptr<HwModel> hwModel = boost::make_shared<HwModel>(irhw);

    Array times(1000);
    for (int i = 0; i < 1000; i++) {
        times[i] = (i + 1) * 0.001;
    }

    ext::shared_ptr<PricingEngine> hw_engine =
        boost::make_shared<AnalyticHwSwaptionEngine>(times, swaption, hwModel, flatCurve);
    swaption.setPricingEngine(hw_engine);

    // check fixedLeg_
    const auto& fixedLeg = underlyingSwap->fixedLeg();
    for (const auto& cf : fixedLeg) {
        auto fixedCoupon = boost::dynamic_pointer_cast<FixedRateCoupon>(cf);
        if (fixedCoupon) {
            // std::cout << "This is in the test unit!!" << std::endl;
            BOOST_CHECK_EQUAL(fixedCoupon->nominal(), nominal);
            BOOST_CHECK_CLOSE(fixedCoupon->rate(), fixedRate, 1e-12);
            BOOST_TEST_MESSAGE("Coupon date: " << fixedCoupon->date() << ", rate: " << fixedCoupon->rate());
            // std::cout << "Payment Date: " << cf->date() << std::endl;
        } else {
            BOOST_ERROR("Expected a FixedRateCoupon but got something else.");
        }
    }

    cout << "Hello I should be here " << endl;

    Real npvAnalyticHw = swaption.NPV();

    cout << "And the price is " << npvAnalyticHw << endl;

    BOOST_TEST_MESSAGE("NPV: " << npvAnalyticHw);
}



BOOST_AUTO_TEST_CASE(testMCSimulation) {
    Array kappa = {1.18575, 0.0189524, 0.0601251, 0.0709152};
    Matrix sigma = {{-0.0122469, 0.0105949, 0, 0}, {0, 0, -0.117401, 0.122529}};
    Size nPaths = 100;
    Time maturity = 1.0;
    Time tenor = 5.0;
    Rate strike  = 0.02;
    //Real notinal = 1.0;
    Size nSteps = 100;
    Time dt = 0.01;
    Rate flatRate = 0.02;

    Size nFactors = kappa.size();
    Size nBrownian = sigma.rows();
    cout << nFactors << " - " << nBrownian << endl;
    mt19937 rng(42);
    normal_distribution<> stdNormal(0.0, 1.0);
    Real sumPayoffs = 0.0;

    for (Size path = 0; path < nPaths; ++path) {
        Array factors(nFactors, 0.0);

        for (Size step = 0; step < nSteps; ++step) {
            vector<Real> dw(nBrownian);
            for (Size j = 0; j < nBrownian; ++j) {
                dw[j] = stdNormal(rng) * sqrt(dt);
            }

            for (Size i = 0; i < nFactors; ++i) {
                Real diffusion = 0.0;
                for (Size j = 0; j < nBrownian; ++j) {
                    diffusion += sigma[j][i] * dw[j];
                }
                // review this part
                Real drift = -kappa[i] * factors[i];
                factors[i] += drift * dt + diffusion;
            }
        }

        Real rT = 0.02;
      /*  Real rT = flatRate;
        Rate flatRate = 0.02;*/
        for (Size i = 0; i < nFactors; ++i) {
            rT += factors[i];
        }

        // swap value at exercise
        Real annuity = 0.0, floatLeg = 0.0;
        for (Size i = 1; i <= tenor; ++i) {
            //Time Ti = maturity + i;
            DiscountFactor df = exp(-rT * i);
            annuity += df;
            floatLeg += df * flatRate;
        }

        Real fixedLeg = strike * annuity;
        Real swapValue = floatLeg - fixedLeg;
        Real payoff = max(swapValue, 0.0);
        // discout back to t = 0;
        DiscountFactor df0T = exp(-flatRate * maturity);
        sumPayoffs += payoff * df0T;


    }

    Real NPV = sumPayoffs / nPaths;
    cout << "NPV: " << NPV << endl;
}
