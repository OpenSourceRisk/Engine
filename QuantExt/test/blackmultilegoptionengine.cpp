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
#include <boost/test/unit_test.hpp>

#include <qle/pricingengines/blackswaptionenginedeltagamma.hpp>

#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <qle/pricingengines/blackmultilegoptionengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>

#include <qle/pricingengines/blackmultilegoptionengine.hpp>
#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
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
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <ql/time/calendars/target.hpp>

#include <qle/pricingengines/analyticlgmcdsoptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

using namespace QuantLib;
using namespace QuantExt;

using namespace boost::unit_test_framework;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackMultilegOptionEngine)

BOOST_AUTO_TEST_CASE(testAgainstLGMEdgeCase) {
    // In case of zero volatility the LGM model and the Black Multileg will both converte to the
    // discounted cashflows method, i.e. the usual swap pricing routine.
    // 
    // To ensure a stable pricing routine we cover a wide range of strikes including high values.
    // Negative strikes are not possible following the definition of the Black'76 formula. 
    // See next unit test for the displaced model approach allowing for negative strikes.

    BOOST_TEST_MESSAGE("Testing Black Multileg Option Engine against LGM in zero valatility case ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date expiryDate(10, July, 2017);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiryDate); // T=2
    double vol = 0.0000001; 
    Date startDate(15, July, 2017);
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

    Volatility vola(vol);
    auto  volsteptimes_a = Array(volstepdates.size());

    for (Size i = 0; i < volstepdates.size(); ++i) 
        volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
    
    for (Size i = 0; i < volstepdates.size() + 1; ++i) 
        eurVols.push_back(vol); 

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.000000); // No mean reversion

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (double strike = 0.01; strike < 0.25; strike += 0.01)
    {
        // Get Lgm Price
        QuantLib::ext::shared_ptr<PricingEngine> lgmEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
        boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise); 
        swaption->setPricingEngine(lgmEngine);
        Real lgmPrice = swaption->NPV(); 

        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        boost::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {true, false};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        boost::shared_ptr<MultiLegOption> swaptionMulti= boost::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        // Get Black Multileg Price
        auto sqc = boost::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, vola, Actual365Fixed());
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    LGM Model: " << lgmPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(lgmPrice - multiPrice) < 10e-4)
          BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(lgmPrice, multiPrice, 1.0); // Tolerance of 1%
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (double strike = 0.01; strike < 0.25; strike += 0.01)
    {
        // Get Lgm Price
        QuantLib::ext::shared_ptr<PricingEngine> lgmEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
        boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise); 
        swaption->setPricingEngine(lgmEngine);
        Real lgmPrice = swaption->NPV(); 

        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        boost::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {false, true};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        boost::shared_ptr<MultiLegOption> swaptionMulti= boost::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        // Get Black Multileg Price
        auto sqc = boost::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, vola, Actual365Fixed());
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    LGM Model: " << lgmPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(lgmPrice - multiPrice) < 10e-4)
          BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(lgmPrice, multiPrice, 1.0); // Tolerance of 1%
    }
}

BOOST_AUTO_TEST_CASE(testAgainstLGMEdgeCaseDisplacement) {
    // The displacement allows for negative strikes values.
    // More precisely the whole "setting" of the Black'76 model is shifted to the
    // positive part, i.e. the upper side above the x-axis. The forward and the strike
    // are thus shifted according to the given displacement parameter before pricing.

    BOOST_TEST_MESSAGE("Testing Black Multileg Option Engine with displacement against LGM in zero valatility case ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date expiryDate(10, July, 2017);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiryDate); // T=2
    double vol = 0.0000001; 
    Date startDate(15, July, 2017);
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

    Volatility vola(vol);
    auto  volsteptimes_a = Array(volstepdates.size());

    for (Size i = 0; i < volstepdates.size(); ++i) 
        volsteptimes_a[i] = eurYts->timeFromReference(volstepdates[i]);
    
    for (Size i = 0; i < volstepdates.size() + 1; ++i) 
        eurVols.push_back(vol); 

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.000000); // No mean reversion

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (double strike = -0.01; strike < 0.25; strike += 0.01)
    {
        // Get Lgm Price
        QuantLib::ext::shared_ptr<PricingEngine> lgmEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
        boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise); 
        swaption->setPricingEngine(lgmEngine);
        Real lgmPrice = swaption->NPV(); 

        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        boost::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {true, false};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        boost::shared_ptr<MultiLegOption> swaptionMulti= boost::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        // Get Black Multileg Price
        double shift = 0.015;
        auto sqc = boost::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, vola, Actual365Fixed(), ShiftedLognormal, shift);
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    LGM Model: " << lgmPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(lgmPrice - multiPrice) < 10e-4)
          BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(lgmPrice, multiPrice, 1.0); // Tolerance of 1%
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (double strike = -0.01; strike < 0.25; strike += 0.01)
    {
        // Get Lgm Price
        QuantLib::ext::shared_ptr<PricingEngine> lgmEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
        boost::shared_ptr<IborIndex> EURIBOR6m = boost::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);
        boost::shared_ptr<VanillaSwap> swap = boost::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise); 
        swaption->setPricingEngine(lgmEngine);
        Real lgmPrice = swaption->NPV(); 

        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        boost::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {false, true};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        boost::shared_ptr<MultiLegOption> swaptionMulti= boost::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        // Get Black Multileg Price
        double shift = 0.015;
        auto sqc = boost::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, vola, Actual365Fixed(), ShiftedLognormal, shift);
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    LGM Model: " << lgmPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(lgmPrice - multiPrice) < 10e-4)
          BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(lgmPrice, multiPrice, 1.0); // Tolerance of 1%
    }
}

BOOST_AUTO_TEST_CASE(testAgainstBermudanEngine) {
    // A Bermudan swaption is defined as a swaption with earyl exercise feature, i.e.
    // there are many dates per year that the swaption can be exercised.
    // Therein, exercising means that the fixed-float deal is applied to the remaining
    // part of the swap period until maturity.
    // Therefore the Bermudan engine must return a value at least as high as the 
    // "European" engine (up to numerical errors).

    BOOST_TEST_MESSAGE("Testing Black Multileg Option Engine against Bermudan Engine ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date expiryDate(10, July, 2017);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiryDate); // T=2
    double vol = 0.01; 
    Date startDate(15, July, 2017);
    Settings::instance().evaluationDate() = settlementDate;
    Date maturityDate = calendar.advance(settlementDate, 5, Years);
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
        eurVols.push_back(vol); 

    Array eurVols_a;
    Array notimes_a, eurKappa_a;
    eurVols_a = Array(eurVols.begin(), eurVols.end());
    notimes_a = Array(0);
    eurKappa_a = Array(1, 0.000000); // No mean reversion

    Volatility vola(vol);

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Checking Receiver Swaps ...");
    for (double strike = 0.01; strike < 0.05; strike += 0.01)
    {
        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        boost::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {true, false};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        boost::shared_ptr<MultiLegOption> swaptionMulti= boost::make_shared<MultiLegOption> (legs, payer, currency, exercise);

         QuantLib::ext::shared_ptr<Integrator> inte=QuantLib::ext::make_shared<SimpsonIntegral>(1.0E-8, 10000);
         QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(model, QuantExt::HwModel::Measure::LGM, 
             LinearGaussMarkovModel::Discretization::Euler, true, inte);
         Size americanExerciseTimeStepsPerYear = 10;
         QuantLib::ext::shared_ptr<PricingEngine> engineBermudan = 
             QuantLib::ext::make_shared<NumericLgmMultiLegOptionEngine>(lgm, 0.0001, 10000, 0.0001, 10000,  eurYtsHandle, americanExerciseTimeStepsPerYear);
         swaptionMulti->setPricingEngine(engineBermudan);
         Real bermPrice = swaptionMulti->NPV();

        // Get Black Multileg Price
        auto sqc = boost::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, vola, Actual365Fixed());
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    Berm Model: " << bermPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        BOOST_CHECK(multiPrice < bermPrice);
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaps ...");
    for (double strike = 0.01; strike < 0.05; strike += 0.01)
    {
        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        boost::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {false, true};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        boost::shared_ptr<MultiLegOption> swaptionMulti= boost::make_shared<MultiLegOption> (legs, payer, currency, exercise);

         QuantLib::ext::shared_ptr<Integrator> inte=QuantLib::ext::make_shared<SimpsonIntegral>(1.0E-8, 100);
         QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(model, QuantExt::HwModel::Measure::LGM, 
             LinearGaussMarkovModel::Discretization::Euler, true, inte);
         Size americanExerciseTimeStepsPerYear = 1;
         QuantLib::ext::shared_ptr<PricingEngine> engineBermudan = 
             QuantLib::ext::make_shared<NumericLgmMultiLegOptionEngine>(lgm, 0.01, 100, 0.01, 100,  eurYtsHandle, americanExerciseTimeStepsPerYear);
         swaptionMulti->setPricingEngine(engineBermudan);
         Real bermPrice = swaptionMulti->NPV();

        // Get Black Multileg Price
        auto sqc = boost::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, vola, Actual365Fixed());
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    Berm Model: " << bermPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(multiPrice - bermPrice) < 50e-4)
          BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(multiPrice, bermPrice, 10.0); // Tolerance of 10%

    }
}


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
