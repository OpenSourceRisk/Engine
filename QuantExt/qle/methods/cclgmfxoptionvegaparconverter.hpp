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

/*! \file cclgmfxoptionvegaparconverter.hpp
    \brief calculates implied vol / sigma conversion matrices
*/

#pragma once

#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>

namespace QuantExt {

class CcLgmFxOptionVegaParConverter {
public:
    CcLgmFxOptionVegaParConverter() = default;
    CcLgmFxOptionVegaParConverter(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, const Size foreignCurrency,
                                  const std::vector<QuantLib::Period>& optionTerms);

    const std::vector<Real>& optionTimes() const;

    const Matrix& dpardzero() const;
    const Matrix& dzerodpar() const;

    const std::vector<Real>& baseImpliedVols() const;

private:
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    Size foreignCurrency_ = 0;
    std::vector<QuantLib::Period> optionTerms_;

    std::vector<Real> optionTimes_;
    std::vector<Real> baseImpliedVols_;
    Matrix dpardzero_;
    Matrix dzerodpar_;
};

} // namespace QuantExt
