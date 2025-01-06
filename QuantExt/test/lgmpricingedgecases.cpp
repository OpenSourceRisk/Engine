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

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;
using namespace boost::accumulators;
namespace bdata = boost::unit_test::data;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CrossAssetModelTest)
 
BOOST_AUTO_TEST_CASE(testLgmEdgeCases1) {

    BOOST_TEST_MESSAGE("Testing pricing in edge cases with very high/low strike...");

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
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(settlementDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");

    for (double strike = -0.10; strike <= -0.05; strike += 0.005)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(12, January, 2017));
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike*100.0 << "%): " << npv * 10000.00 << " bp. ");
        BOOST_CHECK(std::fabs(npv)<1e-4);
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");

    for (double strike = 0.09; strike <= 0.20; strike += 0.005)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(12, January, 2017));
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike*100.0 << "%): " << npv * 10000.00 << " bp. ");
        BOOST_CHECK(std::fabs(npv)<1e-4);
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
}

BOOST_AUTO_TEST_CASE(testLgmEdgeCases2) {

    BOOST_TEST_MESSAGE("Testing pricing in edge cases with very small volatility ..");
    
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

    auto  volsteptimes_a = Array(volstepdates.size());

    for (Size i = 0; i < volstepdates.size(); ++i) 
        volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
    
    for (Size i = 0; i < volstepdates.size() + 1; ++i) 
        eurVols.push_back(0.0000);// + (0.0080 - 0.0050) * std::exp(-0.3 * static_cast<double>(i)));

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.02);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(settlementDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Receiver Swaps");

    for (double strike = -0.02; strike <= 0.14; strike += 0.01)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(12, January, 2017));
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike << "): " << npv * 10000.00 << " bp. ");
    }

    BOOST_TEST_MESSAGE("Payer Swaps");

    for (double strike = -0.02; strike <= 0.14; strike += 0.01)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(12, January, 2017));
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike << "): " << npv * 10000.00 << " bp. ");
    }

    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
}

BOOST_AUTO_TEST_CASE(testLgmEdgeCases3) {

    BOOST_TEST_MESSAGE("Testing pricing in edge cases with zero time to maturity.");
    
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

    auto  volsteptimes_a = Array(volstepdates.size());

    for (Size i = 0; i < volstepdates.size(); ++i) 
        volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
    
    for (Size i = 0; i < volstepdates.size() + 1; ++i) 
        eurVols.push_back(0.0000);// + (0.0080 - 0.0050) * std::exp(-0.3 * static_cast<double>(i)));

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.02);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(settlementDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Receiver Swaps");

    for (double strike = -0.02; strike <= 0.14; strike += 0.01)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(12, August, 2020));
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);

        try {
            swaption->NPV();
            BOOST_FAIL("QL_Require failed.");
        } catch (const std::exception& e) {
            BOOST_TEST_MESSAGE(std::string(e.what()));
            BOOST_CHECK_EQUAL(std::string(e.what()), "fixed leg's periods are all before expiry.");
        }
    }
}

BOOST_AUTO_TEST_CASE(testLgmEdgeCases4) {

    BOOST_TEST_MESSAGE("Testing pricing in edge cases with expiry equal to settlement");
    
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

    auto  volsteptimes_a = Array(volstepdates.size());

    for (Size i = 0; i < volstepdates.size(); ++i) 
        volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
    
    for (Size i = 0; i < volstepdates.size() + 1; ++i) 
        eurVols.push_back(0.0000);// + (0.0080 - 0.0050) * std::exp(-0.3 * static_cast<double>(i)));

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.02);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(settlementDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Receiver Swaps");

    for (double strike = -0.02; strike <= 0.14; strike += 0.01)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(settlementDate);
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike << "): " << npv * 10000.00 << " bp. ");
    }

    BOOST_TEST_MESSAGE("Payer Swaps");

    for (double strike = -0.02; strike <= 0.14; strike += 0.01)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(settlementDate);
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);
        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        BOOST_TEST_MESSAGE("Swaption (Strike = " << strike << "): " << npv * 10000.00 << " bp. ");
    }

    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
