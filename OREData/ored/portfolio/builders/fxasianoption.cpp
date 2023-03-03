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

 #include <ored/portfolio/builders/fxasianoption.hpp>

 namespace ore {
 namespace data {

 ORE_REGISTER_ENGINE_BUILDER_IMPL(FxEuropeanAsianOptionMCDAAPEngineBuilder)
 ORE_REGISTER_ENGINE_BUILDER_IMPL(FxEuropeanAsianOptionMCDAASEngineBuilder)
 ORE_REGISTER_ENGINE_BUILDER_IMPL(FxEuropeanAsianOptionMCDGAPEngineBuilder)
 ORE_REGISTER_ENGINE_BUILDER_IMPL(FxEuropeanAsianOptionADGAPEngineBuilder)
 ORE_REGISTER_ENGINE_BUILDER_IMPL(FxEuropeanAsianOptionADGASEngineBuilder)
 ORE_REGISTER_ENGINE_BUILDER_IMPL(FxEuropeanAsianOptionACGAPEngineBuilder)
 ORE_REGISTER_ENGINE_BUILDER_IMPL(FxEuropeanAsianOptionTWEngineBuilder)

 } // namespace data
 } // namespace ore
