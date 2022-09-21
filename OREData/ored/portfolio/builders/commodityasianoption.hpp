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

 /*! \file portfolio/builders/commodityasianoption.hpp
     \brief Engine builder for commodity Asian options
     \ingroup builders
 */

 #pragma once

 #include <ored/portfolio/builders/asianoption.hpp>

 namespace ore {
 namespace data {

 //! Discrete Monte Carlo Engine Builder for European Asian Commodity Arithmetic Average Price Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class CommodityEuropeanAsianOptionMCDAAPEngineBuilder : public EuropeanAsianOptionMCDAAPEngineBuilder {
 public:
     CommodityEuropeanAsianOptionMCDAAPEngineBuilder()
         : EuropeanAsianOptionMCDAAPEngineBuilder("BlackScholesMerton", {"CommodityAsianOptionArithmeticPrice"},
                                                  AssetClass::COM, expiryDate_) {}
 };

 //! Discrete Monte Carlo Engine Builder for European Asian Commodity Arithmetic Average Strike Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class CommodityEuropeanAsianOptionMCDAASEngineBuilder : public EuropeanAsianOptionMCDAASEngineBuilder {
 public:
     CommodityEuropeanAsianOptionMCDAASEngineBuilder()
         : EuropeanAsianOptionMCDAASEngineBuilder("BlackScholesMerton", {"CommodityAsianOptionArithmeticStrike"},
                                                  AssetClass::COM, expiryDate_) {}
 };

 //! Discrete Monte Carlo Engine Builder for European Asian Commodity Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency/expiry, where
     expiry is null (Date()) if irrelevant.
     \ingroup builders
  */
 class CommodityEuropeanAsianOptionMCDGAPEngineBuilder : public EuropeanAsianOptionMCDGAPEngineBuilder {
 public:
     CommodityEuropeanAsianOptionMCDGAPEngineBuilder()
         : EuropeanAsianOptionMCDGAPEngineBuilder("BlackScholesMerton", {"CommodityAsianOptionGeometricPrice"},
                                                  AssetClass::COM, expiryDate_) {}
 };

 //! Discrete Analytic Engine Builder for European Asian Commodity Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class CommodityEuropeanAsianOptionADGAPEngineBuilder : public EuropeanAsianOptionADGAPEngineBuilder {
 public:
     CommodityEuropeanAsianOptionADGAPEngineBuilder()
         : EuropeanAsianOptionADGAPEngineBuilder("BlackScholesMerton", {"CommodityAsianOptionGeometricPrice"},
                                                 AssetClass::COM) {}
 };

 //! Discrete Analytic Engine Builder for European Asian Commodity Geometric Average Strike Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class CommodityEuropeanAsianOptionADGASEngineBuilder : public EuropeanAsianOptionADGASEngineBuilder {
 public:
     CommodityEuropeanAsianOptionADGASEngineBuilder()
         : EuropeanAsianOptionADGASEngineBuilder("BlackScholesMerton", {"CommodityAsianOptionGeometricStrike"},
                                                 AssetClass::COM) {}
 };

 //! Continuous Analytic Engine Builder for European Asian Commodity Geometric Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class CommodityEuropeanAsianOptionACGAPEngineBuilder : public EuropeanAsianOptionACGAPEngineBuilder {
 public:
     CommodityEuropeanAsianOptionACGAPEngineBuilder()
         : EuropeanAsianOptionACGAPEngineBuilder("BlackScholesMerton", {"CommodityAsianOptionGeometricPrice"},
                                                 AssetClass::COM) {}
 };

 //! Discrete Analytic TW Engine Builder for European Asian Commodity Arithmetic Average Price Options
 /*! Pricing engines are cached by asset/currency
     \ingroup builders
  */
 class CommodityEuropeanAsianOptionTWEngineBuilder : public EuropeanAsianOptionTWEngineBuilder {
 public:
     CommodityEuropeanAsianOptionTWEngineBuilder()
         : EuropeanAsianOptionTWEngineBuilder("BlackScholesMerton", {"CommodityAsianOptionArithmeticPrice"},
                                              AssetClass::COM) {}
 };

 } // namespace data
 } // namespace ore
