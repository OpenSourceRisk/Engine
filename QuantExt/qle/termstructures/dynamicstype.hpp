/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file dynamicstype.hpp
    \brief dynamics type definitions
    \ingroup termstructures
*/

#ifndef quantext_dynamics_type_hpp
#define quantext_dynamics_type_hpp

#include <ostream>
#include <string>

namespace QuantExt {

/*! \addtogroup termstructures
    @{
*/

//! Stickiness
enum Stickyness { StickyStrike, StickyMoneyness, StickySABR };

//! Reaction to Time Decay
enum ReactionToTimeDecay { ConstantVariance, ForwardForwardVariance };

//! Yield Curve Roll Down
enum YieldCurveRollDown { ConstantDiscounts, ForwardForward };

//! Price Curve Roll Down
enum PriceCurveRollDown { Forward, Spot };

/*! @} */

Stickyness parseStickyness(const std::string& s);
ReactionToTimeDecay parseDecayMode(const std::string& s);
YieldCurveRollDown parseYieldCurveRollDown(const std::string& s);
PriceCurveRollDown parsePriceCurveRollDown(const std::string& s);

std::ostream& operator<<(std::ostream& out, const Stickyness t);
std::ostream& operator<<(std::ostream& out, const ReactionToTimeDecay t);
std::ostream& operator<<(std::ostream& out, const YieldCurveRollDown t);

} // namespace QuantExt

#endif
