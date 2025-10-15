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
#include <qle/models/hwpiecewiseparametrization.hpp>
//#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
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

BOOST_AUTO_TEST_CASE(testPiecewiseConstructor) { 
	// Define constant parametrization
    Real forwardRate = 0.02;
    Array kappa = {1.18575, 0.0189524, 0.0601251, 0.079152};
    Matrix sigma{{-0.0122469, 0.0105959, 0, 0}, {0, 0, -0.117401, 0.122529}};
    Handle<YieldTermStructure> ts(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), forwardRate, Actual365Fixed()));

	auto constantParams = ext::make_shared<IrHwConstantParametrization>(EURCurrency(), ts, sigma, kappa);
	
	// Define piecewise constant parametrization
	Array times = {5.0};
    vector<Array> piecewiseKappa = {{1.18575, 0.0189524, 0.0601251, 0.079152}, {1.181209, 0.52398, 0.0601251, 0.122529}};
    vector<Matrix> piecewiseSigma = {{{-0.0122469, 0.0105959, 0, 0}, {0, 0, -0.117401, 0.122529}},
                                      {{-0.024242, 0.0105959, 0, 0}, {0, 0, 0.324324, 0.122529}}};

	auto piecewiseParams =
        ext::make_shared<IrHwPiecewiseParametrization>(EURCurrency(), ts, times, piecewiseSigma, piecewiseKappa);

	// Test correct inner dimensions of parameters
    BOOST_TEST(constantParams->n() == piecewiseParams->n());
    BOOST_TEST(constantParams->m() == piecewiseParams->m());
    
	// Test piecewise constant is selecting correct params
    BOOST_TEST(piecewiseParams->kappa(2.5) == piecewiseKappa[0]);
    BOOST_TEST(piecewiseParams->kappa(5.0) == piecewiseKappa[0]);
    BOOST_TEST(piecewiseParams->kappa(8.4) == piecewiseKappa[1]);

    BOOST_TEST(piecewiseParams->sigma_x(0.0) == piecewiseSigma[0]);
    BOOST_TEST(piecewiseParams->sigma_x(25.93) == piecewiseSigma[1]);
}

BOOST_AUTO_TEST_CASE(testPiecewiseAsConstant) {
    Real forwardRate = 0.02;
    Array kappa = {1.18575, 0.0189524, 0.0601251, 0.079152};
    Matrix sigma{{-0.0122469, 0.0105959, 0, 0}, {0, 0, -0.117401, 0.122529}};
    Handle<YieldTermStructure> ts(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), forwardRate, Actual365Fixed()));

    auto constantParams = ext::make_shared<IrHwConstantParametrization>(EURCurrency(), ts, sigma, kappa);
    
    Array times = {};
    vector<Array> piecewiseKappa = {{1.18575, 0.0189524, 0.0601251, 0.079152}};
    vector<Matrix> piecewiseSigma = {{{-0.0122469, 0.0105959, 0, 0}, {0, 0, -0.117401, 0.122529}}};
    auto piecewiseParams =
        ext::make_shared<IrHwPiecewiseParametrization>(EURCurrency(), ts, times, piecewiseSigma, piecewiseKappa);

    // Create swaption and underlying swap
    Calendar cal = TARGET();
    Date today(10, July, 2025);
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
    auto swaptionConstant = QuantLib::ext::make_shared<Swaption>(underlying, exercise);
    auto swaptionPiecewise = QuantLib::ext::make_shared<Swaption>(underlying, exercise);

    ext::shared_ptr<HwModel> constantModel =
        ext::make_shared<HwModel>(constantParams, IrModel::Measure::BA, HwModel::Discretization::Euler, false);
    ext::shared_ptr<HwModel> piecewiseModel =
        ext::make_shared<HwModel>(piecewiseParams, IrModel::Measure::BA, HwModel::Discretization::Euler, false);

    ext::shared_ptr<PricingEngine> hwConstantEngine =
        boost::make_shared<AnalyticHwSwaptionEngine>(constantModel, ts);
    ext::shared_ptr<PricingEngine> hwPiecewiseEngine =
        boost::make_shared<AnalyticHwSwaptionEngine>(piecewiseModel, ts);



    swaptionConstant->setPricingEngine(hwConstantEngine);
    swaptionPiecewise->setPricingEngine(hwPiecewiseEngine);

    Real constantPrice = swaptionConstant->NPV();
    Real piecewisePrice = swaptionPiecewise->NPV();

    BOOST_TEST(constantPrice == piecewisePrice);
}

BOOST_AUTO_TEST_CASE(testPiecewiseConstant) {
    Real forwardRate = 0.02;
    
    Handle<YieldTermStructure> ts(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), forwardRate, Actual365Fixed()));
    Array times = {3.0};
    vector<Array> piecewiseKappa = {{0.5, 0.10},
                                    {0.1, 0.15}};
    vector<Matrix> piecewiseSigma = {{{-0.01, 0}, {0, 0.12}},
                                     {{-0.02, 0}, {0, 0.05}}};

    auto piecewiseParams =
        ext::make_shared<IrHwPiecewiseParametrization>(EURCurrency(), ts, times, piecewiseSigma, piecewiseKappa);

    // Create swaption and underlying swap
    Calendar cal = TARGET();
    Date today(10, July, 2025);
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
    auto swaption = QuantLib::ext::make_shared<Swaption>(underlying, exercise);

    ext::shared_ptr<HwModel> piecewiseModel =
        ext::make_shared<HwModel>(piecewiseParams, IrModel::Measure::BA, HwModel::Discretization::Euler, false);

    ext::shared_ptr<PricingEngine> hwPiecewiseEngine = boost::make_shared<AnalyticHwSwaptionEngine>(piecewiseModel, ts);

    swaption->setPricingEngine(hwPiecewiseEngine);
    Real price = swaption->NPV();

    cout << "Price of piecewise constant swaption: " << price << endl;

    // TODO: continue testing here...
}
