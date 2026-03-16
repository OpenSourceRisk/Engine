/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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
#include <boost/test/data/test_case.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/mcmultilegoptionengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>

using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(LgmPricingCases)

// The following two helper functions are used in that unit test suite only.
// The implementation of those shall not be called from a library to 
// ensure the independency of testing. Since they both come without 
// discounting they are ready for swaption pricing.

// Bachelier model for European Call option
double bachelierCallPrice(double spot, double strike, double volatility, double timeToMaturity, double riskFreeRate) {
    double d1 = (spot - strike) / (volatility * std::sqrt(timeToMaturity));
    double callPrice = ((spot - strike) * 0.5 * erfc(-d1 / std::sqrt(2.0)) + 
        volatility * std::sqrt(timeToMaturity) * std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * M_PI));
    return callPrice;
}

// Bachelier model for European Put option
double bachelierPutPrice(double spot, double strike, double volatility, double timeToMaturity, double riskFreeRate) {
    double d1 = (spot - strike) / (volatility * std::sqrt(timeToMaturity));
    double putPrice = ((strike - spot) * 0.5 * 
        erfc(d1 / std::sqrt(2.0)) + volatility * std::sqrt(timeToMaturity) * std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * M_PI));
    return putPrice;
}

BOOST_AUTO_TEST_CASE(testBachelier) {
    // The LGM model is approximately close to the well-known Bachelier approach in case of a zero mean-reversion rate.
    // The dynamics defined via the underlying SDE lead to that relationship between the two models.
    // We check that equality for different swaption types (payer and receiver) and different strikes
    // that will cover the practically relevant area.
    
    BOOST_TEST_MESSAGE("Testing LGM in cases equivalent to the Bachelier model  ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(Date(10, July, 2017)); // T=2
    //double T = 2.0;
    double vol = 0.02; 
    Date startDate(15, July, 2017);
    Settings::instance().evaluationDate() = settlementDate;    
    Date maturityDate = calendar.advance(settlementDate, 5, Years);
    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts= QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed(), Compounded, Semiannual);
   
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

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    Volatility vola(vol);
    QuantLib::ext::shared_ptr<PricingEngine> engineLgm = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    QuantLib::ext::shared_ptr<PricingEngine> engineBach = QuantLib::ext::make_shared<BachelierSwaptionEngine>(eurYtsHandle, vola);

    ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    BOOST_TEST_MESSAGE("Checking Receiver Swaptions ...");
    for (double strike = -0.01; strike < 0.05; strike += 0.01)
    {
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, 
            schedule, strike, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual360());        
        ext::shared_ptr<Swaption> swaption = ext::make_shared<Swaption>(swap, exercise); 

        swaption->setPricingEngine(engineLgm);
        Real npv = swaption->NPV();         

        swaption->setPricingEngine(engineBach);
        Real bach = swaption->NPV();
        
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("    Bachelier Model: " << bach * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");

        if (std::fabs(npv - bach) < 10e-4)
            BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(npv, bach, 0.1); // Tolerance of 1%
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaptions ...");
    for (double strike = -0.01; strike < 0.05; strike += 0.01) 
    {
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Payer, notional,
             schedule, strike, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual360());
        ext::shared_ptr<Swaption> swaption = ext::make_shared<Swaption>(swap, exercise); 

        swaption->setPricingEngine(engineLgm);
        Real npv = swaption->NPV();

        swaption->setPricingEngine(engineBach);
        Real bach = swaption->NPV();
        
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("    Bachelier Model: " << bach * 10000.0 << " bp.");        
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(npv - bach) < 10e-4)
            BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(npv, bach, 0.1); // Tolerance of 1%
    }
}

BOOST_AUTO_TEST_CASE(testBachelierManual) {
    // The same test as before, but with manual calculation of the annuity.
    // The latter one is the factor which the Bachelier formula is multiplied by
    // in the usual swaption pricing method. The implementation of the Bachelier
    // formula is also checked by this test at the same time.
    
    BOOST_TEST_MESSAGE("Testing LGM pricing in Bachelier case manually ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(Date(10, July, 2017));
    double T = 2.0;
    double vol = 0.0100; 
    Date startDate(15, July, 2017);
    Settings::instance().evaluationDate() = settlementDate;    
    Date maturityDate = calendar.advance(settlementDate, 5, Years);
    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts= QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed(), Compounded, Semiannual);
   
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
    eurKappa_a = Array(1, 0.00000001); // No mean reversion 

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    QuantLib::ext::shared_ptr<PricingEngine> engineLgm = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
    ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    double Annuity=0.0;

    for (int i=4;i<10;++i)
        Annuity += 0.5 * exp(-(double) (i) * 0.5 *  fixedRate);

    BOOST_TEST_MESSAGE("Checking Receiver Swaptions ...");
    for (double strike = -0.01; strike < 0.05; strike += 0.01)
    {
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional,
             schedule, strike, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual360());        
        ext::shared_ptr<Swaption> swaption = ext::make_shared<Swaption>(swap, exercise); 

        swaption->setPricingEngine(engineLgm);
        Real npv = swaption->NPV(); 

        double limitValue = Annuity * bachelierPutPrice(fixedRate, strike, vol, T, fixedRate);
        
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("    Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");

        if (std::fabs(npv - limitValue) < 10e-4)
            BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(npv, limitValue, 0.1); // Tolerance of 0.1%
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaptionss ...");
    for (double strike = -0.01; strike < 0.05; strike += 0.01) 
    {
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Payer, notional,
             schedule, strike, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual360());
        ext::shared_ptr<Swaption> swaption = ext::make_shared<Swaption>(swap, exercise); 

        swaption->setPricingEngine(engineLgm);
        Real npv = swaption->NPV();

        double limitValue = Annuity * bachelierCallPrice(fixedRate, strike, vol, T, fixedRate);

        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): " << npv * 10000.00 << " bp. ");
        BOOST_TEST_MESSAGE("    Limit Value: " << limitValue * 10000.0 << " bp., " << "Annuity: " << Annuity);
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(npv - limitValue) < 10e-4)
            BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(npv, limitValue, 0.1); // Tolerance of 0.1%
    }
}

BOOST_AUTO_TEST_CASE(testBermudanEngines) {
    // For the pricing of Bermudan-style swaptions there are MC (Monte-Carlo) 
    // and FD (Finite Differences) methods available in ORE. 
    //
    // Both methods must return equal values up to a numerical error. These 
    // results must be higher or equal to the European value.

    BOOST_TEST_MESSAGE("Testing LGM pricing Bermudan functionality ...");
    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date expiryDate(10, July, 2017);
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(expiryDate); // T=2
    double vol = 0.02; 
    Date startDate(15, July, 2017);
    Settings::instance().evaluationDate() = settlementDate;
    Date maturityDate = calendar.advance(settlementDate, 5, Years);
    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts= QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed(), Compounded, Semiannual);
   
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

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    std::vector<Date> exDates;
    exDates.push_back(Date(10, July, 2017));
    exDates.push_back(Date(10, July, 2018));
    exDates.push_back(Date(10, July, 2019));

    bool payoffAtExpiry = false;

    BOOST_TEST_MESSAGE("Checking Receiver Swaptions ...");
    for (double strike = 0.01; strike < 0.05; strike += 0.01)
    {
        // Get Lgm Price
        QuantLib::ext::shared_ptr<PricingEngine> lgmEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional,
            schedule, strike, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        ext::shared_ptr<Swaption> swaption = ext::make_shared<Swaption>(swap, exercise); 
        swaption->setPricingEngine(lgmEngine);
        Real lgmPrice = swaption->NPV(); 

        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {true, false};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};

        BermudanExercise exe(exDates, payoffAtExpiry);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<BermudanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        QuantLib::ext::shared_ptr<Integrator> inte=QuantLib::ext::make_shared<SimpsonIntegral>(1.0E-8, 100);
        QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(model, QuantExt::HwModel::Measure::LGM, 
            LinearGaussMarkovModel::Discretization::Euler, true, inte);

        QuantLib::ext::shared_ptr<PricingEngine> engineBermudanFD =             
             QuantLib::ext::make_shared<NumericLgmMultiLegOptionEngine>(lgm, 50, QuantLib::FdmSchemeDesc::Douglas(), 64, 24, 1e-4, eurYtsHandle, 10);

         swaptionMulti->setPricingEngine(engineBermudanFD);
         Real bermPriceFD = swaptionMulti->NPV();  

        auto engineBermudanMC = QuantLib::ext::make_shared<McMultiLegOptionEngine>(
            lgm, SobolBrownianBridge, SobolBrownianBridge, 100000, 100000, 42, 42, 4, LsmBasisSystem::Monomial);

         swaptionMulti->setPricingEngine(engineBermudanMC);
         Real bermPriceMC = swaptionMulti->NPV();   
        
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    LGM Model: " << lgmPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Berm Model FD: " << bermPriceFD * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Berm Model MC: " << bermPriceMC * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");

        if (std::fabs(bermPriceMC - bermPriceFD) < 10e-4) // Threshold of 10 basis points for "zero" cases
          BOOST_CHECK(true);
        else
          BOOST_CHECK_CLOSE(bermPriceMC, bermPriceFD, 0.1); // Tolerance of 0.1%

        BOOST_CHECK(bermPriceMC > lgmPrice); // The Bermudan price must be higher than the European price
        BOOST_CHECK(bermPriceFD > lgmPrice); // The Bermudan price must be higher than the European price
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaptions ...");
    for (double strike = 0.01; strike < 0.05; strike += 0.01)
    {
        // Get Lgm Price
        QuantLib::ext::shared_ptr<PricingEngine> lgmEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        ext::shared_ptr<Swaption> swaption = ext::make_shared<Swaption>(swap, exercise); 
        swaption->setPricingEngine(lgmEngine);
        Real lgmPrice = swaption->NPV(); 

        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {false, true};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};

        BermudanExercise exe(exDates, payoffAtExpiry);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<BermudanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        QuantLib::ext::shared_ptr<Integrator> inte=QuantLib::ext::make_shared<SimpsonIntegral>(1.0E-8, 100);
        QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(model, QuantExt::HwModel::Measure::LGM, 
            LinearGaussMarkovModel::Discretization::Euler, true, inte);

        QuantLib::ext::shared_ptr<PricingEngine> engineBermudanFD = 
             QuantLib::ext::make_shared<NumericLgmMultiLegOptionEngine>(lgm, 50, QuantLib::FdmSchemeDesc::Douglas(), 64, 24, 1e-4, eurYtsHandle, 10);

        swaptionMulti->setPricingEngine(engineBermudanFD);
        Real bermPriceFD = swaptionMulti->NPV();

        auto engineBermudanMC = QuantLib::ext::make_shared<McMultiLegOptionEngine>(
            lgm, SobolBrownianBridge, SobolBrownianBridge, 100000, 100000, 42, 42, 4, LsmBasisSystem::Monomial);

         swaptionMulti->setPricingEngine(engineBermudanMC);
         Real bermPriceMC = swaptionMulti->NPV();
              
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    LGM Model: " << lgmPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Berm Model FD: " << bermPriceFD * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Berm Model MC: " << bermPriceMC * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");

        if (std::fabs(bermPriceMC - bermPriceFD) < 10e-4) // Threshold of 10 basis points for "zero" cases
          BOOST_CHECK(true);
        else
          BOOST_CHECK_CLOSE(bermPriceMC, bermPriceFD, 0.1); // Tolerance of 0.1%

        BOOST_CHECK(bermPriceMC > lgmPrice); // The Bermudan price must be higher than the European price
        BOOST_CHECK(bermPriceFD > lgmPrice); // The Bermudan price must be higher than the European price
    }
}

BOOST_AUTO_TEST_CASE(testBermudanEnginesEdgeCase) {
    // For the pricing of Bermudan-style swaptions there are MC (Monte-Carlo) 
    // and FD (Finite Differences) methods available in ORE. 
    //
    // Both methods must return values equal to the European price in case of only
    // one given exercise date.

    BOOST_TEST_MESSAGE("Testing LGM Bermduan pricing in cases with only one exercise date ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date expiryDate(10, July, 2017);
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(expiryDate); // T=2
    double vol = 0.02; 
    Date startDate(15, July, 2017);
    Settings::instance().evaluationDate() = settlementDate;
    Date maturityDate = calendar.advance(settlementDate, 5, Years);
    Real notional = 1.0;
    Rate fixedRate = 0.02;

    auto eurYts= QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed(), Compounded, Semiannual);
   
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

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);
    Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);

    std::vector<Date> exDates;
    exDates.push_back(Date(10, July, 2017)); // Only one exercise date

    bool payoffAtExpiry = false;

    BOOST_TEST_MESSAGE("Checking Receiver Swaptions ...");
    for (double strike = 0.01; strike < 0.05; strike += 0.01)
    {
        // Get Lgm Price
        QuantLib::ext::shared_ptr<PricingEngine> lgmEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional,
            schedule, strike, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        ext::shared_ptr<Swaption> swaption = ext::make_shared<Swaption>(swap, exercise); 
        swaption->setPricingEngine(lgmEngine);
        Real lgmPrice = swaption->NPV(); 

        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {true, false};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};

        BermudanExercise exe(exDates, payoffAtExpiry);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<BermudanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        QuantLib::ext::shared_ptr<Integrator> inte=QuantLib::ext::make_shared<SimpsonIntegral>(1.0E-8, 100);
        QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(model, QuantExt::HwModel::Measure::LGM, 
            LinearGaussMarkovModel::Discretization::Euler, true, inte);

        QuantLib::ext::shared_ptr<PricingEngine> engineBermudanFD =             
             QuantLib::ext::make_shared<NumericLgmMultiLegOptionEngine>(lgm, 50, QuantLib::FdmSchemeDesc::Douglas(), 64, 24, 1e-4, eurYtsHandle, 10);

         swaptionMulti->setPricingEngine(engineBermudanFD);
         Real bermPriceFD = swaptionMulti->NPV();  

        auto engineBermudanMC = QuantLib::ext::make_shared<McMultiLegOptionEngine>(
            lgm, SobolBrownianBridge, SobolBrownianBridge, 100000, 100000, 42, 42, 4, LsmBasisSystem::Monomial);

         swaptionMulti->setPricingEngine(engineBermudanMC);
         Real bermPriceMC = swaptionMulti->NPV();   
        
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    LGM Model: " << lgmPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Berm Model FD: " << bermPriceFD * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Berm Model MC: " << bermPriceMC * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");

        if (std::fabs(bermPriceMC - lgmPrice) < 10e-4) // Threshold of 10 basis points for "zero" cases
          BOOST_CHECK(true);
        else
          BOOST_CHECK_CLOSE(bermPriceMC, lgmPrice, 0.1); // Tolerance of 0.1%

        if (std::fabs(bermPriceFD - lgmPrice) < 10e-4) // Threshold of 10 basis points for "zero" cases
          BOOST_CHECK(true);
        else
          BOOST_CHECK_CLOSE(bermPriceFD, lgmPrice, 0.1); // Tolerance of 0.1%

    }

    BOOST_TEST_MESSAGE("Checking Payer Swaptions ...");
    for (double strike = 0.01; strike < 0.05; strike += 0.01)
    {
        // Get Lgm Price
        QuantLib::ext::shared_ptr<PricingEngine> lgmEngine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model);
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());        
        ext::shared_ptr<Swaption> swaption = ext::make_shared<Swaption>(swap, exercise); 
        swaption->setPricingEngine(lgmEngine);
        Real lgmPrice = swaption->NPV(); 

        // Get Bermudan Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {false, true};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};

        BermudanExercise exe(exDates, payoffAtExpiry);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<BermudanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        QuantLib::ext::shared_ptr<Integrator> inte=QuantLib::ext::make_shared<SimpsonIntegral>(1.0E-8, 100);
        QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(model, QuantExt::HwModel::Measure::LGM, 
            LinearGaussMarkovModel::Discretization::Euler, true, inte);

        QuantLib::ext::shared_ptr<PricingEngine> engineBermudanFD = 
             QuantLib::ext::make_shared<NumericLgmMultiLegOptionEngine>(lgm, 50, QuantLib::FdmSchemeDesc::Douglas(), 64, 24, 1e-4, eurYtsHandle, 10);

        swaptionMulti->setPricingEngine(engineBermudanFD);
        Real bermPriceFD = swaptionMulti->NPV();

        auto engineBermudanMC = QuantLib::ext::make_shared<McMultiLegOptionEngine>(
            lgm, SobolBrownianBridge, SobolBrownianBridge, 100000, 100000, 42, 42, 4, LsmBasisSystem::Monomial);

         swaptionMulti->setPricingEngine(engineBermudanMC);
         Real bermPriceMC = swaptionMulti->NPV();
              
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    LGM Model: " << lgmPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Berm Model FD: " << bermPriceFD * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Berm Model MC: " << bermPriceMC * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");

        if (std::fabs(bermPriceMC - lgmPrice) < 10e-4) // Threshold of 10 basis points for "zero" cases
          BOOST_CHECK(true);
        else
          BOOST_CHECK_CLOSE(bermPriceMC, lgmPrice, 0.1); // Tolerance of 0.1%

        if (std::fabs(bermPriceFD - lgmPrice) < 10e-4) // Threshold of 10 basis points for "zero" cases
          BOOST_CHECK(true);
        else
          BOOST_CHECK_CLOSE(bermPriceFD, lgmPrice, 0.1); // Tolerance of 0.1%
    }
}


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
