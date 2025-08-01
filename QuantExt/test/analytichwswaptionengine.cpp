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

BOOST_AUTO_TEST_CASE(testPrintProcess){
    
    BOOST_TEST_MESSAGE("Initializing MC Simulation");
    // Model params
    Size nFactors = 2;
    Array kappa = {0.03, 0.05};
    Array sigma = {0.01, 0.015};
    Real flatRate = 0.02;
    // Swap params
    Real maturity = 1.0;
    Real tenor = 5.0;
    Real strike = 0.02;
    // Simulation params
    Size nPaths = 1;
    Size nSteps = 100;
    Time dt = maturity / nSteps;

    Real sumPayoffs = 0.0;

    MersenneTwisterUniformRng uniRng(42);
    BoxMullerGaussianRng<MersenneTwisterUniformRng> gaussianRng(uniRng);
    
    // Logging
    std::ofstream myfile;
    myfile.open("output.csv");

    // Loop through paths
    for (Size p = 0; p < nPaths; ++p) {
        Array factors(nFactors, 0.0); // track of each factor evolution
        // Loop through the steps
        for (Size s = 0; s < nSteps; ++s) {
            // Model each of the factors x step
            for (Size f = 0; f < nFactors; ++f) {
                Real dw = sqrt(dt) * gaussianRng.next().value;
                Real dx = (-kappa[f] * factors[f] * dt + sigma[f] * dw);
                factors[f] += dx;
            }
            myfile << factors[0] << "," << factors[1] << "\n";
        }

        myfile.close();

        // Compute the short rate at maturity
        // TODO: this can be merged with the previous step
        Real shortRate = 0.0;
        shortRate += flatRate;
        for (Size f = 0; f < nFactors; ++f) {
            shortRate += factors[f];
        }

        // Compute swap value
        Real annuity = 0.0;
        Real floatLeg = 0.0;

        // Assumes annual payments such that deltas is time between payments
        std::cout << "Getting the deltas" << endl;
        for (Size i = 1; i <= tenor; ++i) { 
            std::cout << "The delta is " << i << endl;
            Real df = exp(-shortRate * i);
            annuity += df;
            floatLeg += 0.02 * (i + maturity);
        }
        Real fixLeg = annuity * strike;
        Real payoff = max(floatLeg - fixLeg, 0.0);
        sumPayoffs += payoff * exp(-shortRate * maturity);
    }

    Real expectedPayoff = sumPayoffs / nPaths;
    std::cout << "The expected payoff is: " << expectedPayoff << endl;
};

BOOST_AUTO_TEST_CASE(testAlternativeMC) {
    Date today(10, July, 2025);
    Settings::instance().evaluationDate() = today;

    Array kappa(1, 0.01);
    Matrix sigma(1, 1, 0.01);
    Real strike = 0.02;
    
    //Handle<YieldTermStructure> ts(ext::make_shared<FlatForward>(today, 0.02, Actual365Fixed())); 
    Handle<YieldTermStructure> ts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));

    auto params = ext::make_shared<IrHwConstantParametrization>(EURCurrency(), ts, sigma, kappa);

    //ext::shared_ptr<HwModel> model = ext::make_shared<HwModel>(params);
    ext::shared_ptr<HwModel> model =
        ext::make_shared<HwModel>(params, IrModel::Measure::BA, HwModel::Discretization::Euler, false);


    // Option setup
    auto index = ext::make_shared<EuriborSwapIsdaFixA>(5 * Years, ts);
    Swaption swap = MakeSwaption(index, 2 * Years, strike).withUnderlyingType(VanillaSwap::Payer);
    Swaption swpLgm = MakeSwaption(index, 2 * Years, strike).withUnderlyingType(VanillaSwap::Payer);

    // MC settings
    Size paths = 10000;
    Time optionTime = 2.0; // two years of option
    // NOTE: most accurate result takes place when dt = 0.004
    Time dt = 0.005; // equals to 20 steps x year (maturity / dt = steps)
    Size steps = optionTime / dt;

    mt19937_64 rng(42);
    normal_distribution<> norm(0.0, 1.0);

    Real sumPayoffs = 0.0;

    // Pre-compute swap payments
    const auto& fixedLeg = swap.underlying()->fixedLeg();
    const auto& schedule = swap.underlying()->fixedSchedule();
    vector<Time> payTimes;
    vector<Real> accruals; // deltas (time between payments)
    for (auto const& cf : fixedLeg) {
        auto cpn = ext::dynamic_pointer_cast<FixedRateCoupon>(cf);
        if (!cpn) continue;
        payTimes.push_back(ts->timeFromReference(cpn->date()));
        accruals.push_back(cpn->accrualPeriod());
    }

    Time T0 = ts->timeFromReference(schedule.startDate());
    Time TN = ts->timeFromReference(schedule.endDate());
    Real notional = swap.underlying()->nominal();
    auto process = model->stateProcess();
    //Size nDim = process->size();
    Size nFactors = process->factors();
    cout << "Start date (T0): " << schedule.startDate() << endl;
    cout << "End date (TN): " << schedule.endDate() << endl;

    std::ofstream csv("paths.csv");
    csv << "path,t,x0\n";

    for (Size p = 0; p < paths; ++p) {
        Array state = process->initialValues();
        // print initial state
        if (p == 0) {
            cout << "Array size: " << state.size() << endl;
            cout << "Array: " << state << endl;
        }
        Time t = 0.0;
        for (Size step = 0; step < steps; ++step) {
            Array dw(nFactors);
            for (Size k = 0; k < nFactors; ++k) {
                dw[k] = sqrt(dt) * norm(rng);
            }
            state = process->evolve(t, state, dt, dw);
            t += dt;
            if (p == 0) {
                csv << p << "," << t << "," << state[0] << "\n";
            }
        }

        // Compute swap PV at option expiry
        // Floating leg PV: P(t, T0) - P(t, TN)
        Real P_t_T0 = model->discountBond(optionTime, T0, state, ts);
        Real P_t_TN = model->discountBond(optionTime, TN, state, ts);
        Real floatingPV = notional * (P_t_T0 - P_t_TN);
        // I belive the floatingPV should be aprox. 1
        if (p == 0) {
            cout << "floatingPV: " << floatingPV << endl;
        }

        // Compute fixed leg PV
        Real fixedPv = 0.0;
        for (Size i = 0; i < payTimes.size(); ++i) {
            if (payTimes[i] <= optionTime)
                continue;
            Real P_t_Ti = model->discountBond(optionTime, payTimes[i], state, ts);
            fixedPv += notional * strike * accruals[i] * P_t_Ti;
        }

        Real swapPV = floatingPV - fixedPv;
        Real payoff = max(swapPV, 0.0);
        Real num = model->numeraire(optionTime, state, ts);
        Real discountedPayoff = payoff / num;
        sumPayoffs += discountedPayoff;
    }
    csv.close();


    Real mcPrice = sumPayoffs / static_cast<Real>(paths);
    cout << "MC price: " << mcPrice << endl;

    // Quickly compare against the analytical engine
    auto analyticEngine = ext::make_shared<AnalyticHwSwaptionEngine>(swap, model);
    swap.setPricingEngine(analyticEngine);
    Real analyticPrice = swap.NPV();
    cout << "Analytic price: " << analyticPrice << endl;
    Array sigmaDates;
    Array kappaDates;
    Array sigmaLgm = {0.01}; // LGM model takes a array sigma (not a matrix)

    auto hwAdaptor = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), ts, sigmaDates,
                                                                                          sigmaLgm, kappaDates, kappa);
    QuantLib::ext::shared_ptr<PricingEngine> lgmEngine =
        QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(hwAdaptor);
    swpLgm.setPricingEngine(lgmEngine);
    Real lgmPrice = swpLgm.NPV();
    cout << "LGM adaptor price: " << lgmPrice << endl;
}

double get_df(const vector<double>& path, int t0, int t1, Time dt){
    return exp(-accumulate(path.begin() + t0, path.begin() + t1, 0.0) * dt);
}

BOOST_AUTO_TEST_CASE(testFullPath) {
    Date today(10, July, 2025);
    Settings::instance().evaluationDate() = today;

    Array kappa(1, 0.01);
    Matrix sigma(1, 1, 0.01);
    Real strike = 0.02;

    // Handle<YieldTermStructure> ts(ext::make_shared<FlatForward>(today, 0.02, Actual365Fixed()));
    Handle<YieldTermStructure> ts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));

    auto params = ext::make_shared<IrHwConstantParametrization>(EURCurrency(), ts, sigma, kappa);

    // ext::shared_ptr<HwModel> model = ext::make_shared<HwModel>(params);
    ext::shared_ptr<HwModel> model =
        ext::make_shared<HwModel>(params, IrModel::Measure::BA, HwModel::Discretization::Euler, false);

    // Option setup
    auto index = ext::make_shared<EuriborSwapIsdaFixA>(5 * Years, ts);
    Swaption swap = MakeSwaption(index, 2 * Years, strike).withUnderlyingType(VanillaSwap::Payer);
    Swaption swpLgm = MakeSwaption(index, 2 * Years, strike).withUnderlyingType(VanillaSwap::Payer);

    // MC settings
    Size paths = 10000;
    Time optionTime = 2; // two years of option
    Time maturityTime = 7;
    // NOTE: most accurate result takes place when dt = 0.004
    Time dt = 0.005; // equals to 20 steps x year (maturity / dt = steps)
    Size steps = maturityTime / dt;
    Real delta = 1.0;

    mt19937_64 rng(42);
    normal_distribution<> norm(0.0, 1.0);

    using Path = vector<double>;
    using PathSet = vector<Path>;
    PathSet pathSet;
    pathSet.reserve(paths);

    Real sumPayoffs = 0.0;
    Real sumAltPayoffs = 0.0;

    // Pre-compute swap payments
    const auto& fixedLeg = swap.underlying()->fixedLeg();
    const auto& schedule = swap.underlying()->fixedSchedule();
    vector<Time> payTimes;
    vector<Real> accruals; // deltas (time between payments)
    for (auto const& cf : fixedLeg) {
        auto cpn = ext::dynamic_pointer_cast<FixedRateCoupon>(cf);
        if (!cpn)
            continue;
        payTimes.push_back(ts->timeFromReference(cpn->date()));
        accruals.push_back(cpn->accrualPeriod());
    }

    //Time T0 = ts->timeFromReference(schedule.startDate());
    //Time TN = ts->timeFromReference(schedule.endDate());
    //Real notional = swap.underlying()->nominal();
    auto process = model->stateProcess();
    // Size nDim = process->size();
    Size nFactors = process->factors();
    cout << "Start date (T0): " << schedule.startDate() << endl;
    cout << "End date (TN): " << schedule.endDate() << endl;


    for (Size p = 0; p < paths; ++p) {
        Path singlePath;
        singlePath.reserve(steps + 1);
        Array state = process->initialValues();
        singlePath.push_back(state[0]);
        // print initial state
        if (p == 0) {
            cout << "Array size: " << state.size() << endl;
            cout << "Array: " << state << endl;
        }
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
        pathSet.push_back(move(singlePath));
    }

    // Compute swap value
    vector<Time> paymentDates;
    for (Time t = optionTime + 1; t <= maturityTime; t += 1) {
        paymentDates.push_back(t);
        cout << t << endl;
    }
    cout << paymentDates.size() << endl;
    int idxExp = static_cast<int>(optionTime / dt);
    vector<int> idxPay;
    for (Time t : paymentDates) {
        idxPay.push_back(static_cast<int>(t / dt));
        cout << static_cast<int>(t / dt) << endl;
    }

    int counter = 0;
    for (const auto& path : pathSet) {
        //double df_0_Tepx = exp(-accumulate(path.begin(), path.begin() + idxExp, 0.0) * dt);
        vector<double> dfs;
        double SumPTs = 0.0;
        //double swapVal = 0.0;
        for (int idxTi : idxPay) {
            double df_T_Ti = exp(-accumulate(path.begin() + idxExp, path.begin() + idxTi, 0.0) * dt);
            //double df_T_Ti = get_df(path, idxExp, idxTi, dt);
            dfs.push_back(df_T_Ti);
            SumPTs += delta * df_T_Ti;
        }
        // TODO: payment dates might be incorrect
        double P_T_T0 = dfs.front();
        double P_T_TN = dfs.back();
        //double SwapRate = (P_T_T0 - P_T_TN) / SumPTs;
   
        double fixedPV = SumPTs * strike;
        double floatPV = 1 - P_T_TN;
        double SwapVal =  fixedPV - floatPV;
        Real payoff = max(SwapVal, 0.0);
        if (counter == 0) {
            for (auto const& df : dfs) {
                cout << "This is some discount factor: " << df << endl;
            }
        }
        Real df_0_T0 = get_df(path, 0, idxExp, dt);

        sumPayoffs += (payoff * df_0_T0);

        if (counter == 0) {
            cout << "Printing in priceing" << endl;
            cout << "P_T_T0: " << P_T_T0 << endl;
            cout << "P_T_TN: " << P_T_TN << endl;
            cout << "fixedPV: " << fixedPV << endl;
            cout << "floatPV: " << floatPV << endl;
            cout << "SumPTs: " << SumPTs << endl;
            cout << "df_0_T0: " << df_0_T0 << endl;
        }

        Real altSwapVal = 1 - fixedPV;
        Real altPayoff = max(altSwapVal, 0.0);
        sumAltPayoffs += (altPayoff * df_0_T0);
        counter += 1;
    }

    Real mcPrice = sumPayoffs / static_cast<Real>(paths);
    Real altMcPrice = sumAltPayoffs / static_cast<Real>(paths);
    cout << "MC price: " << mcPrice << endl;
    cout << "MC Alt price: " << altMcPrice << endl;

    // Quickly compare against the analytical engine
    auto analyticEngine = ext::make_shared<AnalyticHwSwaptionEngine>(swap, model);
    swap.setPricingEngine(analyticEngine);
    Real analyticPrice = swap.NPV();
    cout << "Analytic price: " << analyticPrice << endl;
    Array sigmaDates;
    Array kappaDates;
    Array sigmaLgm = {0.01}; // LGM model takes a array sigma (not a matrix)

    auto hwAdaptor = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), ts, sigmaDates,
                                                                                          sigmaLgm, kappaDates, kappa);
    QuantLib::ext::shared_ptr<PricingEngine> lgmEngine =
        QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(hwAdaptor);
    swpLgm.setPricingEngine(lgmEngine);
    Real lgmPrice = swpLgm.NPV();
    cout << "LGM adaptor price: " << lgmPrice << endl;
    cout << pathSet.size() << endl;
    cout << pathSet[0].size() << endl;
    cout << pathSet[0][0] << endl;
}

BOOST_AUTO_TEST_CASE(testAnalyticalvsSimulation) {
    Date today(10, July, 2025);
    Settings::instance().evaluationDate() = today;
    BOOST_TEST_MESSAGE("Initializing Analytical Soltuion");

    //Handle<YieldTermStructure> ts(boost::make_shared<FlatForward>(today, 0.02, Actual360()));
    Handle<YieldTermStructure> ts(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));


    Array kappa = {0.01};
    QuantLib::Matrix sigma = {{0.01}};
    Real strike = 0.02;

    Array sigmaLgm = {0.01}; // LGM model takes a array sigma (not a matrix)
    

    //Array kappa = {1.18575, 0.0189524, 0.0601251, 0.0709152};
    //QuantLib::Matrix sigma = {{-0.0122469, 0.0105949, 0, 0}, {0, 0, -0.117401, 0.122529}};

    // HW model
    ext::shared_ptr<IrHwParametrization> params =
        ext::make_shared<IrHwConstantParametrization>(EURCurrency(), ts, sigma, kappa);
    ext::shared_ptr<HwModel> model = ext::make_shared<HwModel>(params);

    // Underlying declaration
    auto index = ext::make_shared<EuriborSwapIsdaFixA>(5 * Years, ts);
    Swaption swp = MakeSwaption(index, 2 * Years, strike).withUnderlyingType(VanillaSwap::Payer);


    // Create another swap for the LGM model(s) (same as prev swap)
    //IrLgm1fParametrization
    Swaption swpLgm = MakeSwaption(index, 2 * Years, strike).withUnderlyingType(VanillaSwap::Payer);
    //LinearGaussMarkovModel
    Swaption swpLgmTwo = MakeSwaption(index, 2 * Years, strike).withUnderlyingType(VanillaSwap::Payer);
   
    // Define price engine
    ext::shared_ptr<PricingEngine> hw_engine = boost::make_shared<AnalyticHwSwaptionEngine>(swp, model, ts);
    
    /*auto swpUnderlyingFixed = swpLgm.underlying()->fixedSchedule();
    std::cout << "Print type " << typeid(swpUnderlyingFixed).name() << endl;
    std::cout << "Print item " << swpUnderlyingFixed[0] << endl;
    std::cout << "Print start " << swpUnderlyingFixed.startDate() << endl;
    std::cout << "Print size " << swpUnderlyingFixed.size() << endl;*/
    //Date startDate = swpUnderlyingFixed.startDate();
    // cout << "Test array: " << sigmaDates[0] << endl;

    // LGM takes date array to know when to time-vary the sigma/kappa
    // Thus, given constant params we don't vary our inputs (empty array)
    Array sigmaDates;
    Array kappaDates;

    // Define LGM HW adaptor engine
    auto hwAdaptor = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), ts, sigmaDates,sigmaLgm, kappaDates, kappa);
    QuantLib::ext::shared_ptr<PricingEngine> lgmEngine =
         QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(hwAdaptor);
    // This is an additional way I found where one cass pass in the HW adaptor to the LGM engine
    auto lgmTwo = ext::make_shared<LinearGaussMarkovModel>(hwAdaptor);
    QuantLib::ext::shared_ptr<PricingEngine> lgmEngine2 = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(lgmTwo);

    // Pass pricing engines
    swp.setPricingEngine(hw_engine);
    swpLgm.setPricingEngine(lgmEngine);
    swpLgmTwo.setPricingEngine(lgmEngine2);

    Real analyticalPrice = swp.NPV();
    Real analyticalLgmPrice = swpLgm.NPV();
    Real analyticalLgmPriceTwo = swpLgmTwo.NPV();

    cout << "Analytical Price: " << analyticalPrice << endl;
    cout << "The LGM adaptor price: " << analyticalLgmPrice << endl;
    cout << "The alternative LGM adaptor price: " << analyticalLgmPriceTwo << endl;


    BOOST_TEST_MESSAGE("Initializing MC Simulation");
    // Model params
    //Size nFactors = 2;
    //Array kappa = {0.03, 0.05}; // mean reversion speed
  
    // Swap params
    Real maturity = 2.0;
    Real flatRate = 0.02;
    //Real tenor = 5.0;

    // Simulation params
    Size nPaths = 1000;
    Size nSteps = 400;
    Time dt = maturity / nSteps;
    Real sumPayoffs = 0.0;

    MersenneTwisterUniformRng uniRng(42);
    BoxMullerGaussianRng<MersenneTwisterUniformRng> gaussianRng(uniRng);

   const auto& fixedLeg = swp.underlying()->fixedLeg();
   const auto& schedule = swp.underlying()->fixedSchedule();

   std::vector<Time> payTimes;
   std::vector<Real> accruals;
   for (auto const& cf : fixedLeg) {
       auto cpn = ext::dynamic_pointer_cast<FixedRateCoupon>(cf);
       if (!cpn) continue;
       //cout << ts->timeFromReference(cpn->date()) << "(Time from refencer)" << cf->date() << endl;
       //cout << "This is accrual period: " << cpn->accrualPeriod() << endl;
       payTimes.push_back(ts->timeFromReference(cpn->date()));
       accruals.push_back(cpn->accrualPeriod());
   }

   Time T0 = ts->timeFromReference(schedule.startDate());
   Time TN = ts->timeFromReference(schedule.endDate());

   //cout << "This is T0: " << T0 << " and TN: " << TN << endl;

   Real notional = swp.underlying()->nominal();
   //std::cout << "This is the notional (should be 1): " << notional << std::endl;

   Size nFactors = kappa.size();
   //Size nBrownian = sigma.rows();
   mt19937 rng(42);
   normal_distribution<> stdNormal(0.0, 1.0);


   // TODO: extend this to nFactors
   auto getTheta = [](Real& t, Real& flatRate, Real& kappa, Real& sigma) {
       return kappa * flatRate + (std::pow(sigma, 2) / (2 * kappa)) * (1 - exp(-2 * kappa * t));
   };

   // This is a simplified one factor simulation
   for (Size path = 0; path < nPaths; ++path) {
       Array factors(nFactors, flatRate); // TODO: this should start at 0 and not in 

       for (Size step = 0; step < nSteps; ++step) {
           Real t = step * dt;
           Real theta = getTheta(t, flatRate, kappa[0], sigma[0][0]);
           Real dW = stdNormal(rng) * sqrt(dt);
           Real dr = (theta - kappa[0] * factors[0]) * dt + sigma[0][0] * dW;
           factors += dr;
                   
       }
       // THE CODE BELOW IS FOR THE MULTIFACTOR. However, for preliminary testing I'm using the above
       // single factor loop proces

       /*
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
               Real drift = -kappa[i] * factors[i];
               factors[i] += drift * dt + diffusion;
           }
       }*/

      
        // TODO: this can be merged with the previous step
        Real shortRate = 0.0;
        for (Size f = 0; f < nFactors; ++f) {
            shortRate += factors[f];
        }

        Array state(1, shortRate);
        // Calculate discount factors
        // Compute P(t,T0) and P(t, TN)
        //cout << "Maturity: " << maturity << ", T0: " << T0 << ", TN: " << TN << endl;
        Real P_t_T0 = model->discountBond(maturity, T0, factors, ts);
        //cout << "P_t_T0: " << P_t_T0 << endl;
        Real P_t_TN = model->discountBond(maturity, TN, factors, ts);
        // cout << "P_t_TN: " << P_t_TN << endl;
        Real floatingPV = notional * (P_t_T0 - P_t_TN);


        Real fixedPV = 0.0;
        for (Size pt = 0; pt < payTimes.size(); ++pt) {
            if (payTimes[pt] <= maturity) continue;
            Real P_t_Ti = model->discountBond(maturity, payTimes[pt], factors, ts);
            fixedPV += notional * strike * accruals[pt] * P_t_Ti;
        }

        Real swapPV = floatingPV - fixedPV;
        Real payoff = max(swapPV, 0.0);

        Real num = model->numeraire(maturity, factors, ts); // TODO this numeraire might be wrong
        sumPayoffs += payoff / num;
    }

    Real expectedPayoff = sumPayoffs / nPaths;
    std::cout << "The sum payoff is: " << sumPayoffs << endl;
    std::cout << "The expected payoff is: " << expectedPayoff << endl;
};

BOOST_AUTO_TEST_CASE(testMCSimulation) {
    Array kappa = {0.01};
    Matrix sigma = {{0.01}};
    Size nPaths = 1000;
    Time maturity = 2.0;
    Time tenor = 5.0;
    Rate strike  = 0.02;
    Size nSteps = 100;
    Time dt = 0.02;
    cout << "dt: " << dt << endl;
    cout << "dt: " << maturity / nSteps << endl;
    Rate flatRate = 0.02;

    Size nFactors = kappa.size();
    Size nBrownian = sigma.rows();

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
                // TODO: missing theta
                Real drift = -kappa[i] * factors[i];
                factors[i] += drift * dt + diffusion;
            }
        }

        // Add factors
        Real rT = 0.02;
        for (Size i = 0; i < nFactors; ++i) {
            rT += factors[i];
        }

        // Swap value at exercise
        Real annuity = 0.0, floatLeg = 0.0;
        for (Size i = 1; i <= tenor; ++i) {
            DiscountFactor df = exp(-rT * i);
            annuity += df;
            floatLeg += df * flatRate;
        }

        Real fixedLeg = strike * annuity;
        Real swapValue = floatLeg - fixedLeg;
        Real payoff = max(swapValue, 0.0);
        // Discout back to T0
        DiscountFactor df0T = exp(-flatRate * maturity);
        sumPayoffs += payoff * df0T;
    }

    Real NPV = sumPayoffs / nPaths;
    cout << "NPV: " << NPV << endl;
}
