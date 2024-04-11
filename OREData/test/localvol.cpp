/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include <ored/scripting/scriptengine.hpp>
#include <ored/scripting/models/localvol.hpp>
#include <ored/scripting/staticanalyser.hpp>
#include <ored/scripting/scriptparser.hpp>
#include <ored/scripting/astprinter.hpp>
#include <ored/scripting/models/blackscholes.hpp>
#include <ored/scripting/models/dummymodel.hpp>

#include <oret/toplevelfixture.hpp>

#include <ored/model/localvolmodelbuilder.hpp>

#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

#include <ql/exercise.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/stochasticprocessarray.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/sabr.hpp>

#include <iomanip>

using namespace ore::data;
using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(LocalVolTest)

namespace {
void testCalibrationInstrumentRepricing(const std::vector<Date>& expiries, const std::vector<Real>& moneyness,
                                        const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                        const Size timeStepsPerYear, const Size paths, const Real tol) {

    // set up a local vol model with simulation dates = expiries, calibrated to options (expiry, moneyness) with
    // the expiry taken from the expiries vector and the moneyness taken from the moneyness vector

    std::set<Date> simDates(expiries.begin(), expiries.end());

    LocalVolModelBuilder builder(process->riskFreeRate(), process, simDates, std::set<Date>(), timeStepsPerYear,
                                 LocalVolModelBuilder::Type::AndreasenHuge, moneyness, false);

    Model::McParams mcParams;
    mcParams.regressionOrder = 1;
    auto localVol = QuantLib::ext::make_shared<LocalVol>(paths, "EUR", process->riskFreeRate(), "EQ-DUMMY", "EUR",
                                                 builder.model(), mcParams, simDates);

    // loop over the calibration options and price them in the local vol model using MC
    // the result should be close to the market price of the options

    // engine to compute the market price
    auto marketEngine = QuantLib::ext::make_shared<AnalyticEuropeanEngine>(process);

    // context against we can run the script engine, add some variables here, the rest follows below
    auto context = QuantLib::ext::make_shared<Context>();
    context->scalars["Option"] = RandomVariable(paths, 0.0);
    context->scalars["Underlying"] = IndexVec{paths, "EQ-DUMMY"};
    context->scalars["PayCcy"] = CurrencyVec{paths, "EUR"};

    // script engine to price a vanilla option
    ScriptEngine scriptEngine(
        ScriptParser("Option = PAY( max( PutCall * (Underlying(Expiry)-Strike), 0), Expiry, Expiry, PayCcy );").ast(),
        context, localVol);

    Real maxError = 0.0;
    for (Size i = 0; i < expiries.size(); ++i) {
        // compute the atm forward level for the given expiry
        Real atmStrike = process->x0() / process->riskFreeRate()->discount(expiries[i]) *
                         process->dividendYield()->discount(expiries[i]);

        for (Size j = 0; j < moneyness.size(); ++j) {

            // skip check for options that are too far out of the money
            Real t = process->riskFreeRate()->timeFromReference(expiries[i]);
            if (std::fabs(moneyness[j]) > 3.72)
                continue;

            // set up the option and compute the market price
            Option::Type optionType = moneyness[j] > 1.0 ? Option::Call : Option::Put;
            Real strike =
                atmStrike * std::exp(process->blackVolatility()->blackVol(t, atmStrike) * std::sqrt(t) * moneyness[j]);
            VanillaOption option(QuantLib::ext::make_shared<PlainVanillaPayoff>(optionType, strike),
                                 QuantLib::ext::make_shared<EuropeanExercise>(expiries[i]));
            option.setPricingEngine(marketEngine);
            Real marketPrice = option.NPV();

            // price the option in the script engine
            context->scalars["PutCall"] = RandomVariable(paths, optionType == Option::Call ? 1.0 : -1.0);
            context->scalars["Expiry"] = EventVec{paths, expiries[i]};
            context->scalars["Strike"] = RandomVariable(paths, strike);
            scriptEngine.run();
            Real scriptPrice = expectation(QuantLib::ext::get<RandomVariable>(context->scalars["Option"])).at(0);
            // check the market price and the script price are close
            BOOST_TEST_MESSAGE("expiry=" << QuantLib::io::iso_date(expiries[i]) << " moneyness=" << moneyness[j]
                                         << " marketVol = " << process->blackVolatility()->blackVol(t, strike, true)
                                         << " marketPrice=" << marketPrice << " mcPrice=" << scriptPrice
                                         << " error=" << (scriptPrice - marketPrice));
            BOOST_CHECK_SMALL(scriptPrice - marketPrice, tol);
            maxError = std::max(maxError, scriptPrice - marketPrice);
        }
    }
    BOOST_TEST_MESSAGE("max error = " << maxError);
}
} // namespace

BOOST_AUTO_TEST_CASE(testFlatVols) {
    BOOST_TEST_MESSAGE("Testing LocalVol with flat input vols...");

    Date ref(7, May, 2019);
    Settings::instance().evaluationDate() = ref;

    std::vector<Date> expiries{ref + 1 * Months, ref + 3 * Months, ref + 6 * Months, ref + 9 * Months,
                               ref + 1 * Years,  ref + 2 * Years,  ref + 3 * Years,  ref + 4 * Years,
                               ref + 5 * Years,  ref + 7 * Years,  ref + 10 * Years};

    std::vector<Real> moneyness{-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0};

    Handle<YieldTermStructure> r(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    Handle<YieldTermStructure> q(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.03, Actual365Fixed()));
    Handle<BlackVolTermStructure> vol(QuantLib::ext::make_shared<BlackConstantVol>(0, NullCalendar(), 0.10, Actual365Fixed()));
    Handle<Quote> spot(QuantLib::ext::make_shared<SimpleQuote>(100.0));

    auto process = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(spot, q, r, vol);

    testCalibrationInstrumentRepricing(expiries, moneyness, process, 20, 10000, 0.30);
}

BOOST_AUTO_TEST_CASE(testSabrVols) {
    BOOST_TEST_MESSAGE("Testing LocalVol with sabr input vols...");

    Date ref(7, May, 2019);
    Settings::instance().evaluationDate() = ref;

    std::vector<Date> expiries{ref + 1 * Months, ref + 3 * Months, ref + 6 * Months, ref + 9 * Months,
                               ref + 1 * Years,  ref + 2 * Years,  ref + 3 * Years,  ref + 4 * Years,
                               ref + 5 * Years,  ref + 7 * Years,  ref + 10 * Years};

    std::vector<Real> moneyness{-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0};

    Handle<YieldTermStructure> r(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    Handle<YieldTermStructure> q(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.03, Actual365Fixed()));

    class SabrTestSurface : public BlackVolatilityTermStructure {
    public:
        SabrTestSurface(const Handle<Quote>& spot, const Handle<YieldTermStructure>& r,
                        const Handle<YieldTermStructure>& q)
            : BlackVolatilityTermStructure(0, NullCalendar(), Following, ActualActual(ActualActual::ISDA)), spot_(spot), r_(r), q_(q) {}
        Date maxDate() const override { return Date::maxDate(); }
        Real minStrike() const override { return 0.0; }
        Real maxStrike() const override { return QL_MAX_REAL; }

    private:
        Real blackVolImpl(Time maturity, Real strike) const override {
            Real forward = spot_->value() / r_->discount(maturity) * q_->discount(maturity);
            Real w2 = std::min(maturity, 10.0) / 10.0, w1 = 1.0 - w2;
            Real alpha = 0.17 * w1 + 0.10 * w2;
            Real beta = 0.99;
            Real nu = 0.3 * w1 + 0.05 * w2;
            Real rho = -0.2;
            return sabrVolatility(strike, forward, maturity, alpha, beta, nu, rho);
        }
        Handle<Quote> spot_;
        Handle<YieldTermStructure> r_, q_;
    };

    Handle<Quote> spot(QuantLib::ext::make_shared<SimpleQuote>(100.0));
    Handle<BlackVolTermStructure> vol(QuantLib::ext::make_shared<SabrTestSurface>(spot, r, q));

    auto process = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(spot, q, r, vol);

    testCalibrationInstrumentRepricing(expiries, moneyness, process, 20, 10000, 0.30);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
