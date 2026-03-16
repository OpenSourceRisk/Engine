/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/pairwisevarianceswapengine.hpp
    \brief pairwise variance swap engine
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

#include <qle/instruments/pairwisevarianceswap.hpp>

namespace QuantExt {
using namespace QuantLib;

struct Variances {
    Real accruedVariance1;
    Real accruedVariance2;
    Real accruedBasketVariance;
    Real futureVariance1;
    Real futureVariance2;
    Real futureBasketVariance;
    Real totalVariance1;
    Real totalVariance2;
    Real totalBasketVariance;

    Variances() {
        accruedVariance1 = 0.0;
        accruedVariance2 = 0.0;
        accruedBasketVariance = 0.0;
        futureVariance1 = 0.0;
        futureVariance2 = 0.0;
        futureBasketVariance = 0.0;
        totalVariance1 = 0.0;
        totalVariance2 = 0.0;
        totalBasketVariance = 0.0;
    }
};

class PairwiseVarianceSwapEngine : public QuantExt::PairwiseVarianceSwap::engine {
public:
    PairwiseVarianceSwapEngine(const QuantLib::ext::shared_ptr<QuantLib::Index>& index1,
                               const QuantLib::ext::shared_ptr<QuantLib::Index>& index2,
                               const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process1,
                               const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process2,
                               const Handle<YieldTermStructure>& discountingTS, Handle<Quote> correlation);

    void calculate() const override;

protected:
    Variances calculateVariances(const Schedule& valuationSchedule, const Schedule& laggedValuationSchedule,
                                 const Date& evalDate) const;

    QuantLib::ext::shared_ptr<Index> index1_;
    QuantLib::ext::shared_ptr<Index> index2_;
    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> process1_;
    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> process2_;
    Handle<YieldTermStructure> discountingTS_;
    Handle<Quote> correlation_;
};

} // namespace QuantExt
