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

#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>
#include <ql/time/calendars/target.hpp>

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
#include <qle/pricingengines/analyticlgmcdsoptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/blackmultilegoptionengine.hpp>
#include <qle/pricingengines/blackmultilegoptionengine.hpp>
#include <qle/pricingengines/blackswaptionenginedeltagamma.hpp>
#include <qle/pricingengines/mcmultilegoptionengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>

using namespace QuantLib;
using namespace QuantExt;

using namespace boost::unit_test_framework;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackMultilegOptionEngine)

// This formulation comes without discounting and thus suitable for swaption pricing
double black76_swaption_price(double F, double K, double r, double T, double sigma, bool is_call) { 
    double d1 = (std::log(F / K) + 0.5 * sigma * sigma * T) / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);
    return (is_call ? (F * 0.5 * erfc(-d1 / std::sqrt(2.0)) - K * 0.5 * erfc(-d2 / std::sqrt(2.0))) :
                                    (K * 0.5 * erfc(d2 / std::sqrt(2.0)) - F * 0.5 * erfc(d1 / std::sqrt(2.0))));
}

BOOST_AUTO_TEST_CASE(testSwapCase) {
    // In case of zero volatility the Black76 model will converge to the discounted cashflows
    // model, i.e. the price of a swap (when in-the-money).
    // To ensure a stable pricing routine we cover a wide range of strikes including high values. 
    // Negative strikes are not possible following the definition of the Black'76 formula. 
    // See next unit test for the displaced model approach allowing for negative strikes.

    BOOST_TEST_MESSAGE("Testing Black Multileg Option Engine against discounted cashflows ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date expiryDate(10, July, 2017);
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(expiryDate); // T=2
    double vol = 0.000000001;  // No volatility
    Date startDate(15, July, 2017);
    Settings::instance().evaluationDate() = settlementDate;
    Date maturityDate = calendar.advance(settlementDate, 5, Years);
    Real notional = 1.0;
    Rate fixedRate = 0.02;
    Volatility vola(vol);
   
    auto eurYts= QuantLib::ext::make_shared<FlatForward>(settlementDate, fixedRate, Actual365Fixed(), Compounded, Semiannual);
    Handle<YieldTermStructure> eurYtsHandle(eurYts);

    BOOST_TEST_MESSAGE("Checking Receiver Swaptions ...");
    for (double strike = 0.04; strike < 0.10; strike += 0.01) // Only in-the-money cases
    {
        // Get Lgm Price
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, 
                          DateGeneration::Backward, false);
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, 
                          schedule, strike, Actual365Fixed(), schedule, EURIBOR6m,  0.0, Actual365Fixed());
                
        QuantLib::ext::shared_ptr<PricingEngine> swapEng = QuantLib::ext::make_shared<DiscountingSwapEngine>(
                          eurYtsHandle, false, settlementDate, settlementDate);
        swap -> setPricingEngine(swapEng);
        Real swapPrice = swap -> NPV();
       
        // Get Black Multileg Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {true, false};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        auto sqc = ext::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, vola, Actual365Fixed());
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    Swap Model: " << swapPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(swapPrice - multiPrice) < 10e-4)
          BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(swapPrice, multiPrice, 0.1); // Tolerance of 0.1%
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaptions ...");
    for (double strike = 0.001; strike < 0.015; strike += 0.005) // Only in-the-money cases
    {
        // Get Lgm Price
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());  

        QuantLib::ext::shared_ptr<PricingEngine> swapEng = QuantLib::ext::make_shared<DiscountingSwapEngine>(eurYtsHandle, 
                                                                                                             false, settlementDate, settlementDate);
        swap -> setPricingEngine(swapEng);
        Real swapPrice = swap -> NPV();

        // Get Black Multileg Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {false, true};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        auto sqc = ext::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, vola, Actual365Fixed());
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    Swap Model: " << swapPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(swapPrice - multiPrice) < 10e-4)
          BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(swapPrice, multiPrice, 0.1); // Tolerance of 0.1%
    }
}

BOOST_AUTO_TEST_CASE(testBlack76Displacement) {
    // This test checks the results from the BlackMultilegOptionEngine against
    // a manual implementation of the Black76 formula (see top of this source file).
    // Due to a shift in forward and strike (model displacement) the whole "setting"
    // is moved to the positive side above the x-axis.

    BOOST_TEST_MESSAGE("Testing Black Multileg Option Engine against shifted Black76 formula ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date expiryDate(10, July, 2017);
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(expiryDate); // T=2
    double T = 2.0;
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
    
    double Annuity=0.0;

        for (int i=4;i<10;++i)
            Annuity += 0.5 * exp(-(double) (i+1) * 0.5 * fixedRate);

    double shift = 0.015;

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle,
                                                                                     volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    BOOST_TEST_MESSAGE("Checking Receiver Swaptions ...");
    for (double strike = -0.01; strike < 0.05; strike += 0.01)
    {
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());        

        // Get Black Multileg Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {true, false};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        auto sqc = ext::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following,
                                                                  vola, Actual365Fixed(), ShiftedLognormal, shift);
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        double blackFormula= Annuity * black76_swaption_price(fixedRate+shift, strike+shift, fixedRate, T, vol, false);

        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    Black76: " << blackFormula * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(blackFormula - multiPrice) < 10e-4)
            BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(blackFormula, multiPrice, 0.1); // Tolerance of 0.1%
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaptions ...");
    for (double strike = -0.01; strike < 0.05; strike += 0.01)
    {       
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());     

        // Get Black Multileg Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {false, true};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        auto sqc = ext::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, 
                                                                  vola, Actual365Fixed(), ShiftedLognormal, shift);
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        double blackFormula= Annuity* black76_swaption_price(fixedRate+shift, strike+shift, fixedRate, T, vol, true);
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    Black76: " << blackFormula * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(blackFormula - multiPrice) < 10e-4)
            BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(blackFormula, multiPrice, 0.1); // Tolerance of 0.1%
    }
}

BOOST_AUTO_TEST_CASE(testBlack76DisplacementLongTerm) {
    // This test again checks the results from the BlackMultilegOptionEngine against
    // a manual implementation of the Black76 formula as before. 
    // This time, a very long maturity time is given. The engine performs the correct
    // calculation for up to 100 periods.
    
    BOOST_TEST_MESSAGE("Testing Black Multileg Option Engine in case of very long maturity ...");

    Calendar calendar = TARGET();
    Date settlementDate(15, July, 2015);
    Date expiryDate(10, July, 2017);
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(expiryDate); // T=2

    auto dc= Actual365Fixed();
    double T = dc.yearFraction(settlementDate, expiryDate);
    double vol = 0.02; // No volatility
    Date startDate(15, July, 2017);
    Settings::instance().evaluationDate() = settlementDate;
    Date maturityDate = calendar.advance(settlementDate, 102, Years);
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
    
    double shift = 0.015;

    Handle<YieldTermStructure> eurYtsHandle(eurYts);
    auto model = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantParametrization>(EURCurrency(), eurYtsHandle, 
                                                                                     volsteptimes_a, eurVols_a, notimes_a, eurKappa_a);

    BOOST_TEST_MESSAGE("Checking Receiver Swaptions ...");
    for (double strike = -0.01; strike < 0.05; strike += 0.01)
    {
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());    
                
        // Get Black Multileg Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());

        double Annuity=0.0;

        for (int i=0;i<fixedLeg.size();++i){
            auto cpn = ext::dynamic_pointer_cast<Coupon>(fixedLeg[i]);
            Annuity += cpn->accrualPeriod() * exp(-eurYts->timeFromReference(cpn->date()) * fixedRate);
        }

        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {true, false};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        auto sqc = ext::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, vola,
                                                                  Actual365Fixed(), ShiftedLognormal, shift);
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        double blackFormula = Annuity * black76_swaption_price(fixedRate+shift, strike+shift, fixedRate, T, vol, false);

        BOOST_TEST_MESSAGE("Receiver Swaption (Strike = " << strike * 100.0 << "%): "<< "T: "<<T);
        BOOST_TEST_MESSAGE("    Black76: " << blackFormula * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(blackFormula - multiPrice) < 10e-4)
            BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(blackFormula, multiPrice, 0.5); // Tolerance of 0.5%
    }

    BOOST_TEST_MESSAGE("Checking Payer Swaptions ...");
    for (double strike = -0.01; strike < 0.05; strike += 0.01)
    {       
        ext::shared_ptr<IborIndex> EURIBOR6m = ext::make_shared<Euribor6M>(eurYtsHandle);
        Schedule schedule(startDate, maturityDate, Period(Semiannual), calendar, Unadjusted, Unadjusted, DateGeneration::Backward, false);
        ext::shared_ptr<VanillaSwap> swap = ext::make_shared<VanillaSwap>(VanillaSwap::Payer, notional, schedule, strike, Actual365Fixed(), 
            schedule, EURIBOR6m,  0.0, Actual365Fixed());     

        // Get Black Multileg Price
        Leg fixedLeg = FixedRateLeg(schedule).withNotionals(1.0).withCouponRates(strike, Actual365Fixed())
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());

        double Annuity=0.0;

        for (int i=0;i<fixedLeg.size();++i){
            auto cpn = ext::dynamic_pointer_cast<Coupon>(fixedLeg[i]);
            Annuity += cpn->accrualPeriod() * exp(-eurYts->timeFromReference(cpn->date()) *  fixedRate);
        }

        Period period(6, Months);
        ext::shared_ptr<IborIndex> liborIndex(new Euribor(period, eurYtsHandle));
        QuantLib::IborLeg floatLeg = QuantLib::IborLeg(schedule, liborIndex).withNotionals(1.0)
            .withPaymentAdjustment(ModifiedFollowing).withPaymentLag(2).withPaymentCalendar(TARGET());
        std::vector<Leg> legs = {floatLeg, fixedLeg};
        std::vector<bool> payer = {false, true};
        std::vector<Currency> currency{QuantLib::EURCurrency(), QuantLib::EURCurrency()};
        
        EuropeanExercise exe(expiryDate);
        QuantLib::ext::shared_ptr<Exercise> exercise =QuantLib::ext::make_shared<EuropeanExercise>(exe);
        ext::shared_ptr<MultiLegOption> swaptionMulti= ext::make_shared<MultiLegOption> (legs, payer, currency, exercise);

        auto sqc = ext::make_shared<ConstantSwaptionVolatility>(settlementDate, NullCalendar(), Following, 
                                                                  vola, Actual365Fixed(), ShiftedLognormal, shift);
        Handle<SwaptionVolatilityStructure> volatilityHandle(sqc);
        QuantLib::ext::shared_ptr<PricingEngine> engineMulti = QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(eurYtsHandle, volatilityHandle);
        swaptionMulti -> setPricingEngine(engineMulti);
        Real multiPrice = swaptionMulti->NPV();
        
        double blackFormula= Annuity* black76_swaption_price(fixedRate+shift, strike+shift, fixedRate, T, vol, true);
        BOOST_TEST_MESSAGE("Payer Swaption (Strike = " << strike * 100.0 << "%): ");
        BOOST_TEST_MESSAGE("    Black76: " << blackFormula * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("    Multileg Model: " << multiPrice * 10000.0 << " bp.");
        BOOST_TEST_MESSAGE("------------");
        
        if (std::fabs(blackFormula - multiPrice) < 10e-4)
            BOOST_CHECK(true);
        else
            BOOST_CHECK_CLOSE(blackFormula, multiPrice, 0.5); // Tolerance of 0.5%
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
