/*
  Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/varianceswapgeneralreplicationengine.hpp
    \brief variance swap engine
    \ingroup engines
*/

#pragma once

#include <ql/quote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <ql/exercise.hpp>
#include <ql/index.hpp>
#include <ql/instruments/europeanoption.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>

#include <qle/instruments/varianceswap.hpp>

namespace QuantExt {
using namespace QuantLib;

/* Reference:
   - Variance Swaps, European Equity Derivatives Research, JPMorgan, 4.5
   - https://en.wikipedia.org/wiki/Variance_swap */
class GeneralisedReplicatingVarianceSwapEngine : public QuantExt::VarianceSwap2::engine {
public:
    /*
    struct VarSwapSettings {
        enum class Scheme { GaussLobatto, Segment };
        enum class Bounds { Fixed, PriceThreshold };
        Scheme scheme = Scheme::GaussLobatto;
        Bounds bounds = Bounds::PriceThreshold;
        Real accuracy = 1E-5;
        Size maxIterations = 1000;
        Size steps = 100;
        Real priceThreshold = 1E-10;
        Size maxPriceThresholdSteps = 100;
        Real priceThresholdStep = 0.1;
        Real fixedMinStdDevs = -5.0;
        Real fixedMaxStdDevs = 5.0;
    };
    */
    // Renamed from Settings to avoid conflicts during the Python wrapping
    class VarSwapSettings {
    public:
        enum class Scheme { GaussLobatto, Segment };
        enum class Bounds { Fixed, PriceThreshold };
        VarSwapSettings() {}
        Scheme scheme = Scheme::GaussLobatto;
        Bounds bounds = Bounds::PriceThreshold;
        Real accuracy = 1E-5;
        Size maxIterations = 1000;
        Size steps = 100;
        Real priceThreshold = 1E-10;
        Size maxPriceThresholdSteps = 100;
        Real priceThresholdStep = 0.1;
        Real fixedMinStdDevs = -5.0;
        Real fixedMaxStdDevs = 5.0;
    };

    GeneralisedReplicatingVarianceSwapEngine(const QuantLib::ext::shared_ptr<QuantLib::Index>& index,
                                             const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                             const Handle<YieldTermStructure>& discountingTS,
                                             const VarSwapSettings settings = VarSwapSettings(),
                                             const bool staticTodaysSpot = true);

    void calculate() const override;

protected:
    Real calculateAccruedVariance(const Calendar& jointCal) const;
    Real calculateFutureVariance(const Date& maturity) const;

    QuantLib::ext::shared_ptr<Index> index_;
    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    Handle<YieldTermStructure> discountingTS_;
    VarSwapSettings settings_;
    bool staticTodaysSpot_;

    mutable Real cachedTodaysSpot_ = Null<Real>();
};

} // namespace QuantExt
