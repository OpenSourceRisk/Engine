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

/*! \file portfolio/builders/fxoption.hpp
    \brief
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/math/solvers1d/bisection.hpp>
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <ql/pricingengines/vanilla/baroneadesiwhaleyengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/andreasenhugelocalvoladapter.hpp>
#include <ql/termstructures/volatility/equityfx/andreasenhugevolatilityinterpl.hpp>

namespace ore {
namespace data {

namespace {
    struct VanillaOptionData {
        Real strike;
        Time maturity;
        Option::Type optionType;
    };
    ext::shared_ptr<VanillaOption> makeVanillaOption(const VanillaOptionData& params) {
        Date maturity = Date(Settings::instance().evaluationDate())
                                          + Period(Size(params.maturity*365), Days);
        ext::shared_ptr<Exercise> exercise(new EuropeanExercise(maturity));
        ext::shared_ptr<StrikedTypePayoff> payoff(
                    new PlainVanillaPayoff(params.optionType, params.strike));
        return ext::make_shared<VanillaOption>(payoff, exercise);
    }

    typedef std::pair<ext::shared_ptr<VanillaOption>, ext::shared_ptr<Quote>> CalibrationDatum;
    CalibrationDatum makeCalibrationDatum(boost::shared_ptr<PricingEngine> europeanEngine,
                                          const VanillaOptionData& params) {
        auto v = makeVanillaOption(params);
        v->setPricingEngine(europeanEngine);
        boost::shared_ptr<Quote> quote = boost::make_shared<SimpleQuote>(v->NPV());
        return { v, quote };
    }

    class OptionDelta {
        public:
            OptionDelta(const boost::shared_ptr<PricingEngine>& engine, Time maturity, Option::Type optionType, Real target)
            : engine_(engine), maturity_(maturity), optionType_(optionType), target_(target) {};
            Real operator()(Real x) const {
                auto v = makeVanillaOption({ x, maturity_, optionType_ });
                v->setPricingEngine(engine_);
                return v->delta() - target_;
            };
        private:
            boost::shared_ptr<PricingEngine> engine_;
            Time maturity_;
            Option::Type optionType_;
            Real target_;
    };

    std::vector<CalibrationDatum> makeCalibrationData(boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp,
                                                      const std::vector<Period>& expiries,
                                                      const std::vector<Real>& deltas) {
        QL_REQUIRE(expiries.size() > 0, "expiry list for calibration cannot be empty");

        boost::shared_ptr<PricingEngine> engine = boost::make_shared<AnalyticEuropeanEngine>(gbsp);
        std::vector<CalibrationDatum> calibrationSet;

        Bisection bisection;
        bisection.setLowerBound(0.0);

        for (auto expiry : expiries) {
            
            Time maturity = 0;
            switch (expiry.units()) {
                case TimeUnit::Days:
                    maturity = days(expiry) / 365.0;
                    break;
                case TimeUnit::Weeks:
                    maturity = weeks(expiry) / 365.0 * 7;
                    break;
                case TimeUnit::Months:
                    maturity = months(expiry) / 12.0;
                    break;
                case TimeUnit::Years:
                    maturity = years(expiry);
                    break;
                default:
                    QL_FAIL("unsupported time unit in " << expiry);
            }


            Real spot = gbsp->x0();

            // ATM Call
            auto atmCall = makeVanillaOption({spot, maturity, Option::Type::Call});
            atmCall->setPricingEngine(engine);
            boost::shared_ptr<Quote> quote = boost::make_shared<SimpleQuote>(
                atmCall->impliedVolatility(atmCall->NPV(), gbsp));
            calibrationSet.push_back({atmCall, quote});

            // ATM Put
            auto atmPut = makeVanillaOption({spot, maturity, Option::Type::Put});
            atmPut->setPricingEngine(engine);
            quote = boost::make_shared<SimpleQuote>(
                atmPut->impliedVolatility(atmPut->NPV(), gbsp));
            calibrationSet.push_back({atmPut, quote});

            Real strike = spot;
            for (auto delta : deltas) {
                Option::Type type = delta > 0 ? Option::Type::Call : Option::Type::Put;
                OptionDelta deltaFunc(engine, maturity, type, delta);
                
                try {
                    strike = bisection.solve(deltaFunc, 1e-4, strike, 0.001);
                }
                catch(QuantLib::Error e) { continue; };

                auto v = makeVanillaOption({strike, maturity, type});
                v->setPricingEngine(engine);
                boost::shared_ptr<Quote> quote = boost::make_shared<SimpleQuote>(
                    v->impliedVolatility(v->NPV(), gbsp));
                calibrationSet.push_back({v, quote});
            }
        }
        return calibrationSet;
    }
}

//! Abstract Engine Builder for FX Options
/*! Pricing engines are cached by currency pair

    \ingroup builders
 */
class FxOptionEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&, const Currency&> {
public:
    FxOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes)
        : CachingEngineBuilder(model, engine, tradeTypes) {}

protected:
    virtual string keyImpl(const Currency& forCcy, const Currency& domCcy) override {
        return forCcy.code() + domCcy.code();
    }

    boost::shared_ptr<GeneralizedBlackScholesProcess> getBlackScholesProcess(const Currency& forCcy, const Currency& domCcy,
                                                                             const bool localVol = false,
                                                                             const std::string& localVolType = "") {
        string pair = keyImpl(forCcy, domCcy);
        const Handle<Quote>& spot = market_->fxSpot(pair, configuration(ore::data::MarketContext::pricing));
        const Handle<YieldTermStructure>& rTS = market_->discountCurve(
            domCcy.code(), configuration(ore::data::MarketContext::pricing));
        const Handle<YieldTermStructure>& qTS = market_->discountCurve(
            forCcy.code(), configuration(ore::data::MarketContext::pricing));
        const Handle<BlackVolTermStructure>& blackVolTS = market_->fxVol(
            pair, configuration(ore::data::MarketContext::pricing));

        if (!localVol || localVolType == "Dupire")
            return boost::make_shared<GeneralizedBlackScholesProcess>(spot, qTS, rTS, blackVolTS);
        if (localVolType == "AndreasenHuge") {

            std::vector<Period> expiries = parseListOfValues<Period>(
                engineParameter("AndreasenHugeExpiries"), &parsePeriod);
            std::vector<Real> deltas = parseListOfValues<Real>(
                engineParameter("AndreasenHugeDeltas"), &parseReal);

            boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(forCcy, domCcy);
            AndreasenHugeVolatilityInterpl::CalibrationSet calibrationSet = makeCalibrationData(
                gbsp, expiries, deltas);

            auto volInterpl = boost::make_shared<AndreasenHugeVolatilityInterpl>(calibrationSet, spot, rTS, qTS);
            Handle<LocalVolTermStructure> adaptor(boost::make_shared<AndreasenHugeLocalVolAdapter>(volInterpl));
            return boost::make_shared<GeneralizedBlackScholesProcess>(spot, qTS, rTS, blackVolTS, adaptor);
        }
        QL_FAIL("unknown local volatility type: " << localVolType);
    }
};

//! Engine Builder for European FX Options
/*! Pricing engines are cached by currency pair

    \ingroup builders
 */
class FxEuropeanOptionEngineBuilder : public FxOptionEngineBuilder {
public:
    FxEuropeanOptionEngineBuilder() : FxOptionEngineBuilder("GarmanKohlhagen", "AnalyticEuropeanEngine", {"FxOption"}) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy) override {
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(forCcy, domCcy);
        return boost::make_shared<AnalyticEuropeanEngine>(gbsp);
    }
};

//! Abstract Engine Builder for FX American Options
/*! Pricing engines are cached by currency pair

    \ingroup portfolio
 */
class FxAmericanOptionEngineBuilder : public FxOptionEngineBuilder {
protected:
    FxAmericanOptionEngineBuilder(const string& model, const string& engine)
        : FxOptionEngineBuilder(model, engine, {"FxAmericanOption"}) {}
};

//! Engine Builder for FX American Options using Finite Difference Method
/*! Pricing engines are cached by currency pair

    \ingroup portfolio
 */
class FxAmericanOptionFDEngineBuilder
    : public FxAmericanOptionEngineBuilder {
public:
    FxAmericanOptionFDEngineBuilder()
        : FxAmericanOptionEngineBuilder("GarmanKohlhagen", "FdBlackScholesVanillaEngine") {}

protected:

    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy) override {
        std::string scheme = engineParameter("Scheme");
        Size tGrid = ore::data::parseInteger(engineParameter("TimeGrid"));
        Size xGrid = ore::data::parseInteger(engineParameter("XGrid"));
        Size dampingSteps = ore::data::parseInteger(engineParameter("DampingSteps"));
        bool localVol = ore::data::parseBool(engineParameter("LocalVol"));
        std::string localVolType = engineParameter("LocalVolType");

        static std::map<std::string, FdmSchemeDesc> fdmSchemeMap = {
            {"Hundsdorfer", FdmSchemeDesc::Hundsdorfer()},
            {"Douglas", FdmSchemeDesc::Douglas()},
            {"CraigSneyd", FdmSchemeDesc::CraigSneyd()},
            {"ModifiedCraigSneyd", FdmSchemeDesc::ModifiedCraigSneyd()},
            {"ImplicitEuler", FdmSchemeDesc::ImplicitEuler()},
            {"ExplicitEuler", FdmSchemeDesc::ExplicitEuler()},
            {"MethodOfLines", FdmSchemeDesc::MethodOfLines()},
            {"TrBDF2", FdmSchemeDesc::TrBDF2()}
        };

        auto it = fdmSchemeMap.find(scheme);
        QL_REQUIRE(it != fdmSchemeMap.end(), "unknown scheme for finite difference method: " << scheme);
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(forCcy, domCcy, localVol, localVolType);
        return boost::make_shared<FdBlackScholesVanillaEngine>(gbsp, tGrid, xGrid,
                                                               dampingSteps,
                                                               it->second,
                                                               localVol);
    }
};

//! Engine Builder for FX American Options using Barone Adesi Whaley Approximation
/*! Pricing engines are cached by currency pair

    \ingroup portfolio
 */
class FxAmericanOptionBAWApproxEngineBuilder
    : public FxAmericanOptionEngineBuilder {
public:
    FxAmericanOptionBAWApproxEngineBuilder()
        : FxAmericanOptionEngineBuilder("GarmanKohlhagen", "BaroneAdesiWhaleyApproximationEngine") {}

protected:

    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& forCcy, const Currency& domCcy) override {
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(forCcy, domCcy);
        return boost::make_shared<BaroneAdesiWhaleyApproximationEngine>(gbsp);
    }

};

} // namespace data
} // namespace ore
