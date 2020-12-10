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

/*! \file qle/termstructures/atmspreadedswaptionvolatility.hpp
    \brief swaption cube defined via atm vol spreads over another cube
    \ingroup termstructures
*/

#pragma once

#include <ql/indexes/swapindex.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvoldiscrete.hpp>

#include <boost/shared_ptr.hpp>

namespace QuantExt {
using namespace QuantLib;

class SpreadedSwaptionSmileSection : public SmileSection {
public:
    SpreadedSwaptionSmileSection(const boost::shared_ptr<SmileSection>& base, const std::vector<Real>& strikeSpreads,
                                 const std::vector<Real>& volSpreads, const Real atmLevel = Null<Real>());
    Rate minStrike() const;
    Rate maxStrike() const;
    Rate atmLevel() const;

protected:
    Volatility volatilityImpl(Rate strike) const;

private:
    boost::shared_ptr<SmileSection> base_;
    std::vector<Real> strikeSpreads_, volSpreads_;
    Real atmLevel_;
    Interpolation volSpreadInterpolation_;
};

class SpreadedSwaptionVolatility : public SwaptionVolatilityDiscrete {
public:
    /* The swap indices define the atm level for the vol spreads. They should be consistent with the base's swap
       indices, if applicable. If the swap indices are not given, only one strike spread 0 is allowed for the vol
       spreads. */
    SpreadedSwaptionVolatility(const Handle<SwaptionVolatilityStructure>& base, const std::vector<Period>& optionTenors,
                               const std::vector<Period>& swapTenors, const std::vector<Real>& strikeSpreads,
                               const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                               const boost::shared_ptr<SwapIndex>& swapIndexBase,
                               const boost::shared_ptr<SwapIndex>& shortSwapIndexBase);

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
    boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const override;
    Volatility volatilityImpl(Time optionTime, Time swapLength, Rate strike) const override;
    void performCalculations() const override;

    Handle<SwaptionVolatilityStructure> base_;
    std::vector<Real> strikeSpreads_;
    std::vector<std::vector<Handle<Quote>>> volSpreads_;
    boost::shared_ptr<SwapIndex> swapIndexBase_, shortSwapIndexBase_;
    mutable std::vector<Matrix> volSpreadValues_;
    mutable Matrix atmLevelValues_;
    mutable std::vector<Interpolation2D> volSpreadInterpolation_;
    mutable Interpolation2D atmLevelInterpolation_;
};

} // namespace QuantExt
