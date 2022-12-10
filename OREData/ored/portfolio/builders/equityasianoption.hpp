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

 /*! \file portfolio/builders/equityasianoption.hpp
     \brief Engine builder for equity Asian options
     \ingroup builders
 */

 #pragma once

 #include <ored/portfolio/builders/asianoption.hpp>

 namespace ore {
 namespace data {

 //! Discrete Monte Carlo Engine Builder for European Asian Equity Arithmetic Average Price Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class EquityEuropeanAsianOptionMCDAAPEngineBuilder : public EuropeanAsianOptionMCDAAPEngineBuilder {
 public:
     EquityEuropeanAsianOptionMCDAAPEngineBuilder()
         : EuropeanAsianOptionMCDAAPEngineBuilder("BlackScholesMerton", {"EquityAsianOptionArithmeticPrice"},
                                                  AssetClass::EQ, expiryDate_) {}
 };

 //! Discrete Monte Carlo Engine Builder for European Asian Equity Arithmetic Average Strike Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class EquityEuropeanAsianOptionMCDAASEngineBuilder : public EuropeanAsianOptionMCDAASEngineBuilder {
 public:
     EquityEuropeanAsianOptionMCDAASEngineBuilder()
         : EuropeanAsianOptionMCDAASEngineBuilder("BlackScholesMerton", {"EquityAsianOptionArithmeticStrike"},
                                                  AssetClass::EQ, expiryDate_) {}
 };

 //! Discrete Monte Carlo Engine Builder for European Asian Equity Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class EquityEuropeanAsianOptionMCDGAPEngineBuilder : public EuropeanAsianOptionMCDGAPEngineBuilder {
 public:
     EquityEuropeanAsianOptionMCDGAPEngineBuilder()
         : EuropeanAsianOptionMCDGAPEngineBuilder("BlackScholesMerton", {"EquityAsianOptionGeometricPrice"},
                                                  AssetClass::EQ, expiryDate_) {}
 };

 //! Discrete Analytic Engine Builder for European Asian Equity Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class EquityEuropeanAsianOptionADGAPEngineBuilder : public EuropeanAsianOptionADGAPEngineBuilder {
 public:
     EquityEuropeanAsianOptionADGAPEngineBuilder()
         : EuropeanAsianOptionADGAPEngineBuilder("BlackScholesMerton", {"EquityAsianOptionGeometricPrice"},
                                                 AssetClass::EQ) {}
 };

 //! Discrete Analytic Engine Builder for European Asian Equity Geometric Average Strike Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class EquityEuropeanAsianOptionADGASEngineBuilder : public EuropeanAsianOptionADGASEngineBuilder {
 public:
     EquityEuropeanAsianOptionADGASEngineBuilder()
         : EuropeanAsianOptionADGASEngineBuilder("BlackScholesMerton", {"EquityAsianOptionGeometricStrike"},
                                                 AssetClass::EQ) {}
 };

 //! Continuous Analytic Engine Builder for European Asian Equity Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class EquityEuropeanAsianOptionACGAPEngineBuilder : public EuropeanAsianOptionACGAPEngineBuilder {
 public:
     EquityEuropeanAsianOptionACGAPEngineBuilder()
         : EuropeanAsianOptionACGAPEngineBuilder("BlackScholesMerton", {"EquityAsianOptionGeometricPrice"},
                                                 AssetClass::EQ) {}
 };

 //! Discrete Analytic TW Engine Builder for European Asian Equity Arithmetic Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class EquityEuropeanAsianOptionTWEngineBuilder : public EuropeanAsianOptionTWEngineBuilder {
 public:
     EquityEuropeanAsianOptionTWEngineBuilder()
         : EuropeanAsianOptionTWEngineBuilder("BlackScholesMerton", {"EquityAsianOptionArithmeticPrice"},
                                              AssetClass::EQ) {}
 };

 } // namespace data
 } // namespace ore
