/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file svimodeltraits.hpp
    \brief Per-variant pure-function helpers for SVI volatility model calibration.

    All functions are stateless (no class members accessed) and take a
    \c ModelVariant explicitly.  They are collected here to avoid large
    switch statements scattered through four class implementations.
*/

#pragma once

#include <qle/termstructures/sviparametricvolatility.hpp>

#include <ql/math/matrix.hpp>
#include <ql/math/optimization/constraint.hpp>

#include <tuple>
#include <utility>
#include <vector>

namespace QuantExt {

namespace SviModelTraits {

using ModelVariant         = SviParametricVolatility::ModelVariant;
using ParameterCalibration = ParametricVolatility::ParameterCalibration;
using MarketQuoteType      = ParametricVolatility::MarketQuoteType;

//! Returns ShiftedLognormalVolatility for every SVI model variant.
MarketQuoteType preferredOutputQuoteType(ModelVariant mv);

//! Number of calibrated parameters for \p mv.
QuantLib::Size expectedParametersSize(ModelVariant mv);

//! Default (initial) parameter vector (value, calibration status) for \p mv.
std::vector<std::pair<QuantLib::Real, ParameterCalibration>> defaultParameters(ModelVariant mv);

//! Random guess for the Levenberg-Marquardt restart strategy.
std::vector<QuantLib::Real>
getGuess(ModelVariant mv,
         const std::vector<std::pair<QuantLib::Real, ParameterCalibration>>& params,
         const std::vector<QuantLib::Real>& randomSeq,
         QuantLib::Real forward, QuantLib::Real lognormalShift);

/*!  Box / composite optimizer constraint for \p mv.
     \p params may contain fixed values in addition to calibrated ones.
     \note The Corbetta (CorbettaEtAl2019Essvi) variant is handled by the
           SsviParametricVolatilityRobust subclass override and is therefore
           not covered by this function. */
QuantLib::Constraint
calibrationConstraint(ModelVariant mv,
                      const std::vector<std::pair<QuantLib::Real, ParameterCalibration>>& params,
                      bool arbitrageFree);

//! Clamp interpolated SVI parameter matrices to their valid parameter domain.
void sanitiseParams(ModelVariant mv, std::vector<QuantLib::Matrix>& m);

//! Convert \p mv parameters to raw SVI form \f$(a, b, \rho, m, \sigma)\f$.
std::tuple<QuantLib::Real, QuantLib::Real, QuantLib::Real, QuantLib::Real, QuantLib::Real>
toRawSvi(QuantLib::Real timeToExpiry, const std::vector<QuantLib::Real>& params, ModelVariant mv);

//! Convert raw SVI \f$(a, b, \rho, m, \sigma)\f$ back to \p mv parameters.
std::vector<QuantLib::Real>
fromRawSvi(QuantLib::Real timeToExpiry, const std::vector<QuantLib::Real>& params, ModelVariant mv);

/*! Convert single-slice \p mv parameters to natural SSVI form \f$(\rho, \theta, \phi)\f$.
    Valid for Heston, PowerLaw, and Corbetta variants. */
std::tuple<QuantLib::Real, QuantLib::Real, QuantLib::Real>
toNaturalSvi(QuantLib::Real timeToExpiry, const std::vector<QuantLib::Real>& params, ModelVariant mv);

/*! Convert a flat vector of multi-slice parameters to natural SSVI vectors.
    \returns A tuple \f$(\rho[], \theta[], \phi[])\f$, one entry per time slice.
    Valid only for global model variants (HendriksMartini, Mingone). */
std::tuple<std::vector<QuantLib::Real>, std::vector<QuantLib::Real>, std::vector<QuantLib::Real>>
toNaturalSviGlobal(const std::vector<QuantLib::Real>& params, ModelVariant mv);

} // namespace SviModelTraits

} // namespace QuantExt
