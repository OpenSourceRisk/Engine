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

#include "toplevelfixture.hpp"
#include "utilities.hpp"
// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <qle/indexes/equityindex.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/cdsoptionhelper.hpp>
#include <qle/models/cirppconstantfellerparametrization.hpp>
#include <qle/models/commodityschwartzmodel.hpp>
#include <qle/models/commodityschwartzparametrization.hpp>
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
#include <qle/models/jyimpliedzeroinflationtermstructure.hpp>
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
#include <qle/pricingengines/discountingswapenginemulticurve.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/pricingengines/oiccbasisswapengine.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>
#include <qle/processes/commodityschwartzstateprocess.hpp>
#include <qle/processes/crossassetstateprocess.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>
#include <qle/termstructures/pricecurve.hpp>

#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/ibor/gbplibor.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/instruments/cpicapfloor.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <ql/methods/montecarlo/multipathgenerator.hpp>
#include <ql/methods/montecarlo/pathgenerator.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/inflation/interpolatedzeroinflationcurve.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include <iostream>
#include <cmath>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <boost/make_shared.hpp>
// fix for boost 1.64, see https://lists.boost.org/Archives/boost/2016/11/231756.php
#if BOOST_VERSION >= 106400
#include <boost/serialization/array_wrapper.hpp>
#endif
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/covariance.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>
#include <boost/make_shared.hpp>


#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>


using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;
using namespace boost::accumulators;
namespace bdata = boost::unit_test::data;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(LgmPricingCases)

// Define helper functions for Black'76 test case. The source code needs
// this additional implementation to ensure an independent testing.
double blackCallPrice(double F, double K, double vol, double T, double r) {
    double d1 = (std::log(F / K) + 0.5 * vol * vol * T) / (vol * std::sqrt(T));
    double d2 = d1 - vol * std::sqrt(T);
    return std::exp(-r * T) * (F * 0.5 * erfc(-d1 / std::sqrt(2.0)) - K * 0.5 * erfc(-d2 / std::sqrt(2.0)));
}

double blackPutPrice(double F, double K, double vol, double T, double r) {
    double d1 = (std::log(F / K) + 0.5 * vol * vol * T) / (vol * std::sqrt(T));
    double d2 = d1 - vol * std::sqrt(T);
    return std::exp(-r * T) * (K *  0.5 * erfc(d2 / std::sqrt(2.0)) - F *  0.5 * erfc(d1 / std::sqrt(2.0)));
}

BOOST_AUTO_TEST_CASE(testBlackCase) {
    // The LGM model converes to the famous Black'76 model approach in case of a zero mean-reversion rate.
    // The dynamics defined via the underlying SDE lead to that relationship between the two models.
    
    BOOST_TEST_MESSAGE("Testing LGM pricing in cases equal to the Black76 model ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(10, July, 2016)); // T=1
    double T = 1.0;
    double vol = 0.005;
    Date startDate(15, July, 2016);
    Settings::instance().evaluationDate() = settlementDate;    
    Date maturityDate = calendar.advance(settlementDate, 5, Years);
    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts= QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed());
   
    std::vector<Date> volstepdates;
    volstepdates.push_back(Date(15, July, 2016));
    volstepdates.push_back(Date(15, July, 2017));
    volstepdates.push_back(Date(15, July, 2018));
    volstepdates.push_back(Date(15, July, 2019));
    volstepdates.push_back(Date(15, July, 2020));

    std::vector<Real> eurVols;

    auto  volsteptimes_a = Array(volstepdates.size());

    for (Size i = 0; i < volstepdates.size(); ++i) 
        volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
    
    for (Size i = 0; i < volstepdates.size() + 1; ++i) 
        eurVols.push_back(vol); // Alpha

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.00); // No mean reversion 

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    double Annuity=0.0;

    for (int i=2;i<10;++i)
        Annuity += 0.5 * exp(-(double) i * 0.5 *  fixedRate);

    // Starting 1% above market rate level of 2%
    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (double strike = 0.03; strike < 0.07; strike += 0.005)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        double limitValue = Annuity * blackPutPrice(fixedRate, strike, vol, T, fixedRate);
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK_CLOSE(npv, limitValue, 1.0); // Tolerance of 1%
    }

    // Starting 1% below market rate level of 2%
    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (double strike = 0.01; strike > 0.0; strike -= 0.005) //TODO go lower
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        double limitValue = Annuity * blackCallPrice(fixedRate, strike, vol, T, fixedRate);
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK_CLOSE(npv, limitValue, 3.0); // Tolerance of 3%
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
}

/* 
BOOST_AUTO_TEST_CASE(testLgmPricingVola) {

    BOOST_TEST_MESSAGE("Testing pricing with different volas ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Settings::instance().evaluationDate() = settlementDate;    
    Date maturityDate = calendar.advance(settlementDate, 5, Years);
    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts= QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed());
   
    std::vector<Date> volstepdates;
    volstepdates.push_back(Date(15, July, 2016));
    volstepdates.push_back(Date(15, July, 2017));
    volstepdates.push_back(Date(15, July, 2018));
    volstepdates.push_back(Date(15, July, 2019));
    volstepdates.push_back(Date(15, July, 2020));

    std::vector<Real> eurVols;
    std::vector<Real> eurVols2;

    auto  volsteptimes_a = Array(volstepdates.size());

    for (Size i = 0; i < volstepdates.size(); ++i) 
        volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
    
    for (Size i = 0; i < volstepdates.size() + 1; ++i)     
    {
        eurVols.push_back(0.02);// + (0.0080 - 0.0050) * std::exp(-0.3 * static_cast<double>(i)));
        eurVols2.push_back(0.0213);// + (0.0080 - 0.0050) * std::exp(-0.3 * static_cast<double>(i)));
    }

    Array eurVols_a;
    Array eurVols2_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    eurVols2_a = Array(eurVols2.begin(), eurVols2.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.02);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);
    auto model2 = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols2_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng2 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model2);
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(settlementDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");

    for (double strike = 0.01; strike <= 0.05; strike += 0.005)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(12, January, 2017));
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv1 = swaption->NPV();
        
        swaption->setPricingEngine(eurSwEng2);
        Real npv2 = swaption->NPV();

        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike*100.0 << "%): " << npv1 * 10000.00 << " bp., "<< npv2 * 10000.00 << " bp., diff: "<< (npv1-npv2)*10000.00 <<" bp.");
        //BOOST_CHECK(std::fabs(npv)<1e-4);
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");

    for (double strike = 0.01; strike <= 0.05; strike += 0.005)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(12, January, 2017));
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv1 = swaption->NPV();
        
        swaption->setPricingEngine(eurSwEng2);
        Real npv2 = swaption->NPV();

        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike*100.0 << "%): " << npv1 * 10000.00 << " bp., "<< npv2 * 10000.00 << " bp., diff: "<< (npv1-npv2)*10000.00 <<" bp.");
        //BOOST_CHECK(std::fabs(npv)<1e-4);
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
}

BOOST_AUTO_TEST_CASE(testLgmPricingBachelier) {

    BOOST_TEST_MESSAGE("Testing pricing compared to the Bachelier model ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Settings::instance().evaluationDate() = settlementDate;    
    Date maturityDate = calendar.advance(settlementDate, 5, Years);

    Date swapStartDate = calendar.advance(settlementDate, 3, Years);

    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts= QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed());

    std::vector<Date> volstepdates;
    volstepdates.push_back(Date(15, July, 2016));
    volstepdates.push_back(Date(15, July, 2017));
    volstepdates.push_back(Date(15, July, 2018));
    volstepdates.push_back(Date(15, July, 2019));
    volstepdates.push_back(Date(15, July, 2020));

    std::vector<Real> eurVols;

    auto  volsteptimes_a = Array(volstepdates.size());

    for (Size i = 0; i < volstepdates.size(); ++i) 
        volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
    
    for (Size i = 0; i < volstepdates.size() + 1; ++i)     
        eurVols.push_back(0.02);// + (0.0080 - 0.0050) * std::exp(-0.3 * static_cast<double>(i)));

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.02);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);
    
    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);

    auto nVol = QuantLib::ext::make_shared<SimpleQuote>(0.02);
    QuantLib::ext::shared_ptr<BachelierSwaptionEngine> bachSwEng = QuantLib::ext::make_shared<BachelierSwaptionEngine>(eurYtsHandle, Handle<Quote>(nVol));

    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(swapStartDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");

    for (double strike = 0.01; strike <= 0.05; strike += 0.005)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(15, May, 2018));
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv1 = swaption->NPV();
        
        swaption->setPricingEngine(bachSwEng);
        Real npv2 = swaption->NPV();

        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike*100.0 << "%): " << npv1 * 10000.00 << " bp., "<< npv2 * 10000.00 << " bp., diff: "<< (npv1-npv2)*10000.00 <<" bp.");
        //BOOST_CHECK(std::fabs(npv)<1e-4);
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");

    for (double strike = 0.01; strike <= 0.05; strike += 0.005)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(12, January, 2017));
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv1 = swaption->NPV();
        
        swaption->setPricingEngine(bachSwEng);
        Real npv2 = swaption->NPV();

        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike*100.0 << "%): " << npv1 * 10000.00 << " bp., "<< npv2 * 10000.00 << " bp., diff: "<< (npv1-npv2)*10000.00 <<" bp.");
        //BOOST_CHECK(std::fabs(npv)<1e-4);
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
} */

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
