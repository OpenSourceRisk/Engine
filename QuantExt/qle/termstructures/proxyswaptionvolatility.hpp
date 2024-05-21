/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/proxyswaptionvolatility.hpp
    \brief moneyness-adjusted swaption vol for normal vols
    \ingroup termstructures
*/

#pragma once

#include <ql/indexes/swapindex.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

namespace QuantExt {

class ProxySwaptionVolatility : public QuantLib::SwaptionVolatilityStructure {
public:
    ProxySwaptionVolatility(const QuantLib::Handle<SwaptionVolatilityStructure>& baseVol,
                            QuantLib::ext::shared_ptr<QuantLib::SwapIndex> baseSwapIndexBase,
                            QuantLib::ext::shared_ptr<QuantLib::SwapIndex> baseShortSwapIndexBase,
                            QuantLib::ext::shared_ptr<QuantLib::SwapIndex> targetSwapIndexBase,
                            QuantLib::ext::shared_ptr<QuantLib::SwapIndex> targetShortSwapIndexBase);

    QuantLib::Rate minStrike() const override { return baseVol_->minStrike(); }
    QuantLib::Rate maxStrike() const override { return baseVol_->maxStrike(); }
    QuantLib::Date maxDate() const override { return baseVol_->maxDate(); }
    const QuantLib::Date& referenceDate() const override { return baseVol_->referenceDate(); }
    QuantLib::VolatilityType volatilityType() const override { return baseVol_->volatilityType(); }
    QuantLib::Calendar calendar() const override { return baseVol_->calendar(); }

    const QuantLib::Period& maxSwapTenor() const override { return baseVol_->maxSwapTenor(); }

private:
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(const QuantLib::Date& optionDate,
                                                               const QuantLib::Period& swapTenor) const override;
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime,
                                                               QuantLib::Time swapLength) const override;
    QuantLib::Volatility volatilityImpl(QuantLib::Time optionTime, QuantLib::Time swapLength,
                                        QuantLib::Rate strike) const override;

    QuantLib::Handle<QuantLib::SwaptionVolatilityStructure> baseVol_;
    QuantLib::ext::shared_ptr<QuantLib::SwapIndex> baseSwapIndexBase_;
    QuantLib::ext::shared_ptr<QuantLib::SwapIndex> baseShortSwapIndexBase_;
    QuantLib::ext::shared_ptr<QuantLib::SwapIndex> targetSwapIndexBase_;
    QuantLib::ext::shared_ptr<QuantLib::SwapIndex> targetShortSwapIndexBase_;
};

} // namespace QuantExt
