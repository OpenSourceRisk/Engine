/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/inflation/inflationtraits.hpp
    \brief interpolated correlation term structure
*/

#pragma once

#include <ql/termstructures/bootstraphelper.hpp>
#include <ql/termstructures/inflation/interpolatedyoyinflationcurve.hpp>
#include <ql/termstructures/inflation/interpolatedzeroinflationcurve.hpp>

namespace QuantExt {

namespace detail {
const QuantLib::Rate avgInflation = 0.02;
const QuantLib::Rate maxInflation = 0.5;
} // namespace detail

//! Bootstrap traits to use for PiecewiseZeroInflationCurve
class ZeroInflationTraits {
public:
    class BootstrapFirstDateInitializer {
    public:
        virtual ~BootstrapFirstDateInitializer() = default;
        virtual QuantLib::Date initialDate() const = 0;
    };

    typedef QuantLib::BootstrapHelper<QuantLib::ZeroInflationTermStructure> helper;

    // start of curve data
    static QuantLib::Date initialDate(const BootstrapFirstDateInitializer* t) { return t->initialDate(); }
    // value at reference date
    static QuantLib::Rate initialValue(const QuantLib::ZeroInflationTermStructure* t) { return t->baseRate(); }

    // guesses
    template <class C>
    static QuantLib::Rate guess(QuantLib::Size i, const C* c, bool validData,
                                QuantLib::Size) // firstAliveHelper
    {
        if (validData) // previous iteration value
            return c->data()[i];

        if (i == 1) // first pillar
            return detail::avgInflation;

        // could/should extrapolate
        return detail::avgInflation;
    }

    // constraints
    template <class C>
    static QuantLib::Rate minValueAfter(QuantLib::Size i, const C* c, bool validData,
                                        QuantLib::Size) // firstAliveHelper
    {
        if (validData) {
            QuantLib::Rate r = *(std::min_element(c->data().begin(), c->data().end()));
            return r < 0.0 ? r * 2.0 : r / 2.0;
        }
        return -detail::maxInflation;
    }
    template <class C>
    static QuantLib::Rate maxValueAfter(QuantLib::Size i, const C* c, bool validData,
                                        QuantLib::Size) // firstAliveHelper
    {
        if (validData) {
            QuantLib::Rate r = *(std::max_element(c->data().begin(), c->data().end()));
            return r < 0.0 ? r / 2.0 : r * 2.0;
        }
        // no constraints.
        // We choose as max a value very unlikely to be exceeded.
        return detail::maxInflation;
    }

    // update with new guess
    static void updateGuess(std::vector<QuantLib::Rate>& data, QuantLib::Rate level, QuantLib::Size i) {
        data[i] = level;
    }
    // upper bound for convergence loop
    // calibration is trivial, should be immediate
    static QuantLib::Size maxIterations() { return 5; }
};
} // namespace QuantExt
