/*
Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/impliedbondspread.hpp
\brief utilities for implied bond credit spread calculation

\ingroup instruments
*/

#ifndef quantlib_implied_bond_spread_hpp
#define quantlib_implied_bond_spread_hpp

#include <ql/instruments/bond.hpp>
#include <ql/quotes/simplequote.hpp>

namespace QuantExt {

namespace detail {

//! helper class for implied vanilla bond spread calculation
/*! The passed engine must be linked to the passed quote

     \note this function is meant for developers of bond
           classes so that they can compute a fair credit spread,
           or infer spread from quoted bond price.
*/
class ImpliedBondSpreadHelper {
public:
    static QuantLib::Real calculate(const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond,
                                    const QuantLib::ext::shared_ptr<QuantLib::PricingEngine>& engine,
                                    const QuantLib::ext::shared_ptr<QuantLib::SimpleQuote>& spreadQuote,
                                    QuantLib::Real targetValue,
                                    bool isCleanPrice, // if false, assumes targetValue is based on dirty price
                                    QuantLib::Real accuracy, QuantLib::Natural maxEvaluations, QuantLib::Real minSpread,
                                    QuantLib::Real maxSpread);
};
} // namespace detail

} // namespace QuantExt

#endif
