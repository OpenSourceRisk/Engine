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

/*! \file portfolio/builders/fxbarrieroption.hpp
    \brief Engine builder for FX barrier options
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/barrieroption.hpp>

namespace ore {
namespace data {

//! Engine Builder for Standard Fx Barrier Options
/*! Pricing engines are cached by currency pair/currency

    \ingroup builders
 */
class FxStandardBarrierOptionAnalyticalEngineBuilder : public StandardBarrierOptionAnalyticEngineBuilder {
public:
    FxStandardBarrierOptionAnalyticalEngineBuilder()
        : StandardBarrierOptionAnalyticEngineBuilder("GarmanKohlhagen", {"FxBarrierOption"}, AssetClass::FX) {}
};

//! Engine Builder for Standard Fx Double Barrier Options
/*! Pricing engines are cached by currency pair/currency

    \ingroup builders
 */
class FxStandardDoubleBarrierOptionAnalyticalEngineBuilder : public StandardDoubleBarrierOptionAnalyticEngineBuilder {
public:
    FxStandardDoubleBarrierOptionAnalyticalEngineBuilder()
        : StandardDoubleBarrierOptionAnalyticEngineBuilder("GarmanKohlhagen", {"FxDoubleBarrierOption"},
                                                           AssetClass::FX) {}
};

//! Engine Builder for Standard Fx Barrier Options using Vanna-Volga Barrier Engine
/*! Pricing engines are cached by currency pair/currency/expiry

    \ingroup builders
 */
class FxStandardBarrierOptionVVEngineBuilder : public StandardBarrierOptionVVEngineBuilder {
public:
    FxStandardBarrierOptionVVEngineBuilder()
        : StandardBarrierOptionVVEngineBuilder("GarmanKohlhagen", {"FxBarrierOption"}, AssetClass::FX, expiryDate_) {}
};

//! Engine Builder for Standard Fx Double Barrier Options using Vanna-Volga Double Barrier Engine
/*! Pricing engines are cached by currency pair/currency/expiry

    \ingroup builders
 */
class FxStandardDoubleBarrierOptionVVEngineBuilder : public StandardDoubleBarrierOptionVVEngineBuilder {
public:
    FxStandardDoubleBarrierOptionVVEngineBuilder()
        : StandardDoubleBarrierOptionVVEngineBuilder("GarmanKohlhagen", {"FxDoubleBarrierOption"}, AssetClass::FX,
                                                     expiryDate_) {}
};

//! Engine Builder for Partial-time Barrier Options using Analytic Partial-time Barrier Engine
/*! Pricing engines are cached by currency pair/currency

    \ingroup builders
 */
class FxPartialTimeBarrierOptionAnalyticEngineBuilder : public PartialTimeBarrierOptionAnalyticEngineBuilder {
public:
    FxPartialTimeBarrierOptionAnalyticEngineBuilder()
        : PartialTimeBarrierOptionAnalyticEngineBuilder("GarmanKohlhagen", {"FxBarrierOptionPartialTime"},
                                                        AssetClass::FX) {}
};

} // namespace data
} // namespace ore
