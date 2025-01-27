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
#include <qle/pricingengines/discountingswapenginedeltagamma.hpp>
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

BOOST_AUTO_TEST_SUITE(LgmPricingEdgeCases)
 
BOOST_AUTO_TEST_CASE(testHighStrike) {
    // This test checks the LGM model swaption pricing routine in case of very high strike values.
    // The example receiver swaptions will be strongly in the money so the model price will be equal
    // to the swap forward price D * (K - S). The payer swaptions will be strongly out of the money 
    // so their value can be assumed zero in presence of high strikes.
    //
    // This test also checks the annuity calculation against a benchmark value calculated "manually".

    BOOST_TEST_MESSAGE("Testing LGM pricing in edge cases with very high strike ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(13, July, 2016)); // T = 1
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
        eurVols.push_back(0.005);

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.02);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    double Annuity=0.0;

    for (int i = 2; i < 10; ++i) // Starting at T = 1
        Annuity +=0.5 * exp(-(double)i * 0.5 * fixedRate);

    // Starting at 5% which is 3% above the market atm rate
    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (int strike = 5; strike <= 10; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, (double)strike/100.0, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        double limitValue = exp(-fixedRate * 1.0) * ((double)strike / 100.0 - fixedRate) * Annuity;
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK_CLOSE(npv, limitValue, 1.0); // Tolerance of 1%
    }

    // Starting at 10% which is 8% above the market atm rate
    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (int strike = 10; strike <= 15; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, (double)strike/100.0, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();

        double limitValue = 0.0;
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK(std::fabs(npv-limitValue)<5e-4); // Five basis points tolerance
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
}

BOOST_AUTO_TEST_CASE(testLowStrike) {
    // This test checks the LGM model swaption pricing routine in case of very low strike values.
    // The example payer swaptions will be strongly in the money so the model price will be equal
    // to the swap forward price D * (K - S). The receiver swaptions will be strongly out of the money 
    // so their value can be assumed zero in presence of low strikes.
    //
    // This test also checks the annuity calculation against a benchmark value calculated "manually".

    BOOST_TEST_MESSAGE("Testing LGM pricing in edge cases with very low strike ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(13, July, 2016)); // T = 1
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
        eurVols.push_back(0.005);

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.02);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    double Annuity=0.0;

    for (int i = 2; i < 10; ++i)// Starting at T = 1
        Annuity += 0.5 * exp(-(double)i * 0.5 * fixedRate);

    // Ending at -1% which is 3% below the market atm rate
    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (int strike = -7; strike <= -1; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, (double)strike/100.0, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        double limitValue = exp(-fixedRate * 1.0) * (fixedRate - (double)strike / 100.0) * Annuity;
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK_CLOSE(npv, limitValue, 2.0); // Tolerance of 2%
    }

    // Ending at -5% which is 7% below the market atm rate
    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (int strike = -11; strike <=-5; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, (double)strike/100.0, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();

        double limitValue = 0.0;
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK(std::fabs(npv-limitValue)<5e-4); // Five basis points tolerance
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model -> printParameters(1));    
}

BOOST_AUTO_TEST_CASE(testTooLateExercise) {
    // This test checks the behaviour of the LGM model in case of an erroneous setup,
    // i.e. an exercise date after maturity. The expected behaviour is a software fail,
    // more precisely a fail of QL_Require which is checked for and captured by 
    // the test environment.
    BOOST_TEST_MESSAGE("Testing LGM pricing in edge case of expiry after maturity ...");
    
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
        eurVols.push_back(0.0000);

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
    // Covering the whole range of strikes that matters
    for (int strike = -2; strike <= 14; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, (double)strike/100.0, Actual360(), schedule, EURIBOR6m,  0.0, Actual360());
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(12, August, 2020)); // This is intentional
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

BOOST_AUTO_TEST_CASE(testImmediateExpiry) {
    // This test checks he behaviour of the LGM in case of a given
    // expiry date nearly equal to the settlement date. Since the decision is trivial
    // we compare again to the limit cases.
    BOOST_TEST_MESSAGE("Testing pricing in edge cases with expiry equal to settlement");
    
    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(Date(20, July, 2015)); // T=0
    double T = 0.0;
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
        eurVols.push_back(0.0050);

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, fixedRate);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> eurSwEng1 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    double Annuity=0.0;

    for (int i=2;i<10;++i)
        Annuity += 0.5 * exp(-(double)i * 0.5 * fixedRate);

    // Starting 1% above market rate level of 2%
    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (int strike = 3; strike <= 7; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, (double)strike/100.0, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        double limitValue = exp(-fixedRate * T) * std::fmax(((double)strike / 100.0 - fixedRate), 0.0) * Annuity;
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");

        BOOST_CHECK_CLOSE(npv, limitValue, 2.0); // Tolerance of 2%
    }

    // Ending 1% below market rate level of 2%
    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (int strike = -3; strike <= 1; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, (double)strike/100.0, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(eurSwEng1);
        Real npv = swaption->NPV();
        double limitValue = exp(-fixedRate * T) * (fixedRate - (double)strike / 100.0) * Annuity;
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
         
        BOOST_CHECK_CLOSE(npv, limitValue, 1.0); // Tolerance of 1%
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
}

BOOST_AUTO_TEST_CASE(testLowVolatility) {
    // This test checks the LGM model pricing in case of very low given volatility values. 
    // Again, the swaption prices will either equal the inner value (which is D*(S-K) or D*(K-S) depending
    // on the option type, i.e. "payer" or "receiver") or be zero. Even if the strike is only a little
    // different from the atm rate, the very low volatility will ensure that there are only these two
    // edge cases. That is the difference to testHighStrike and testLowStrike, where the big difference between
    // the strike and the atm rate causes the limit cases to be valid. The numerical results will equal the
    // underlying swap value, which is used as a benchmark here.

    BOOST_TEST_MESSAGE("Testing pricing in edge cases with very low volatility and long maturity ...");

    DayCounter dc = Actual360();
    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date ex = Date(12, July, 2016);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(ex); // T=1
    double T = dc.yearFraction(settlementDate, ex);
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
        eurVols.push_back(0.0000003);

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.002);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    QuantLib::ext::shared_ptr<PricingEngine> swapEng= QuantLib::ext::make_shared<DiscountingSwapEngine>(eurYtsHandle, false, settlementDate, settlementDate);
   
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    double Annuity=0.0;

    for (int i=2;i<100;++i)
        Annuity += dc.yearFraction(settlementDate, ex)/2.0 * exp(-(double) (i) * dc.yearFraction(settlementDate, ex)/2.0 * fixedRate);

    // Starting 1% above market rate level of 2%
    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (int strike = 3; strike < 7; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swap->setPricingEngine(swapEng);
        swaption->setPricingEngine(swaptionEngine);
        Real npv = swaption->NPV();
        double limitValue = exp(-fixedRate * T) * ((double)strike / 100.0 - fixedRate) * Annuity;
        double swapValue = swap->NPV();
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Swap Value: " << swapValue * 10000.0);
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK_CLOSE(npv, swapValue, 0.1); // Tolerance of 1%
    }

    // Starting 1% below market rate level of 2%
    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (int strike = -3; strike <=1; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(swaptionEngine);
        swap->setPricingEngine(swapEng);
        Real npv = swaption->NPV();
        double limitValue = exp(-fixedRate * T) * (fixedRate - (double) strike / 100.0) * Annuity;
        double swapValue = swap->NPV();
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Swap Value: " << swapValue * 10000.0);
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK_CLOSE(npv, swapValue, 0.1); // Tolerance of 3%
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
}

BOOST_AUTO_TEST_CASE(testLowVolatilityLongTerm) {
    // This test checks the LGM model pricing routine in case of a model with very low volatility values and
    // a high maturity of fifty years. Again, the swaption prices will equal the inner value D*(S-K) or
    // D*(K-S) as before. The numerical results will equal the underlying swap value, which is used as a 
    // benchmark in this example. 

    BOOST_TEST_MESSAGE("Testing pricing in edge cases with very low volatility and long maturity ...");

    DayCounter dc = Actual360();
    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date ex = Date(12, July, 2016);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(ex); // T=1
    double T = dc.yearFraction(settlementDate, ex);
    Date startDate(15, July, 2016);
    Settings::instance().evaluationDate() = settlementDate;    
    Date maturityDate = calendar.advance(settlementDate, 50, Years);
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
        eurVols.push_back(0.0000003);

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.002);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    QuantLib::ext::shared_ptr<PricingEngine> swapEng= QuantLib::ext::make_shared<DiscountingSwapEngine>(eurYtsHandle, false, settlementDate, settlementDate);
   
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    double Annuity=0.0;

    for (int i=2;i<100;++i)
        Annuity += dc.yearFraction(settlementDate, ex)/2.0 * exp(-(double) (i) * dc.yearFraction(settlementDate, ex)/2.0 * fixedRate);

    // Starting 1% above market rate level of 2%
    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (int strike = 3; strike < 7; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swap->setPricingEngine(swapEng);
        swaption->setPricingEngine(swaptionEngine);
        Real npv = swaption->NPV();
        double limitValue = exp(-fixedRate * T) * (strike - fixedRate) * Annuity;
        double swapValue = swap->NPV();
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike/100.0 << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Swap Value: " << swapValue * 10000.0);
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK_CLOSE(npv, swapValue, 0.1); // Tolerance of 1%
    }

    // Starting 1% below market rate level of 2%
    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (int strike = -3; strike <=1; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(swaptionEngine);
        swap->setPricingEngine(swapEng);
        Real npv = swaption->NPV();
        double limitValue = exp(-fixedRate * T) * (fixedRate - strike/100.0) * Annuity;
        double swapValue = swap->NPV();
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("Swap Value: " << swapValue * 10000.0);
        BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK_CLOSE(npv, swapValue, 0.1); // Tolerance of 3%
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - "<< model->printParameters(1));    
}

BOOST_AUTO_TEST_CASE(testLowVolatilityLongTerm2) {
    // This test checks the LGM model pricing routine in case of a model with very low volatility values and
    // a very high maturity of one hundred years. These cases may occur e.g. in case of government bonds.
    // Again, the swaption prices will equal their inner value as before. 
    //
    // This long term version with a hundred years maturity checks the bisection method before the 
    // optimization of yStarHelper in analyticlgmswaptionengine.cpp. It covers a wide range of strike inputs
    // as well to check the optimization in cases with unusual inputs. In the other version 
    // "testLowVolatilityLongTerm" that bisection is not in use.

    BOOST_TEST_MESSAGE("Testing pricing in edge cases with very low volatility and long maturity ...");

    DayCounter dc = Actual360();
    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date ex = Date(12, July, 2016);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(ex); // T=1

    Date startDate(15, July, 2016);
    Settings::instance().evaluationDate() = settlementDate;    
    Date maturityDate = calendar.advance(settlementDate, 100, Years);
    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts = QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed());
   
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
        eurVols.push_back(0.0000003);

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.002);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    QuantLib::ext::shared_ptr<PricingEngine> swapEng= QuantLib::ext::make_shared<DiscountingSwapEngine>(eurYtsHandle, false, settlementDate, settlementDate);
   
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (int strike = -10; strike <=10; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(swaptionEngine);
        Real npv = swaption->NPV();

        // Limit cases where the NPV will equal the swap value
        if (strike > 2)
        {
            BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike  << "%): " << npv * 10000.00 << " bp. ");
            swap->setPricingEngine(swapEng);
            double swapValue = swap->NPV();
            BOOST_TEST_MESSAGE("Swap Value: " << swapValue * 10000.0);
            BOOST_TEST_MESSAGE("------------");
            BOOST_CHECK_CLOSE(npv, swapValue, 0.1); // Tolerance of 0.1%
        }

        // Limit cases where NPV will equal zero
        if (strike < 2)
        {
            BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike  << "%): " << npv * 10000.00 << " bp. ");
            double limitValue = 0.0;
            BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp.");
            BOOST_TEST_MESSAGE("------------");
            BOOST_CHECK(std::fabs(npv-limitValue) < 1e-4); // Tolerane of one basis point
        }
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (int strike = -10; strike < 10; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(swaptionEngine);
        Real npv = swaption->NPV();
        
        // Limit cases where the NPV will equal the swap value
        if (strike < 2)
        {           
            BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
            swap->setPricingEngine(swapEng);
            double swapValue = swap->NPV();
            BOOST_TEST_MESSAGE("Swap Value: " << swapValue * 10000.0);
            BOOST_TEST_MESSAGE("------------");
            BOOST_CHECK_CLOSE(npv, swapValue, 0.1); // Tolerance of 0.1%
        }

        // Limit cases where NPV will equal zero
        if (strike > 2)
        {
            BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike << "%): " << npv * 10000.00 << " bp. ");
            double limitValue = 0.0;
            BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp." );
            BOOST_TEST_MESSAGE("------------");
            BOOST_CHECK(std::fabs(npv-limitValue) < 1e-4); // Tolerance of 1 basis point
        }
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - " << model -> printParameters(1));    
}

BOOST_AUTO_TEST_CASE(testHighMeanReversion) {
    // This tests checks the pricing functionality of the LGM in case of a very high
    // given mean reversion parameter. This will ensure that the simulated short rate
    // nearly stays constant over time. Again, in this degenerate case the calculated 
    // swaption price will equal the swap price (for strikes that differ from
    // the atm case).
    //
    // Unit tests with low mean reversion already exist, see "testBachelierCase" in lgmpricing.cpp.
    // In these cases the LGM modelling approach converges to the well know Bachelier model.

    BOOST_TEST_MESSAGE("Testing pricing in edge cases with very high mean reversion ...");

    DayCounter dc = Actual360();
    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date ex = Date(12, July, 2016);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(ex); 
    Date startDate(15, July, 2016);
    Settings::instance().evaluationDate() = settlementDate;    
    Date maturityDate = calendar.advance(settlementDate, 10, Years);
    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts = QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed());
   
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
        eurVols.push_back(0.3);

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 4.0);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    QuantLib::ext::shared_ptr<PricingEngine> swapEng= QuantLib::ext::make_shared<DiscountingSwapEngine>(eurYtsHandle, false, settlementDate, settlementDate);
   
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (int strike = -2; strike < 6; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(swaptionEngine);
        Real npv = swaption->NPV();

        // Limit cases where the NPV will equal the swap value
        if (strike > 2)
        {
            BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike  << "%): " << npv * 10000.00 << " bp. ");
            swap->setPricingEngine(swapEng);
            double swapValue = swap->NPV();
            BOOST_TEST_MESSAGE("Swap Value: " << swapValue * 10000.0);
            BOOST_TEST_MESSAGE("------------");
            BOOST_CHECK_CLOSE(npv, swapValue, 0.1); // Tolerance of 0.1%
        }

        // Limit cases where NPV will equal zero
        if (strike < 2)
        {
            BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike  << "%): " << npv * 10000.00 << " bp. ");
            double limitValue = 0.0;
            BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp.");
            BOOST_TEST_MESSAGE("------------");
            BOOST_CHECK(std::fabs(npv-limitValue) < 1e-4); // Tolerane of one basis point
        }
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (int strike = -2; strike <=6; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(swaptionEngine);
        Real npv = swaption->NPV();
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike  << "%): " << npv * 10000.00 << " bp. ");
        
        // Limit cases where the NPV will equal the swap value
        if (strike <2)
        {           
            swap->setPricingEngine(swapEng);
            double swapValue = swap->NPV();
            BOOST_TEST_MESSAGE("Swap Value: " << swapValue * 10000.0);
            BOOST_TEST_MESSAGE("------------");
            BOOST_CHECK_CLOSE(npv, swapValue, 0.1); // Tolerance of 0.1%
        }

        // Limit cases where NPV will equal zero
        if (strike >2)
        {
            double limitValue = 0.0;
            BOOST_TEST_MESSAGE("Limit Value: " << limitValue * 10000.0 << " bp." );
            BOOST_TEST_MESSAGE("------------");
            BOOST_CHECK(std::fabs(npv-limitValue) < 1e-4); // Tolerance of 1 basis point
        }
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - " << model -> printParameters(1.0));    
}


BOOST_AUTO_TEST_CASE(testSmallMaturity) {
    // This test checks the LGM pricing functionality in case of a very small maturity of 5 days.
    // The pricing routine shall be stable and return values smaller than five basis points.

    BOOST_TEST_MESSAGE("Testing LGM pricing in edge cases with very small maturity ...");

    DayCounter dc = Actual360();
    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date ex = Date(18, July, 2015);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(ex); 
    Date startDate(15, July, 2015);
    Settings::instance().evaluationDate() = settlementDate;    
    Date maturityDate = calendar.advance(settlementDate, 5, Days);
    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts = QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed());
   
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
        eurVols.push_back(0.3);

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 4.0);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
   
    boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Daily), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (int strike = -2; strike < 6; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(swaptionEngine);
        Real npv = swaption->NPV();

        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike  << "%): " << npv * 10000.00 << " bp. ");
        double limitValue = 0.0;
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK(std::fabs(npv-limitValue) < 6e-4); // Tolerane of five basis points
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (int strike = -2; strike <=6; strike ++)
    {
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, (double)strike/100.0, dc, schedule, EURIBOR6m, 0.0, dc);
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(swaptionEngine);
        Real npv = swaption->NPV();
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike  << "%): " << npv * 10000.00 << " bp. ");
        
        double limitValue = 0.0;
        BOOST_TEST_MESSAGE("------------");
        BOOST_CHECK(std::fabs(npv-limitValue) < 6e-4); // Tolerane of five basis points
    }
    
    BOOST_TEST_MESSAGE(" T = 1: Model - " << model -> printParameters(1.0));    
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
