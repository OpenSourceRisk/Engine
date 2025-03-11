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

/*! \file orea/engine/simpleim.hpp
    \brief simple im model for dynamic im calculations
*/

#pragma once

#include <qle/math/randomvariable.hpp>

#include <ql/math/matrix.hpp>

namespace ore {
namespace analytics {

class SimpleDynamicSimm {
public:
    SimpleDynamicSimm(const std::size_t n, const std::vector<std::string>& currencies,
                      const std::vector<QuantLib::Period> irVegaTerms, const std::vector<QuantLib::Period> fxVegaTerms,
                      const QuantLib::ext::shared_ptr<SimmConfiguration>& simmConfiguration);

    QuantExt::RandomVariable value(const std::vector<QuantExt::RandomVariable>& irDelta,
                                   const std::vector<std::vector<QuantExt::RandomVariable>>& irVega,
                                   const std::vector<QuantExt::RandomVariable>& fxDelta,
                                   const std::vector<std::vector<QuantExt::RandomVariable>>& fxVega);

private:
    // input params

    std::size_t n_;
    std::vector<std::string> currencies_;
    std::vector<QuantLib::Period> irVegaTerms_;
    std::vector<QuantLib::Period> fxVegaTerms_;
    QuantLib::ext::shared_ptr<SimmConfiguration> simmConfiguration_;

    // extracted from the given simm configuration, for easy and efficient use in value()

    double corrIrFx_;
    double irDeltaRw_;
    double irVegaRw_;
    double irGamma_;
    double irHvr_;
    QuantLib::Matrix irVegaCorrelations_;
    QuantLib::Array irCurvatureWeights_;
    
};

} // namespace analytics
} // namespace ore
