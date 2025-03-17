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

/*! \file lgmswaptionvegaparconverter.hpp
    \brief calculates implied vol / alpha conversion matrices
*/

#pragma once

#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ql/indexes/swapindex.hpp>

namespace QuantExt {

class LgmSwaptionVegaParConverter {
public:
    LgmSwaptionVegaParConverter() = default;
    LgmSwaptionVegaParConverter(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                const std::vector<QuantLib::Period>& optionTerms,
                                const std::vector<QuantLib::Period>& underlyingTerms,
                                const QuantExt::ext::shared_ptr<QuantLib::SwapIndex>& index);

    const std::vector<Real>& optionTimes() const;

    const Matrix& dpardzero() const;
    const Matrix& dzerodpar() const;

    const std::vector<Real>& baseImpliedVols() const;

private:
    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> model_;
    std::vector<QuantLib::Period> optionTerms_, underlyingTerms_;
    QuantExt::ext::shared_ptr<SwapIndex> index_;

    std::vector<Real> optionTimes_;
    std::vector<Real> baseImpliedVols_;
    Matrix dpardzero_;
    Matrix dzerodpar_;
};

} // namespace QuantExt
