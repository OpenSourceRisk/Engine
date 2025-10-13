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

#include "utilities.hpp"
#include "toplevelfixture.hpp"

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
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
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
#include <ql/time/daycounters/thirty360.hpp>


using namespace QuantLib;
using namespace QuantExt;
using namespace std;

double DF(const vector<double>& path, int t0, int t1, Time dt) {
    return exp(-accumulate(path.begin() + t0, path.begin() + t1, 0.0) * dt);
}

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AnalyticHwSwaptionEngineTest)

BOOST_AUTO_TEST_CASE(testAnalyticalZCB) {
    /* 
     * A 1-factor HW unit test using the analytical solution for ZCB to discount swap payments 
     */

    // Setup test parameters
    Calendar cal = TARGET();
    Date today(10, July, 2025);
    Settings::instance().evaluationDate() = today;

    Array kappa(1, 0.01);
    Matrix sigma(1, 1, 0.01);
    Real strike = 0.02;
    Real forwardRate = 0.02;


    Handle<YieldTermStructure> ts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), forwardRate, Actual365Fixed()));

    auto params = ext::make_shared<IrHwConstantParametrization>(EURCurrency(), ts, sigma, kappa);

    ext::shared_ptr<HwModel> model =
        ext::make_shared<HwModel>(params, IrModel::Measure::BA, HwModel::Discretization::Euler, false);

    // Create swaption and underlying swap
    Date startDate = cal.advance(today, 2 * Days);
    Date exerciseDate = cal.advance(startDate, 2 * Years);
    Date maturityDate = cal.advance(exerciseDate, 5 * Years);
    Handle<YieldTermStructure> curve(ts);
    auto index2 = ext::make_shared<Euribor6M>(curve);

    Schedule fixedSchedule(exerciseDate, maturityDate, Period(Annual), cal, Following, Following, DateGeneration::Forward, false);
    Schedule floatSchedule(exerciseDate, maturityDate, Period(Annual), cal, Following, Following, DateGeneration::Forward, false);
    auto underlying = ext::make_shared<VanillaSwap>(VanillaSwap::Payer, 1.0, fixedSchedule,
        0.02, Thirty360(Thirty360::BondBasis), floatSchedule, index2, 0.02, Actual360());
    
    auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);
    auto swaptionHw = QuantLib::ext::make_shared<Swaption>(underlying, exercise);
    auto swaptionLgm = QuantLib::ext::make_shared<Swaption>(underlying, exercise);

    // Setup MC parameters
    // Testing a 2y5y payer swaption
    Size paths = 1000;
    Time optionTime = 2;
    Time maturityTime = 7; // total product live time
    Time dt = 0.004; // equivalent to 250 steps / year
    Size steps = optionTime / dt;
    Real delta = 1.0; // assume annual payments

    mt19937_64 rng(42);
    normal_distribution<> norm(0.0, 1.0);

    // Create a matrix to store paths evolution
    using Path = vector<double>;
    using PathSet = vector<Path>;
    PathSet pathSet;
    pathSet.reserve(paths);

    Real sumPayoffs = 0.0;

    // Extract swap payment times
    const auto& fixedLeg = swaptionHw->underlying()->fixedLeg();
    vector<Time> payTimes;
    vector<Real> accruals; // deltas (time between payments)
    for (auto const& cf : fixedLeg) {
        auto cpn = ext::dynamic_pointer_cast<FixedRateCoupon>(cf);
        if (!cpn)
            continue;
        payTimes.push_back(ts->timeFromReference(cpn->date()));
        accruals.push_back(cpn->accrualPeriod());
    }

    auto process = model->stateProcess();
    Size nFactors = process->factors();

    for (Size p = 0; p < paths; ++p) {

        Path singlePath;
        Array state = process->initialValues();
        singlePath.reserve(steps + 1);
        singlePath.push_back(state[0]);
      
        Time t = 0.0;

        for (Size step = 0; step < steps; ++step) {

            Array dw(nFactors);

            for (Size k = 0; k < nFactors; ++k) {
                dw[k] = sqrt(dt) * norm(rng);
            }

            state = process->evolve(t, state, dt, dw);
            t += dt;
            singlePath.push_back(state[0]);
        }

        pathSet.push_back(std::move(singlePath));
    }

    // Calculate swaption payoff
    // Store indexes for payment dates
    vector<int> idxPay;
    int idxExp = static_cast<int>(optionTime / dt);
    for (Time t = optionTime + 1; t <= maturityTime; t += 1) {
        idxPay.push_back(static_cast<int>(t / dt));
    }

//    int counter = 0; // initialise counter for debugging
    for (const auto& path : pathSet) {
       
        vector<double> ZCBs;
        double SumZCBs = 0.0;

        // Compute ZCB analitically - P(t,Ti)
        for (int idxTi : idxPay) {
            double bT = idxTi * dt;
            double lT = idxExp * dt;
            double tau = bT - lT;
            double B_T_Ti = (1 - exp(-kappa[0] * tau)) / kappa[0];
            double A_T_Ti = exp(-forwardRate * tau + B_T_Ti * forwardRate -
                                pow(sigma[0][0], 2) / (4 * pow(kappa[0], 3)) *
                                    (exp(-kappa[0] * bT) - exp(-kappa[0] * lT)) * exp(2 * kappa[0] * lT) -
                                1);
            double P_T_Ti = A_T_Ti * exp(-B_T_Ti * path[idxExp]);

            ZCBs.push_back(P_T_Ti);
            SumZCBs += delta * P_T_Ti;
        }
      
        double P_T_T0 = 1; // using approximation here
        double P_T_TN = ZCBs.back();

        double fixedPV = SumZCBs * strike;
        double floatPV = P_T_T0 - P_T_TN;
        double SwapVal = floatPV - fixedPV;
        double payoff = max(SwapVal, 0.0);
       
        Real df_0_T0 = DF(path, 0, idxExp, dt);

        sumPayoffs += (payoff * df_0_T0);

        // counter += 1;
    }

    Real mcPrice = sumPayoffs / static_cast<Real>(paths);

    // Additional setup for the LGM HW adaptor
    Array sigmaDates;
    Array kappaDates;
    Array sigmaLgm = {0.01};

    auto hwAdaptor = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), ts, sigmaDates,
                                                                                          sigmaLgm, kappaDates, kappa);
    auto analyticEngine = ext::make_shared<AnalyticHwSwaptionEngine>(model);
    QuantLib::ext::shared_ptr<PricingEngine> lgmEngine =
        QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(hwAdaptor);
    
 
    swaptionHw->setPricingEngine(analyticEngine);
    swaptionLgm->setPricingEngine(lgmEngine);


    Real analyticPrice = swaptionHw->NPV();
    Real lgmPrice = swaptionLgm->NPV();

    cout << "MC Price: " << mcPrice << endl;
    cout << "Analytic Price: " << analyticPrice << endl;
    cout << "LGM Adaptor Price: " << lgmPrice << endl;
}

BOOST_AUTO_TEST_CASE(testDiscountFactorsFullPath) {
    /*
     * A 1-factor HW unit test using discount factors (function DF) to discount swap payments
     */

    // Setup parameters
    Calendar cal = TARGET();
    Date today(10, July, 2025);
    Settings::instance().evaluationDate() = today;

    Array kappa(1, 0.01);
    Matrix sigma(1, 1, 0.01);
    Real strike = 0.02;

    // Model declaration
    Handle<YieldTermStructure> ts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    auto params = ext::make_shared<IrHwConstantParametrization>(EURCurrency(), ts, sigma, kappa);
    ext::shared_ptr<HwModel> model =
        ext::make_shared<HwModel>(params, IrModel::Measure::BA, HwModel::Discretization::Euler, false);

   // Create swaption and underlying swap
    Date startDate = cal.advance(today, 2 * Days);
    Date exerciseDate = cal.advance(startDate, 2 * Years);
    Date maturityDate = cal.advance(exerciseDate, 5 * Years);
    Handle<YieldTermStructure> curve(ts);
    auto index2 = ext::make_shared<Euribor6M>(curve);

    Schedule fixedSchedule(exerciseDate, maturityDate, Period(Annual), cal, Following, Following,
                           DateGeneration::Forward, false);
    Schedule floatSchedule(exerciseDate, maturityDate, Period(Annual), cal, Following, Following,
                           DateGeneration::Forward, false);
    auto underlying =
        ext::make_shared<VanillaSwap>(VanillaSwap::Payer, 1.0, fixedSchedule, 0.02, Thirty360(Thirty360::BondBasis),
                                      floatSchedule, index2, 0.02, Actual360());

    auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);
    auto swaptionHw = QuantLib::ext::make_shared<Swaption>(underlying, exercise);
    auto swaptionLgm = QuantLib::ext::make_shared<Swaption>(underlying, exercise);

    // MC parameters
    Size paths = 10000;
    Time optionTime = 2;
    Time maturityTime = 7;
    Time dt = 0.004;  // equivalent to 250 steps / year
    Size steps = maturityTime / dt;
    Real delta = 1.0;

    mt19937_64 rng(42);
    normal_distribution<> norm(0.0, 1.0);

    using Path = vector<double>;
    using PathSet = vector<Path>;
    PathSet pathSet;
    pathSet.reserve(paths);

    Real sumPayoffs = 0.0;

    // Extract payment dates
    const auto& fixedLeg = swaptionHw->underlying()->fixedLeg();
    vector<Time> payTimes;
    vector<Real> accruals; // deltas (time between payments)
    for (auto const& cf : fixedLeg) {
        auto cpn = ext::dynamic_pointer_cast<FixedRateCoupon>(cf);
        if (!cpn)
            continue;
        payTimes.push_back(ts->timeFromReference(cpn->date()));
        accruals.push_back(cpn->accrualPeriod());
    }

  
    auto process = model->stateProcess();
    Size nFactors = process->factors();

    for (Size p = 0; p < paths; ++p) {
        Time t = 0.0;
        Path singlePath;
        Array state = process->initialValues();
        singlePath.reserve(steps + 1);
        singlePath.push_back(state[0]);
      
        for (Size step = 0; step < steps; ++step) {

            Array dw(nFactors);

            for (Size k = 0; k < nFactors; ++k) {

                dw[k] = sqrt(dt) * norm(rng);
            }

            state = process->evolve(t, state, dt, dw);
            t += dt;
            singlePath.push_back(state[0]);
        }

        pathSet.push_back(std::move(singlePath));
    }

    // Store indexes for payment dates
    vector<int> idxPay;
    int idxExp = static_cast<int>(optionTime / dt);

    for (Time t = optionTime + 1; t <= maturityTime; t += 1) {
        idxPay.push_back(static_cast<int>(t / dt));
    }

    // int counter = 0;

    for (const auto& path : pathSet) {

        vector<double> dfs;
        double SumPTs = 0.0;

        // Compute discount factors on payment dates
        for (int idxTi : idxPay) {
            double df_T_Ti = exp(-accumulate(path.begin() + idxExp, path.begin() + idxTi, 0.0) * dt);
            dfs.push_back(df_T_Ti);
            SumPTs += delta * df_T_Ti;
        }

        double P_T_T0 = 1; // approximation used here
        double P_T_TN = dfs.back();
   
        double fixedPV = SumPTs * strike;
        double floatPV = P_T_T0 - P_T_TN;
        double SwapVal =  floatPV - fixedPV;
        double payoff = max(SwapVal, 0.0);
        Real df_0_T0 = DF(path, 0, idxExp, dt);

        sumPayoffs += (payoff * df_0_T0);
        // counter += 1;
    }

    Real mcPrice = sumPayoffs / static_cast<Real>(paths);

    // Additional setup for thhe LGM model
    Array sigmaDates;
    Array kappaDates;
    Array sigmaLgm = {0.01};

    auto hwAdaptor = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), ts, sigmaDates,
                                                                                          sigmaLgm, kappaDates, kappa);
    auto analyticEngine = ext::make_shared<AnalyticHwSwaptionEngine>(model);
    QuantLib::ext::shared_ptr<PricingEngine> lgmEngine =
        QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(hwAdaptor);
    
    swaptionHw->setPricingEngine(analyticEngine);
    swaptionLgm->setPricingEngine(lgmEngine);

    Real analyticPrice = swaptionHw->NPV();
    Real lgmPrice = swaptionLgm->NPV();

    cout << "MC Price: " << mcPrice << endl;
    cout << "Analytic Price: " << analyticPrice << endl;
    cout << "LGM adaptor Price: " << lgmPrice << endl;
}

BOOST_AUTO_TEST_CASE(testBuildinMethods) {

    // Setup test parameters
    Calendar cal = TARGET();
    Date today(10, July, 2025);
    Settings::instance().evaluationDate() = today;

    Handle<YieldTermStructure> ts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    Array kappa = {0.01};
    QuantLib::Matrix sigma = {{0.01}};
    Real strike = 0.02;

    // LGM model takes a array sigma (not a matrix; one-factor)
    Array sigmaLgm = {0.01};
    Array sigmaDates;
    Array kappaDates;

    // Model setup
    ext::shared_ptr<IrHwParametrization> params =
        ext::make_shared<IrHwConstantParametrization>(EURCurrency(), ts, sigma, kappa);
    auto hwAdaptor = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), ts, sigmaDates,
                                                                                          sigmaLgm, kappaDates, kappa);
    ext::shared_ptr<HwModel> model = ext::make_shared<HwModel>(params,IrModel::Measure::BA, HwModel::Discretization::Euler, false);

    // Create swaption and underlying swap
    Date startDate = cal.advance(today, 2 * Days);
    Date exerciseDate = cal.advance(startDate, 2 * Years);
    Date maturityDate = cal.advance(exerciseDate, 5 * Years);
    Handle<YieldTermStructure> curve(ts);
    auto index2 = ext::make_shared<Euribor6M>(curve);

    Schedule fixedSchedule(exerciseDate, maturityDate, Period(Annual), cal, Following, Following,
                           DateGeneration::Forward, false);
    Schedule floatSchedule(exerciseDate, maturityDate, Period(Annual), cal, Following, Following,
                           DateGeneration::Forward, false);
    auto underlying =
        ext::make_shared<VanillaSwap>(VanillaSwap::Payer, 1.0, fixedSchedule, 0.02, Thirty360(Thirty360::BondBasis),
                                      floatSchedule, index2, 0.02, Actual360());

    auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);
    auto swaptionHw = QuantLib::ext::make_shared<Swaption>(underlying, exercise);
    auto swaptionLgm = QuantLib::ext::make_shared<Swaption>(underlying, exercise);

    ext::shared_ptr<PricingEngine> hwEngine = boost::make_shared<AnalyticHwSwaptionEngine>(model, ts);
    QuantLib::ext::shared_ptr<PricingEngine> lgmEngine =
         QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(hwAdaptor);

    // Pass pricing engines
    swaptionHw->setPricingEngine(hwEngine);
    swaptionLgm->setPricingEngine(lgmEngine);


    Real analyticalPrice = swaptionHw->NPV();
    Real analyticalLgmPrice = swaptionLgm->NPV();



    BOOST_TEST_MESSAGE("Initializing MC Simulation");
    /*
     * A 1-factor HW unit test using build-in methods within 'model' to price swaption
     */

    // Simulation parameters
    Real maturity = 2.0;
    Size nPaths = 10000;
    Size nSteps = 510;
    Time dt = maturity / nSteps;
    Real sumPayoffs = 0.0;

    MersenneTwisterUniformRng uniRng(42);
    BoxMullerGaussianRng<MersenneTwisterUniformRng> gaussianRng(uniRng);

   const auto& fixedLeg = swaptionHw->underlying()->fixedLeg();
   const auto& schedule = swaptionLgm->underlying()->fixedSchedule();

   std::vector<Time> payTimes;
   std::vector<Real> accruals;
   for (auto const& cf : fixedLeg) {
       auto cpn = ext::dynamic_pointer_cast<FixedRateCoupon>(cf);
       if (!cpn) continue;
       payTimes.push_back(ts->timeFromReference(cpn->date()));
       accruals.push_back(cpn->accrualPeriod());
   }

   Time T0 = ts->timeFromReference(schedule.startDate());
   Time TN = ts->timeFromReference(schedule.endDate());


   Size nFactors = kappa.size();
   //Size nBrownian = sigma.rows();
   mt19937 rng(42);
   normal_distribution<> norm(0.0, 1.0);


   for (Size path = 0; path < nPaths; ++path) {
       auto process = model->stateProcess();
       Array state = process->initialValues();
       Time t = 0.0;
       for (Size step = 0; step < nSteps; ++step) {
           Array dw(nFactors);
           for (Size k = 0; k < nFactors; ++k) {
               dw[k] = sqrt(dt) * norm(rng);
           }
           state = process->evolve(t, state, dt, dw);
           t += dt;
       }

       
        Real P_t_T0 = model->discountBond(maturity, T0, state, ts);
        Real P_t_TN = model->discountBond(maturity, TN, state, ts);

        Real floatingPV = P_t_T0 - P_t_TN;
        Real fixedPV = 0.0;

        for (Size pt = 0; pt < payTimes.size(); ++pt) {
            if (payTimes[pt] <= maturity) continue;
            Real P_t_Ti = model->discountBond(maturity, payTimes[pt], state, ts);
            fixedPV += strike * accruals[pt] * P_t_Ti;
        }

        Real swapPV = floatingPV - fixedPV;
        Real payoff = max(swapPV, 0.0);

        Real num = model->numeraire(maturity, state, ts);
        sumPayoffs += payoff / num;
    }

    Real expectedPayoff = sumPayoffs / nPaths;

    cout << "MC Price: " << expectedPayoff << endl;
    cout << "Analytic Price: " << analyticalPrice << endl;
    cout << "LGM Adaptor Price: " << analyticalLgmPrice << endl;
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
