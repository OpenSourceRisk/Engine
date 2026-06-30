/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/**
 * \file qle/instruments/bondfutureoption.hpp
 * \brief Light wrapper around QuantLib's vanilla option for bond future options.
 *
 * The reason this wrapper is needed is discussed in the QuantLib issue:
 * https://github.com/lballabio/QuantLib/issues/2340.
 * 
 * The commit 8dfdce7cde has a similar approach for bond option, bond TRS and bond forward.
 * In particular, the overridden `calculate()` method simply calls the base class `calculate()` and then sets the
 * `calculated_` flag to `true`. This is necessary because the `calculated_` flag is set to `false` during the 
 * base class `calculate()` call due to a `recalculate` call on a dependent bond instrument when calculating the 
 * bond forward price.
 */

#pragma once
#include <ql/instruments/vanillaoption.hpp>

namespace QuantExt {

class BondFutureOption : public QuantLib::VanillaOption {
public:
    BondFutureOption(const QuantLib::ext::shared_ptr<StrikedTypePayoff>& payoff,
        const QuantLib::ext::shared_ptr<Exercise>& exercise)
        : QuantLib::VanillaOption(payoff, exercise) {}

private:
    void calculate() const override {
        QuantLib::VanillaOption::calculate();
        setCalculated(true);
    }
};

} // namespace QuantExt
