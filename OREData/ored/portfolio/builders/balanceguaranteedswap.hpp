/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/flexiswap.hpp
    \brief
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/flexiswap.hpp>

namespace ore {
namespace data {

//! Balance Guaranteed Swap Discounting Engine Builder
class BalanceGuaranteedSwapDiscountingEngineBuilder : public FlexiSwapBGSDiscountingEngineBuilderBase {
public:
    BalanceGuaranteedSwapDiscountingEngineBuilder()
        : FlexiSwapBGSDiscountingEngineBuilderBase("BalanceGuaranteedSwap") {}
};

//! Balance Guaranteed Swap Flexi Swap LGM Grid Engine Builder
class BalanceGuaranteedSwapFlexiSwapLGMGridEngineBuilder : public FlexiSwapBGSLGMGridEngineBuilderBase {
public:
    BalanceGuaranteedSwapFlexiSwapLGMGridEngineBuilder()
        : FlexiSwapBGSLGMGridEngineBuilderBase("BalanceGuaranteedSwap", "LGM-FlexiSwap") {}

protected:
    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& id, const std::string& id2,
                                                                  const std::string& ccy,
                                                                  const std::vector<QuantLib::Date>& dates,
                                                                  const QuantLib::Date& maturity,
                                                                  const std::vector<QuantLib::Real>& strikes) override;
};

} // namespace data
} // namespace ore
