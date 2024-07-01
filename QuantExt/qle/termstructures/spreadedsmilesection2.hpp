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

/*! \file qle/termstructures/spreadedsmilesection2.hpp
    \brief smile section with linear interpolated vol spreads
    \ingroup termstructures
*/

#pragma once

#include <ql/math/interpolation.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>

#include <ql/shared_ptr.hpp>

namespace QuantExt {
using namespace QuantLib;

class SpreadedSmileSection2 : public SmileSection {
public:
    /*! - If stickyAbsMoney = true or if strikes are relative to atm and more than one strike is giv en, the base smile
          section must provide an ATM level that does not react to changes in the rate levels. Alternatively, baseAtmLevel
          can be provided.
        - If stickyAbsMoney = true, simulatedAtmLevel must be provided and represent the ATM
          level that _does_ react to changes in the rate levels. */
    SpreadedSmileSection2(const QuantLib::ext::shared_ptr<SmileSection>& base, const std::vector<Real>& volSpreads,
                          const std::vector<Real>& strikes, const bool strikesRelativeToAtm = false,
                          const Real baseAtmLevel = Null<Real>(), const Real simulatedAtmLevel = Null<Real>(),
                          const bool stickyAbsMoney = false);
    Rate minStrike() const override;
    Rate maxStrike() const override;
    Rate atmLevel() const override;

protected:
    Volatility volatilityImpl(Rate strike) const override;

private:
    QuantLib::ext::shared_ptr<SmileSection> base_;
    std::vector<Real> volSpreads_;
    std::vector<Real> strikes_;
    bool strikesRelativeToAtm_;
    Real baseAtmLevel_;
    Real simulatedAtmLevel_;
    bool stickyAbsMoney_;
    Interpolation volSpreadInterpolation_;
};

} // namespace QuantExt
