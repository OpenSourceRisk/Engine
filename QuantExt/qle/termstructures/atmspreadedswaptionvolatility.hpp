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

#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvoldiscrete.hpp>

#include <boost/shared_ptr.hpp>

namespace QuantExt {
using namespace QuantLib;

class AtmSpreadedSmileSection : public SmileSection {
public:
    AtmSpreadedSmileSection(const boost::shared_ptr<SmileSection>& base, const Real spread);
    Rate minStrike() const;
    Rate maxStrike() const;
    Rate atmLevel() const;

protected:
    Volatility volatilityImpl(Rate strike) const;

private:
    boost::shared_ptr<SmileSection> base_;
    Real spread_;
};

class AtmSpreadedSwaptionVolatility : public SwaptionVolatilityDiscrete {
public:
    AtmSpreadedSwaptionVolatility(const Handle<SwaptionVolatilityStructure>& base,
                                  const std::vector<Period>& optionTenors, const std::vector<Period>& swapTenors,
                                  const std::vector<std::vector<Handle<Quote>>>& spreads);

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
    std::vector<Period> optionTenors_, swapTenors_;
    std::vector<std::vector<Handle<Quote>>> spreads_;
    mutable Matrix spreadValues_;
    mutable Interpolation2D spread_;
};

} // namespace QuantExt
