/*
  Copyright (C) 2020 Skandinaviska Enskilda Banken AB (publ)
  Copyright (C) 2022 Quaternion Risk Management Ltd
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
 #include <ql/pricingengines/asian/turnbullwakemanasianengine.hpp>
 #include <ql/utilities/null.hpp>
 #include <ored/portfolio/basketoption.hpp>
 #include <ored/portfolio/enginefactory.hpp>

 namespace ore {
 namespace data {

 //! Abstract Engine Builder for Asian Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class AsianOptionEngineBuilder
     : public CachingOptionEngineBuilder<string, const string&, const Currency&, const AssetClass&, const Date&> {
 public:
     AsianOptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                              const AssetClass& assetClass, const Date& expiryDate)
         : CachingOptionEngineBuilder(model, engine, tradeTypes, assetClass), expiryDate_(expiryDate) {}

     QuantLib::ext::shared_ptr<PricingEngine> engine(const string& assetName, const Currency& ccy, const Date& expiryDate) {
         return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&,
                                            const Date&>::engine(assetName, ccy, assetClass_, expiryDate);
     }

     QuantLib::ext::shared_ptr<PricingEngine> engine(const Currency& ccy1, const Currency& ccy2, const Date& expiryDate) {
         return CachingPricingEngineBuilder<string, const string&, const Currency&, const AssetClass&,
                                            const Date&>::engine(ccy1.code(), ccy2, assetClass_, expiryDate);
     }

     //! This is used in building the option to select between Discrete- and ContinuousAveragingAsianOption
     virtual std::string processType() { return ""; }

 protected:
     virtual string keyImpl(const string& assetName, const Currency& ccy, const AssetClass& assetClassUnderlying,
                            const Date& expiryDate) override {
         return assetName + "/" + ccy.code() + "/" + to_string(expiryDate);
     }

     Date expiryDate_;
 };

 //! Discrete Monte Carlo Engine Builder for European Asian Arithmetic Average Price Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class EuropeanAsianOptionMCDAAPEngineBuilder : public AsianOptionEngineBuilder {
 public:
     EuropeanAsianOptionMCDAAPEngineBuilder(const string& model, const set<string>& tradeTypes,
                                            const AssetClass& assetClass, const Date& expiryDate)
         : AsianOptionEngineBuilder(model, "MCDiscreteArithmeticAPEngine", tradeTypes, assetClass, expiryDate) {}

     std::string processType() override { return "Discrete"; }

 protected:
     virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                         const AssetClass& assetClassUnderlying,
                                                         const Date& expiryDate) override {
         bool brownianBridge = ore::data::parseBool(engineParameter("BrownianBridge", {}, false, "true"));
         bool antitheticVariate = ore::data::parseBool(engineParameter("AntitheticVariate", {}, false, "true"));
         bool controlVariate = ore::data::parseBool(engineParameter("ControlVariate", {}, false, "true"));
         Size requiredSamples = ore::data::parseInteger(engineParameter("RequiredSamples", {}, false, "0"));
         Real requiredTolerance = ore::data::parseReal(engineParameter("RequiredTolerance", {}, false, "0"));
         Size maxSamples = ore::data::parseInteger(engineParameter("MaxSamples", {}, false, "0"));
         BigNatural seed = ore::data::parseInteger(engineParameter("Seed", {}, false, "123456"));

         // Check if values defaulted to 0, if so replace by Null<T>().
         if (requiredSamples == 0)
             requiredSamples = Null<Size>();
         if (requiredTolerance == 0)
             requiredTolerance = Null<Real>();
         if (maxSamples == 0)
             maxSamples = Null<Size>();
         QL_REQUIRE(requiredSamples != QuantLib::Null<Size>() || requiredTolerance != QuantLib::Null<Real>(),
                    "RequiredSamples or RequiredTolerance must be set for engine MCDiscreteArithmeticAPEngine.");

         QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
             getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
         return QuantLib::ext::make_shared<MCDiscreteArithmeticAPEngine<LowDiscrepancy>>(gbsp, brownianBridge, antitheticVariate,
                                                                                 controlVariate, requiredSamples,
                                                                                 requiredTolerance, maxSamples, seed);
     }
 };

 //! Discrete Monte Carlo Engine Builder for European Asian Arithmetic Average Strike Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class EuropeanAsianOptionMCDAASEngineBuilder : public AsianOptionEngineBuilder {
 public:
     EuropeanAsianOptionMCDAASEngineBuilder(const string& model, const set<string>& tradeTypes,
                                            const AssetClass& assetClass, const Date& expiryDate)
         : AsianOptionEngineBuilder(model, "MCDiscreteArithmeticASEngine", tradeTypes, assetClass, expiryDate) {}

     std::string processType() override { return "Discrete"; }

 protected:
     virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                         const AssetClass& assetClassUnderlying,
                                                         const Date& expiryDate) override {
         bool brownianBridge = ore::data::parseBool(engineParameter("BrownianBridge", {}, false, "true"));
         bool antitheticVariate = ore::data::parseBool(engineParameter("AntitheticVariate", {}, false, "true"));
         Size requiredSamples = ore::data::parseInteger(engineParameter("RequiredSamples", {}, false, "0"));
         Real requiredTolerance = ore::data::parseReal(engineParameter("RequiredTolerance", {}, false, "0"));
         Size maxSamples = ore::data::parseInteger(engineParameter("MaxSamples", {}, false, "0"));
         BigNatural seed = ore::data::parseInteger(engineParameter("Seed", {}, false, "123456"));

         // Check if values defaulted to 0, if so replace by Null<T>().
         if (requiredSamples == 0)
             requiredSamples = Null<Size>();
         if (requiredTolerance == 0)
             requiredTolerance = Null<Real>();
         if (maxSamples == 0)
             maxSamples = Null<Size>();
         QL_REQUIRE(requiredSamples != QuantLib::Null<Size>() || requiredTolerance != QuantLib::Null<Real>(),
                    "RequiredSamples or RequiredTolerance must be set for engine MCDiscreteArithmeticASEngine.");

         QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
             getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
         return QuantLib::ext::make_shared<MCDiscreteArithmeticASEngine<LowDiscrepancy>>(
             gbsp, brownianBridge, antitheticVariate, requiredSamples, requiredTolerance, maxSamples, seed);
     }
 };

 //! Discrete Monte Carlo Engine Builder for European Asian Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class EuropeanAsianOptionMCDGAPEngineBuilder : public AsianOptionEngineBuilder {
 public:
     EuropeanAsianOptionMCDGAPEngineBuilder(const string& model, const set<string>& tradeTypes,
                                            const AssetClass& assetClass, const Date& expiryDate)
         : AsianOptionEngineBuilder(model, "MCDiscreteGeometricAPEngine", tradeTypes, assetClass, expiryDate) {}

     std::string processType() override { return "Discrete"; }

 protected:
     virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                         const AssetClass& assetClassUnderlying,
                                                         const Date& expiryDate) override {
         bool brownianBridge = ore::data::parseBool(engineParameter("BrownianBridge", {}, false, "true"));
         bool antitheticVariate = ore::data::parseBool(engineParameter("AntitheticVariate", {}, false, "true"));
         Size requiredSamples = ore::data::parseInteger(engineParameter("RequiredSamples", {}, false, "0"));
         Real requiredTolerance = ore::data::parseReal(engineParameter("RequiredTolerance", {}, false, "0"));
         Size maxSamples = ore::data::parseInteger(engineParameter("MaxSamples", {}, false, "0"));
         BigNatural seed = ore::data::parseInteger(engineParameter("Seed", {}, false, "123456"));

         // Check if values defaulted to 0, if so replace by Null<T>().
         if (requiredSamples == 0)
             requiredSamples = Null<Size>();
         if (requiredTolerance == 0)
             requiredTolerance = Null<Real>();
         if (maxSamples == 0)
             maxSamples = Null<Size>();
         QL_REQUIRE(requiredSamples != QuantLib::Null<Size>() || requiredTolerance != QuantLib::Null<Real>(),
                    "RequiredSamples or RequiredTolerance must be set for engine MCDiscreteGeometricAPEngine.");

         QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
             getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
         return QuantLib::ext::make_shared<MCDiscreteGeometricAPEngine<LowDiscrepancy>>(
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
         : AsianOptionEngineBuilder(model, "AnalyticDiscreteGeometricAPEngine", tradeTypes, assetClass, Date()) {}

     std::string processType() override { return "Discrete"; }

 protected:
     virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                         const AssetClass& assetClassUnderlying,
                                                         const Date& expiryDate) override {
         QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
             getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
         return QuantLib::ext::make_shared<AnalyticDiscreteGeometricAveragePriceAsianEngine>(gbsp);
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
         : AsianOptionEngineBuilder(model, "AnalyticDiscreteGeometricASEngine", tradeTypes, assetClass, Date()) {}

     std::string processType() override { return "Discrete"; }

 protected:
     virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                         const AssetClass& assetClassUnderlying,
                                                         const Date& expiryDate) override {
         QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
             getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
         return QuantLib::ext::make_shared<AnalyticDiscreteGeometricAverageStrikeAsianEngine>(gbsp);
     }
 };

 //! Continuous Analytic Engine Builder for European Asian Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency
     Note that this engine disregards fixing dates, i.e. it utilizes continuous averaging and is mainly for testing.
     \ingroup builders
  */
 class EuropeanAsianOptionACGAPEngineBuilder : public AsianOptionEngineBuilder {
 public:
     EuropeanAsianOptionACGAPEngineBuilder(const string& model, const set<string>& tradeTypes,
                                           const AssetClass& assetClass)
         : AsianOptionEngineBuilder(model, "AnalyticContinuousGeometricAPEngine", tradeTypes, assetClass, Date()) {}

     std::string processType() override { return "Continuous"; }

 protected:
     virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                         const AssetClass& assetClassUnderlying,
                                                         const Date& expiryDate) override {
         QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
             getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
         return QuantLib::ext::make_shared<AnalyticContinuousGeometricAveragePriceAsianEngine>(gbsp);
     }
 };

 //! Discrete Analytic TW Engine Builder for European Asian Arithmetic Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class EuropeanAsianOptionTWEngineBuilder : public AsianOptionEngineBuilder {
 public:
     EuropeanAsianOptionTWEngineBuilder(const string& model, const set<string>& tradeTypes,
                                        const AssetClass& assetClass)
         : AsianOptionEngineBuilder(model, "TurnbullWakemanAsianEngine", tradeTypes, assetClass, Date()) {}

     std::string processType() override { return "Discrete"; }

 protected:
     virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& assetName, const Currency& ccy,
                                                         const AssetClass& assetClassUnderlying,
                                                         const Date& expiryDate) override {
         QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
             getBlackScholesProcess(assetName, ccy, assetClassUnderlying);
         return QuantLib::ext::make_shared<TurnbullWakemanAsianEngine>(gbsp);
     }
 };

class AsianOptionScriptedEngineBuilder : public DelegatingEngineBuilder {
public:
    AsianOptionScriptedEngineBuilder()
        : DelegatingEngineBuilder("ScriptedTrade", "ScriptedTrade",
                                  {"EquityAsianOptionArithmeticPrice", "EquityAsianOptionArithmeticStrike",
                                   "EquityAsianOptionGeometricPrice", "EquityAsianOptionGeometricStrike",
                                   "FxAsianOptionArithmeticPrice", "FxAsianOptionArithmeticStrike",
                                   "FxAsianOptionGeometricPrice", "FxAsianOptionGeometricStrike",
                                   "CommodityAsianOptionArithmeticPrice", "CommodityAsianOptionArithmeticStrike",
                                   "CommodityAsianOptionGeometricPrice", "CommodityAsianOptionGeometricStrike"}) {}
    QuantLib::ext::shared_ptr<ore::data::Trade> build(const Trade* trade,
                                              const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) override;
    std::string effectiveTradeType() const override { return "ScriptedTrade"; }
};

 } // namespace data
 } // namespace ore
