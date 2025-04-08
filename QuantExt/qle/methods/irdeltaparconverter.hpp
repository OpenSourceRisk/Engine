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

/*! \file irdeltaparconverter.hpp
    \brief calculates ir par conversion matrices
*/

#pragma once

#include <ql/indexes/swapindex.hpp>
#include <ql/math/matrix.hpp>

namespace QuantExt {

using namespace QuantLib;

class IrDeltaParConverter {
public:
    enum class InstrumentType { Deposit, Swap };

    IrDeltaParConverter() = default;
    IrDeltaParConverter(const std::vector<QuantLib::Period>& terms, const std::vector<InstrumentType>& instrumentTypes,
                        const QuantLib::ext::shared_ptr<QuantLib::SwapIndex>& indexBase,
                        const std::function<Real(Date)>& dateToTime);

    const std::vector<Real>& times() const;

    const Matrix& dpardzero() const;
    const Matrix& dzerodpar() const;

private:
    std::vector<QuantLib::Period> terms_;
    std::vector<InstrumentType> instrumentTypes_;
    QuantExt::ext::shared_ptr<SwapIndex> indexBase_;
    std::function<Real(Date)> dateToTime_;

    std::vector<Real> times_;
    Matrix dpardzero_;
    Matrix dzerodpar_;
};

} // namespace QuantExt
