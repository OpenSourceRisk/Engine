/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file models/lgmcg.hpp
    \brief computation graph based lgm model calculations
    \ingroup models
*/

#pragma once

#include <qle/ad/computationgraph.hpp>
#include <qle/models/irlgm1fparametrization.hpp>

#include <ql/indexes/iborindex.hpp>

namespace QuantExt {

using namespace QuantLib;

class LgmCG {
public:
    LgmCG(QuantExt::ComputationGraph& g, const boost::shared_ptr<IrLgm1fParametrization>& p,
          std::vector<std::pair<std::size_t, std::function<double(void)>>>& modelParameters)
        : g_(g), p_(p), modelParameters_(modelParameters) {}

    boost::shared_ptr<IrLgm1fParametrization> parametrization() const { return p_; }

    std::size_t numeraire(const Date& d, const std::size_t x) const;

    std::size_t discountBond(const Date& d, const Date& e, const std::size_t x) const;

    std::size_t reducedDiscountBond(const Date& d, const Date& e, const std::size_t x) const;

    /* Handles IborIndex and SwapIndex. Requires observation time t <= fixingDate */
    std::size_t fixing(const boost::shared_ptr<InterestRateIndex>& index, const Date& fixingDate, const Date& t,
                       const std::size_t x) const;

private:
    QuantExt::ComputationGraph& g_;
    boost::shared_ptr<IrLgm1fParametrization> p_;
    std::vector<std::pair<std::size_t, std::function<double(void)>>>& modelParameters_;
};

} // namespace QuantExt
