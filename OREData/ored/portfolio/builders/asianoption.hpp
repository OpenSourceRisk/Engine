/*
 Copyright (C) 2020 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file portfolio/builders/asianoption.hpp
    \brief Abstract engine builders for European Asian Options
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/vanillaoption.hpp>
#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ql/pricingengines/asian/analytic_cont_geom_av_price.hpp>
#include <ql/pricingengines/asian/analytic_discr_geom_av_price.hpp>
#include <ql/pricingengines/asian/analytic_discr_geom_av_strike.hpp>
#include <ql/pricingengines/asian/mc_discr_arith_av_price.hpp>
#include <ql/pricingengines/asian/mc_discr_arith_av_strike.hpp>
#include <ql/utilities/null.hpp>

namespace ore {
namespace data {

//! Abstract Engine Builder for Asian Options
/*! Pricing engines are cached by asset/currency/expiry/strike, where 
    expiry is null (Date()) if irrelevant and strike is 0 if irrelevant.
    
    \ingroup builders
 */
class AsianOptionEngineBuilder : public CachingOptionEngineBuilder<string, const string&, const Currency&,
                                                                   const AssetClass&, const Date&, const double> {
public:
    AsianOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                             const AssetClass& assetClass, const Date& expiryDate, const double strike)
        : CachingOptionEngineBuilder(model, engine, tradeTypes, assetClass), expiryDate_(expiryDate), strike_(strike) {}

    boost::shared_ptr<PricingEngine> engine(const string& assetName, const Currency& ccy, const Date& expiryDate,
                                            const double strike) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&, const Date&,
                                           const double>::engine(assetName, ccy, assetClass_, expiryDate, strike);
    }

    boost::shared_ptr<PricingEngine> engine(const Currency& ccy1, const Currency& ccy2, const Date& expiryDate,
                                            const double strike) {
        return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&, const Date&,
                                           const double>::engine(ccy1.code(), ccy2, assetClass_, expiryDate, strike);
    }

protected:
    virtual string keyImpl(const string& assetName, const Currency& ccy, const AssetClass& assetClassUnderlying,
                           const Date& expiryDate, const double strike) override {
        return assetName + "/" + ccy.code() + "/" + to_string(expiryDate) + "/" + to_string(strike);
    }

    boost::shared_ptr<GeneralizedBlackScholesProcess>
    getConstBlackScholesProcess(const string& assetName, const Currency& ccy, const AssetClass& assetClassUnderlying,
                                const Date& expiryDate, const double strike) {
        // Probably should change so that expiryDate_ and strike_ are populated and used instead as parameters for
        // function call and in blackVol()-call.

        string config = configuration(ore::data::MarketContext::pricing);

        if (assetClassUnderlying == AssetClass::EQ) {
            Handle<BlackVolTermStructure> vol = market_->equityVol(assetName, config);
            Volatility constVol = vol->blackVol(expiryDate, strike, true);
            vol = Handle<BlackVolTermStructure>(boost::make_shared<BlackConstantVol>(
                vol->referenceDate(), vol->calendar(), constVol, vol->dayCounter()));

            DLOG("Configured Black const vol = " << constVol << " for asset name = " << assetName
                                                << " with expiry date = " << expiryDate << " and strike = " << strike);
            return boost::make_shared<GeneralizedBlackScholesProcess>(
                market_->equitySpot(assetName, config), market_->equityDividendCurve(assetName, config),
                market_->equityForecastCurve(assetName, config), vol);
        } else if (assetClassUnderlying == AssetClass::FX) {
            const string& ccyPairCode = assetName + ccy.code();
            Handle<BlackVolTermStructure> vol = market_->fxVol(ccyPairCode, config);
            Volatility constVol = vol->blackVol(expiryDate, strike, true);
            vol = Handle<BlackVolTermStructure>(boost::make_shared<BlackConstantVol>(
                vol->referenceDate(), vol->calendar(), constVol, vol->dayCounter()));

            return boost::make_shared<GeneralizedBlackScholesProcess>(market_->fxSpot(ccyPairCode, config),
                                                                      market_->discountCurve(assetName, config),
                                                                      market_->discountCurve(ccy.code(), config), vol);
        } else if (assetClassUnderlying == AssetClass::COM) {
            Handle<BlackVolTermStructure> vol = market_->commodityVolatility(assetName, config);

            Date asof = vol->referenceDate();
            DayCounter volDc = vol->dayCounter();
            std::vector<Date> dates(expiryDate - asof);
            std::vector<Volatility> volatilities(expiryDate - asof);

            /*size_t index = 0;
            for (Date d = asof + 1; d <= expiryDate; ++d) {
                dates[index] = d;
                volatilities[index] = vol->blackVol(dates[index], strike, true);
                DLOG("Configured Black vol = " << volatilities[index] << " at date = " << d
                                               << " for asset name = " << assetName
                                               << " with expiry date = " << expiryDate << " and strike = " << strike);
                ++index;
            }
            vol = Handle<BlackVolTermStructure>(
                boost::make_shared<BlackVarianceCurve>(asof, dates, volatilities, volDc, false));
            */
            
            Volatility constVol = vol->blackVol(expiryDate, strike, true);
            vol = Handle<BlackVolTermStructure>(boost::make_shared<BlackConstantVol>(
                vol->referenceDate(), vol->calendar(), constVol, vol->dayCounter()));

            DLOG("Configured Black const vol = " << constVol << " for asset name = " << assetName
                                                 << " with expiry date = " << expiryDate << " and strike = " << strike);
            

            // Create the commodity convenience yield curve for the process
            Handle<QuantExt::PriceTermStructure> priceCurve = market_->commodityPriceCurve(assetName, config);
            Handle<Quote> commoditySpot(boost::make_shared<QuantExt::DerivedPriceQuote>(priceCurve));
            Handle<YieldTermStructure> discount = market_->discountCurve(ccy.code(), config);
            Handle<YieldTermStructure> yield(
                boost::make_shared<QuantExt::PriceTermStructureAdapter>(*priceCurve, *discount));
            yield->enableExtrapolation();

            return boost::make_shared<GeneralizedBlackScholesProcess>(commoditySpot, yield, discount, vol);
        } else {
            QL_FAIL("Asset class of " << (int)assetClassUnderlying << " not recognized.");
        }
    }

    Date expiryDate_;
    double strike_;
};

//! Discrete Monte Carlo Engine Builder for European Asian Arithmetic Average Price Options
/*! Pricing engines are cached by asset/currency/expiry/strike, where
    expiry is null (Date()) if irrelevant and strike is 0 if irrelevant.

    \ingroup builders
 */
class EuropeanAsianOptionMCDAAPEngineBuilder : public AsianOptionEngineBuilder {
public:
    EuropeanAsianOptionMCDAAPEngineBuilder(const string& model, const set<string>& tradeTypes,
                                           const AssetClass& assetClass, const Date& expiryDate, const double strike)
        : AsianOptionEngineBuilder(model, "MCDiscreteArithmeticAPEngine", tradeTypes, assetClass, expiryDate, strike) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const double strike) override {
        bool brownianBridge = ore::data::parseBool(engineParameter("BrownianBridge", "", false, "true"));
        bool antitheticVariate = ore::data::parseBool(engineParameter("AntitheticVariate", "", false, "true"));
        bool controlVariate = ore::data::parseBool(engineParameter("ControlVariate", "", false, "true"));
        Size requiredSamples = ore::data::parseInteger(engineParameter("RequiredSamples", "", false, "0"));
        Real requiredTolerance = ore::data::parseReal(engineParameter("RequiredTolerance", "", false, "0"));
        Size maxSamples = ore::data::parseInteger(engineParameter("MaxSamples", "", false, "0"));
        BigNatural seed = ore::data::parseInteger(engineParameter("Seed", "", false, "123456"));

        // Check if values defaulted to 0, if so replace by Null<T>().
        if (requiredSamples == 0)
            requiredSamples = Null<Size>();
        if (requiredTolerance == 0)
            requiredTolerance = Null<Real>();
        if (maxSamples == 0)
            maxSamples = Null<Size>();
        QL_REQUIRE(requiredSamples != QuantLib::Null<Size>() || requiredTolerance != QuantLib::Null<Real>(),
                   "RequiredSamples or RequiredTolerance must be set for engine MCDiscreteArithmeticAPEngine.");

        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getConstBlackScholesProcess(assetName, ccy, assetClassUnderlying, expiryDate, strike);
        return boost::make_shared<MCDiscreteArithmeticAPEngine<LowDiscrepancy>>(gbsp, brownianBridge, antitheticVariate,
                                                                  controlVariate, requiredSamples, requiredTolerance,
                                                                  maxSamples, seed);
    }
};

//! Discrete Monte Carlo Engine Builder for European Asian Arithmetic Average Strike Options
/*! Pricing engines are cached by asset/currency/expiry/strike, where
    expiry is null (Date()) if irrelevant and strike is 0 if irrelevant.

    \ingroup builders
 */
class EuropeanAsianOptionMCDAASEngineBuilder : public AsianOptionEngineBuilder {
public:
    EuropeanAsianOptionMCDAASEngineBuilder(const string& model, const set<string>& tradeTypes,
                                           const AssetClass& assetClass, const Date& expiryDate, const double strike)
        : AsianOptionEngineBuilder(model, "MCDiscreteArithmeticASEngine", tradeTypes, assetClass, expiryDate, strike) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const double strike) override {
        bool brownianBridge = ore::data::parseBool(engineParameter("BrownianBridge", "", false, "true"));
        bool antitheticVariate = ore::data::parseBool(engineParameter("AntitheticVariate", "", false, "true"));
        Size requiredSamples = ore::data::parseInteger(engineParameter("RequiredSamples", "", false, "0"));
        Real requiredTolerance = ore::data::parseReal(engineParameter("RequiredTolerance", "", false, "0"));
        Size maxSamples = ore::data::parseInteger(engineParameter("MaxSamples", "", false, "0"));
        BigNatural seed = ore::data::parseInteger(engineParameter("Seed", "", false, "123456"));

        // Check if values defaulted to 0, if so replace by Null<T>().
        if (requiredSamples == 0)
            requiredSamples = Null<Size>();
        if (requiredTolerance == 0)
            requiredTolerance = Null<Real>();
        if (maxSamples == 0)
            maxSamples = Null<Size>();
        QL_REQUIRE(requiredSamples != QuantLib::Null<Size>() || requiredTolerance != QuantLib::Null<Real>(),
                   "RequiredSamples or RequiredTolerance must be set for engine MCDiscreteArithmeticASEngine.");

        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getConstBlackScholesProcess(assetName, ccy, assetClassUnderlying, expiryDate, strike);
        return boost::make_shared<MCDiscreteArithmeticASEngine<LowDiscrepancy>>(
            gbsp, brownianBridge, antitheticVariate, requiredSamples, requiredTolerance, maxSamples, seed);
    }
};

//! Discrete Monte Carlo Engine Builder for European Asian Geometric Average Price Options
/*! Pricing engines are cached by asset/currency/expiry/strike, where
    expiry is null (Date()) if irrelevant and strike is 0 if irrelevant.

    \ingroup builders
 */
class EuropeanAsianOptionMCDGAPEngineBuilder : public AsianOptionEngineBuilder {
public:
    EuropeanAsianOptionMCDGAPEngineBuilder(const string& model, const set<string>& tradeTypes,
                                           const AssetClass& assetClass, const Date& expiryDate, const double strike)
        : AsianOptionEngineBuilder(model, "MCDiscreteGeometricAPEngine", tradeTypes, assetClass, expiryDate, strike) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const double strike) override {
        bool brownianBridge = ore::data::parseBool(engineParameter("BrownianBridge", "", false, "true"));
        bool antitheticVariate = ore::data::parseBool(engineParameter("AntitheticVariate", "", false, "true"));
        Size requiredSamples = ore::data::parseInteger(engineParameter("RequiredSamples", "", false, "0"));
        Real requiredTolerance = ore::data::parseReal(engineParameter("RequiredTolerance", "", false, "0"));
        Size maxSamples = ore::data::parseInteger(engineParameter("MaxSamples", "", false, "0"));
        BigNatural seed = ore::data::parseInteger(engineParameter("Seed", "", false, "123456"));

        // Check if values defaulted to 0, if so replace by Null<T>().
        if (requiredSamples == 0)
            requiredSamples = Null<Size>();
        if (requiredTolerance == 0)
            requiredTolerance = Null<Real>();
        if (maxSamples == 0)
            maxSamples = Null<Size>();
        QL_REQUIRE(requiredSamples != QuantLib::Null<Size>() || requiredTolerance != QuantLib::Null<Real>(),
                   "RequiredSamples or RequiredTolerance must be set for engine MCDiscreteArithmeticAPEngine.");

        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getConstBlackScholesProcess(assetName, ccy, assetClassUnderlying, expiryDate, strike);
        return boost::make_shared<MCDiscreteGeometricAPEngine<LowDiscrepancy>>(
            gbsp, brownianBridge, antitheticVariate, requiredSamples, requiredTolerance, maxSamples, seed);
    }
};

//! Discrete Analytic Engine Builder for European Asian Geometric Average Price Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EuropeanAsianOptionADGAPEngineBuilder : public AsianOptionEngineBuilder {
public:
    EuropeanAsianOptionADGAPEngineBuilder(const string& model, const set<string>& tradeTypes,
                                          const AssetClass& assetClass)
        : AsianOptionEngineBuilder(model, "AnalyticDiscreteGeometricAPEngine", tradeTypes, assetClass, Date(), 0) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const double strike) override {
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        return boost::make_shared<AnalyticDiscreteGeometricAveragePriceAsianEngine>(gbsp);
    }
};

//! Discrete Analytic Engine Builder for European Asian Geometric Average Strike Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EuropeanAsianOptionADGASEngineBuilder : public AsianOptionEngineBuilder {
public:
    EuropeanAsianOptionADGASEngineBuilder(const string& model, const set<string>& tradeTypes,
                                          const AssetClass& assetClass)
        : AsianOptionEngineBuilder(model, "AnalyticDiscreteGeometricASEngine", tradeTypes, assetClass, Date(), 0) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const double strike) override {
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        return boost::make_shared<AnalyticDiscreteGeometricAverageStrikeAsianEngine>(gbsp);
    }
};

//! Continuous Analytic Engine Builder for European Asian Geometric Average Price Options
/*! Pricing engines are cached by asset/currency

    \ingroup builders
 */
class EuropeanAsianOptionACGAPEngineBuilder : public AsianOptionEngineBuilder {
public:
    EuropeanAsianOptionACGAPEngineBuilder(const string& model, const set<string>& tradeTypes,
                                          const AssetClass& assetClass)
        : AsianOptionEngineBuilder(model, "AnalyticContinuousGeometricAPEngine", tradeTypes, assetClass, Date(), 0) {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                        const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                        const double strike) override {
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
            getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
        return boost::make_shared<AnalyticContinuousGeometricAveragePriceAsianEngine>(gbsp);
    }
};

} // namespace data
} // namespace ore
