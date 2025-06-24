/*
 Copyright (C) 2017 Quaternion Risk Management Ltd.
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

/*!
  \file qle/math/stoplightbounds.hpp
  \brief compute stop light bounds for overlapping and correlated PL
 */

#pragma once

#include <ql/math/matrix.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/types.hpp>

#include <vector>

namespace QuantExt {
using QuantLib::Matrix;
using QuantLib::Null;
using QuantLib::Real;
using QuantLib::SalvagingAlgorithm;
using QuantLib::Size;

/*! Computes the maximum number of exceptions K such that the probability of having less or equal to
  K exceptions is less than s for a given vector of stop light levels s, i.e.

  P(X <= K) < s and P(X <= K+1) >= s

  In the basel approach we have

  s = 0.95   :  K = maximum number of exceptions such that the model is considered to be "green"
  s = 0.9999 :  K = maxium number of exception such that the model is considered to be "amber"

  See https://www.bis.org/publ/bcbs22.pdf:

  "The yellow zone begins at the point such that the probability of obtaining that number or fewer exceptions
  equals or exceeds 95%."

  and similar for the red zone.

  An overlapping PL over a given period is considered, possibly also several portfolios with correlated PL.
  If the parameter exceptions m is not null, cumProb is set to the probability of having less of equal to m
  exceptions (this is not affecting the return value). */
std::vector<Size> stopLightBounds(const std::vector<Real>& stopLightP, const Size observations,
                                  const Size numberOfDays = 10, const Real p = 0.99, const Size numberOfPortfolios = 1,
                                  const Matrix& correlation = Matrix(1, 1, 1.0), const Size samples = 1500000,
                                  const Size seed = 42,
                                  const SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::Spectral,
                                  const Size exceptions = Null<Size>(), Real* cumProb = nullptr);

/* Uses pretabulated values, throws if no suitable values are tabulated (single portfolio case only) */
std::vector<Size> stopLightBoundsTabulated(const std::vector<Real>& stopLightP, const Size observations,
                                           const Size numberOfDays = 10, const Real p = 0.99, const std::vector<int>& obsNb = std::vector<int>{},
                                           const std::vector<int>& amberLimit = std::vector<int>{}, const std::vector<int>& redLimit = std::vector<int>{});

/* Same as above, but for non-overlapping and independent observations */
std::vector<Size> stopLightBounds(const std::vector<Real>& stopLightP, const Size observations, const Real p,
                                  const Size exceptions = Null<Size>(), Real* cumProb = nullptr);

/* Generate table of stop light bounds (observations_k, (b_1,k, b_2,k, ..., b_n,k) for a given vector of threshold
   probabilities {stopLightP[1], ..., stopLightP[n]} and a given vector of observations */
std::vector<std::pair<Size, std::vector<Size>>>
generateStopLightBoundTable(const std::vector<Size>& observations, const std::vector<Real>& stopLightP,
                            const Size samples, const Size seed, const Size numberOfDays = 10, const Real p = 0.99);

/* Decorrelate a given n-day overlapping pnl. If (X_k) is a sequence of n-day overlapping PLs then the return value
   is calculated as L^{-1} X_k where L is Cholesky factor

   C = L L^T

   with C = correlation matrix of n-day overlapping pnl. In our model, the n-day overlapping pnl is the sum of n
   i.i.d. normally distributed daily pnls and therefore the entries of C are given by

     c_{i,j} = 1 - \frac{\max(\min(i-j,l),0)}{l} */
std::vector<double> decorrelateOverlappingPnls(const std::vector<double>& pnl, const Size numberOfDays);

} // namespace QuantExt
