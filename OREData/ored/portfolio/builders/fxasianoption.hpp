/*
  Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

 /*! \file portfolio/builders/fxasianoption.hpp
     \brief Engine builder for fx Asian options
     \ingroup builders
 */

 #pragma once

 #include <ored/portfolio/builders/asianoption.hpp>

 namespace ore {
 namespace data {

 //! Discrete Monte Carlo Engine Builder for European Asian Fx Arithmetic Average Price Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class FxEuropeanAsianOptionMCDAAPEngineBuilder : public EuropeanAsianOptionMCDAAPEngineBuilder {
 public:
     FxEuropeanAsianOptionMCDAAPEngineBuilder()
         : EuropeanAsianOptionMCDAAPEngineBuilder("GarmanKohlhagen", {"FxAsianOptionArithmeticPrice"}, AssetClass::FX,
                                                  expiryDate_) {}
 };

 //! Discrete Monte Carlo Engine Builder for European Asian Fx Arithmetic Average Strike Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class FxEuropeanAsianOptionMCDAASEngineBuilder : public EuropeanAsianOptionMCDAASEngineBuilder {
 public:
     FxEuropeanAsianOptionMCDAASEngineBuilder()
         : EuropeanAsianOptionMCDAASEngineBuilder("GarmanKohlhagen", {"FxAsianOptionArithmeticStrike"},
                                                  AssetClass::FX, expiryDate_) {}
 };

 //! Discrete Monte Carlo Engine Builder for European Asian Fx Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class FxEuropeanAsianOptionMCDGAPEngineBuilder : public EuropeanAsianOptionMCDGAPEngineBuilder {
 public:
     FxEuropeanAsianOptionMCDGAPEngineBuilder()
         : EuropeanAsianOptionMCDGAPEngineBuilder("GarmanKohlhagen", {"FxAsianOptionGeometricPrice"}, AssetClass::FX,
                                                  expiryDate_) {}
 };

 //! Discrete Analytic Engine Builder for European Asian Fx Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class FxEuropeanAsianOptionADGAPEngineBuilder : public EuropeanAsianOptionADGAPEngineBuilder {
 public:
     FxEuropeanAsianOptionADGAPEngineBuilder()
         : EuropeanAsianOptionADGAPEngineBuilder("GarmanKohlhagen", {"FxAsianOptionGeometricPrice"}, AssetClass::FX) {
     }
 };

 //! Discrete Analytic Engine Builder for European Asian Fx Geometric Average Strike Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class FxEuropeanAsianOptionADGASEngineBuilder : public EuropeanAsianOptionADGASEngineBuilder {
 public:
     FxEuropeanAsianOptionADGASEngineBuilder()
         : EuropeanAsianOptionADGASEngineBuilder("GarmanKohlhagen", {"FxAsianOptionGeometricStrike"},
                                                 AssetClass::FX) {}
 };

 //! Continuous Analytic Engine Builder for European Asian Fx Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class FxEuropeanAsianOptionACGAPEngineBuilder : public EuropeanAsianOptionACGAPEngineBuilder {
 public:
     FxEuropeanAsianOptionACGAPEngineBuilder()
         : EuropeanAsianOptionACGAPEngineBuilder("GarmanKohlhagen", {"FxAsianOptionGeometricPrice"}, AssetClass::FX) {
     }
 };

 //! Discrete Analytic TW Engine Builder for European Asian Fx Arithmetic Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class FxEuropeanAsianOptionTWEngineBuilder : public EuropeanAsianOptionTWEngineBuilder {
 public:
     FxEuropeanAsianOptionTWEngineBuilder()
         : EuropeanAsianOptionTWEngineBuilder("GarmanKohlhagen", {"FxAsianOptionArithmeticPrice"}, AssetClass::FX) {}
 };

 } // namespace data
 } // namespace ore
