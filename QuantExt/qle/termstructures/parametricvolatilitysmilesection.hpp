/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file paramtricvolatilitysmilesection.hpp
    \brief smile section based on parametric volatility
    \ingroup termstructures
*/

#ifndef quantext_parametric_volatility_smile_section_h
#define quantext_parametric_volatility_smile_section_h

#include <qle/termstructures/parametricvolatility.hpp>

#include <ql/termstructures/volatility/smilesection.hpp>

namespace QuantExt {
using namespace QuantLib;

class ParametricVolatilitySmileSection : public QuantLib::SmileSection {
public:
    ParametricVolatilitySmileSection(const Real optionTime, const Real swapLength, const Real atmLevel,
                                     const boost::shared_ptr<ParametricVolatility> parametricVolatility,
                                     const ParametricVolatility::MarketQuoteType outputMarketQuoteType);
    Real minStrike() const override { return -QL_MAX_REAL; }
    Real maxStrike() const override { return QL_MAX_REAL; }
    Real atmLevel() const override;

private:
    Volatility volatilityImpl(Rate strike) const override;
    Real optionTime_, swapLength_, atmLevel_;
    boost::shared_ptr<ParametricVolatility> parametricVolatility_;
    ParametricVolatility::MarketQuoteType outputMarketQuoteType_;
    mutable std::map<Real, Real> cache_;
};

} // namespace QuantExt

#endif
