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

/*! \file spreadedswaptionvolatility.hpp
    \brief swaption cube defined via atm vol spreads over another cube
    \ingroup termstructures
*/

#pragma once

#include <ql/indexes/swapindex.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvoldiscrete.hpp>

#include <ql/shared_ptr.hpp>

namespace QuantExt {
using namespace QuantLib;

class SpreadedSwaptionVolatility : public SwaptionVolatilityDiscrete {
public:
    /* - The base vol is required to provide smile sections with atm levels, if there is more than one strike spread
         given. Alternatively, baseSwapIndexBase and baseShortSwapIndexBase can be provided to compute these ATM levels.
       - If stickyAbsMoney is true, the simulatedSwapIndexBase and simulatedShortSwapIndexBase must be provided and
         represent an ATM level reacting to changes in rate levels. The ATM levels implied by base vol,
         baseSwapIndexBase, baseShortSwapIndexBase must not react to changes in the rate levels on the other hand.
    */
    SpreadedSwaptionVolatility(const Handle<SwaptionVolatilityStructure>& base, const std::vector<Period>& optionTenors,
                               const std::vector<Period>& swapTenors, const std::vector<Real>& strikeSpreads,
                               const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                               const QuantLib::ext::shared_ptr<SwapIndex>& baseSwapIndexBase = nullptr,
                               const QuantLib::ext::shared_ptr<SwapIndex>& baseShortSwapIndexBase = nullptr,
                               const QuantLib::ext::shared_ptr<SwapIndex>& simulatedSwapIndexBase = nullptr,
                               const QuantLib::ext::shared_ptr<SwapIndex>& simulatedShortSwapIndexBase = nullptr,
                               const bool stickyAbsMoney = false);

    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const override;
    Date maxDate() const override;
    Time maxTime() const override;
    const Date& referenceDate() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;
    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const override;
    Rate maxStrike() const override;
    //@}
    //! \name SwaptionVolatilityStructure interface
    //@{
    const Period& maxSwapTenor() const override;
    VolatilityType volatilityType() const override;
    //@}
    //! \name Observer interface
    //@{
    void deepUpdate() override;
    //@}
    const Handle<SwaptionVolatilityStructure>& baseVol();

private:
    QuantLib::ext::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const override;
    Volatility volatilityImpl(Time optionTime, Time swapLength, Rate strike) const override;
    Real shiftImpl(const Date& optionDate, const Period& swapTenor) const override;
    Real shiftImpl(Time optionTime, Time swapLength) const override;
    void performCalculations() const override;
    Real getAtmLevel(const Real optionTime, const Real swapLength, const QuantLib::ext::shared_ptr<SwapIndex> swapIndexBase,
                     const QuantLib::ext::shared_ptr<SwapIndex> shortSwapIndexBase) const;

    Handle<SwaptionVolatilityStructure> base_;
    std::vector<Real> strikeSpreads_;
    std::vector<std::vector<Handle<Quote>>> volSpreads_;
    QuantLib::ext::shared_ptr<SwapIndex> baseSwapIndexBase_, baseShortSwapIndexBase_;
    QuantLib::ext::shared_ptr<SwapIndex> simulatedSwapIndexBase_, simulatedShortSwapIndexBase_;
    bool stickyAbsMoney_;
    mutable std::vector<Matrix> volSpreadValues_;
    mutable std::vector<Interpolation2D> volSpreadInterpolation_;
};

} // namespace QuantExt
