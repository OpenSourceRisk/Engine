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

/*! \file qle/termstructures/proxyoptionletvolatility.hpp
    \brief moneyness-adjusted optionlet vol for normal vols
    \ingroup termstructures
*/

#pragma once

#include <ql/indexes/iborindex.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {

class ProxyOptionletVolatility : public QuantLib::OptionletVolatilityStructure {
public:
    ProxyOptionletVolatility(const QuantLib::Handle<OptionletVolatilityStructure>& baseVol,
                             const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& baseIndex,
                             const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& targetIndex,
                             const QuantLib::Period& baseRateComputationPeriod = 0 * QuantLib::Days,
                             const QuantLib::Period& targetRateComputationPeriod = 0 * QuantLib::Days,
                             double scalingFactor = 1.0);

    QuantLib::Rate minStrike() const override { return baseVol_->minStrike(); }
    QuantLib::Rate maxStrike() const override { return baseVol_->maxStrike(); }
    QuantLib::Date maxDate() const override { return baseVol_->maxDate(); }
    const QuantLib::Date& referenceDate() const override { return baseVol_->referenceDate(); }
    QuantLib::VolatilityType volatilityType() const override { return baseVol_->volatilityType(); }
    QuantLib::Real displacement() const override { return baseVol_->displacement(); }
    QuantLib::Calendar calendar() const override { return baseVol_->calendar(); }

    const QuantLib::Handle<QuantLib::OptionletVolatilityStructure>& baseVol() const { return baseVol_; }
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& baseIndex() const { return baseIndex_; }
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& targetIndex() const { return targetIndex_; }
    const QuantLib::Period& baseRateComputationPeriod() const { return baseRateComputationPeriod_; }
    const QuantLib::Period& targetRateComputationPeriod() const { return targetRateComputationPeriod_; }
    double scalingFactor() const { return scalingFactor_; }

    static QuantLib::Real getAtmLevel(const QuantLib::Date& fixingDate,
                                      const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index,
                                      const QuantLib::Period& rateComputationPeriod);

     QuantLib::ext::shared_ptr<QuantLib::IborIndex> getBaseIndex() const { return baseIndex_; }
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> getTargetIndex() const { return targetIndex_; }

private:
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(const QuantLib::Date& optionDate) const override;
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const override;
    QuantLib::Volatility volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const override;

    QuantLib::Handle<QuantLib::OptionletVolatilityStructure> baseVol_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> baseIndex_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> targetIndex_;
    QuantLib::Period baseRateComputationPeriod_;
    QuantLib::Period targetRateComputationPeriod_;
    double scalingFactor_;
};

} // namespace QuantExt
