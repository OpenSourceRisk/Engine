/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file spreadedoptionletvolatility2.hpp
    \brief Optionlet volatility with overlayed bilinearly interpolated spread surface
    \ingroup termstructures
*/

#pragma once
#include <qle/termstructures/dynamicstype.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {

class SpreadedOptionletVolatility2 : public QuantLib::OptionletVolatilityStructure, public QuantLib::LazyObject {
public:
    SpreadedOptionletVolatility2(
        const QuantLib::Handle<OptionletVolatilityStructure>& baseVol,
        const std::vector<QuantLib::Date>& optionDates,
        const std::vector<QuantLib::Real>& strikes,
        const std::vector<std::vector<QuantLib::Handle<QuantLib::Quote>>>& volSpreads,
        ReactionToTimeDecay decayMode,
        Stickyness stickyness = StickyStrike,
        QuantLib::ext::shared_ptr<QuantLib::IborIndex> index = nullptr,
        QuantLib::ext::shared_ptr<QuantLib::IborIndex> initIndex = nullptr);

    QuantLib::BusinessDayConvention businessDayConvention() const override;
    QuantLib::Rate minStrike() const override;
    QuantLib::Rate maxStrike() const override;
    QuantLib::Date maxDate() const override;
    QuantLib::VolatilityType volatilityType() const override;
    QuantLib::Real displacement() const override;
    void update() override;
    void deepUpdate() override;

protected:
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const override;
    QuantLib::Volatility volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const override;
    void performCalculations() const override;

    const QuantLib::Handle<QuantLib::OptionletVolatilityStructure>& baseVol() const { return baseVol_; }
    const std::vector<QuantLib::Real>& strikes() const { return strikes_; }
    const std::vector<QuantLib::Real>& optionTimes() const { return optionTimes_; }
    const QuantLib::Matrix& volSpreadValues() const { return volSpreadValues_; }

    QuantLib::Handle<QuantLib::OptionletVolatilityStructure> baseVol_;
    std::vector<QuantLib::Date> optionDates_;
    std::vector<QuantLib::Real> strikes_;
    std::vector<std::vector<QuantLib::Handle<QuantLib::Quote>>> volSpreads_;
    ReactionToTimeDecay decayMode_;
    Stickyness stickyness_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> index_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> initIndex_;

    mutable std::vector<QuantLib::Real> optionTimes_;
    mutable QuantLib::Matrix volSpreadValues_;
    mutable QuantLib::Interpolation2D volSpreadInterpolation_;
    mutable QuantLib::Date originalRefDate_, actualRefDate_;
    mutable QuantLib::Real t0_;

private:
    QuantLib::Date dateFromTime(QuantLib::Time optionTime) const;
};

class AtmAdjustedSpreadedOptionletVolatility2 : public SpreadedOptionletVolatility2 {
public:
    AtmAdjustedSpreadedOptionletVolatility2(
        const QuantLib::Handle<QuantLib::OptionletVolatilityStructure>& baseVol,
        const std::vector<QuantLib::Date>& optionDates,
        const std::vector<QuantLib::Real>& strikes,
        const std::vector<std::vector<QuantLib::Handle<QuantLib::Quote>>>& volSpreads,
        const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& baseIndex,
        const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& targetIndex,
        const QuantLib::Period& baseRateComputationPeriod = 0 * QuantLib::Days,
        const QuantLib::Period& targetRateComputationPeriod = 0 * QuantLib::Days,
        QuantLib::Real scalingFactor = 1.0,
        ReactionToTimeDecay decayMode = ForwardForwardVariance);
    void update() override;
    void deepUpdate() override;

protected:
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(const QuantLib::Date& fixingDate) const override;
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const override;
    QuantLib::Volatility volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const override;
    void performCalculations() const override;

private:
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> baseIndex_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> targetIndex_;
    QuantLib::Period baseRateComputationPeriod_;
    QuantLib::Period targetRateComputationPeriod_;
    QuantLib::Real scalingFactor_;
    mutable std::map<QuantLib::Time, QuantLib::ext::shared_ptr<QuantLib::SmileSection>> smileSectionCache_;
};

} // namespace QuantExt
