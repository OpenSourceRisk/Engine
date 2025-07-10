/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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
#pragma once

#include <ql/termstructures/bootstraphelper.hpp>
#include <qle/termstructures/inflation/cpicurve.hpp>
#include <qle/termstructures/inflation/interpolatedcpiinflationcurve.hpp>

namespace QuantExt {

namespace detail {
constexpr double minCPI = 1.0;
constexpr double maxCPI = 100000.0;
} // namespace detail

//! Bootstrap traits to use for PiecewiseZeroInflationCurve
class CPITraits {
public:
    template <class Interpolator> struct curve {
        typedef InterpolatedCPIInflationCurve<Interpolator> type;
    };
    typedef QuantLib::BootstrapHelper<QuantLib::ZeroInflationTermStructure> helper;
    // start of curve data
    static QuantLib::Date initialDate(const QuantLib::ZeroInflationTermStructure* t) {
        if (t->hasExplicitBaseDate())
            return t->baseDate();
        else
            return QuantLib::inflationPeriod(t->referenceDate() - t->observationLag(), t->frequency()).first;
    }
    // value at reference date

    static QuantLib::Rate initialValue(const CPICurve* ts) {
        // this will be overwritten during bootstrap
        return ts->baseCPI();
    }

    // guesses
    template <class C>
    static QuantLib::Rate guess(QuantLib::Size i, const C* c, bool validData,
                                QuantLib::Size) // firstAliveHelper
    {
        if (validData) // previous iteration value
            return c->data()[i];
        return c->baseCPI();
    }

    // constraints
    template <class C>
    static QuantLib::Rate minValueAfter(QuantLib::Size, const C* c, bool validData,
                                        QuantLib::Size) // firstAliveHelper
    {
        if (validData) {
            QuantLib::Rate r = *(std::min_element(c->data().begin(), c->data().end()));
            return r < 0.0 ? QuantLib::Real(r * 2.0) : r / 2.0;
        }
        return detail::minCPI;
    }
    template <class C>
    static QuantLib::Rate maxValueAfter(QuantLib::Size, const C* c, bool validData,
                                        QuantLib::Size) // firstAliveHelper
    {
        if (validData) {
            QuantLib::Rate r = *(std::max_element(c->data().begin(), c->data().end()));
            return r < 0.0 ? QuantLib::Real(r / 2.0) : r * 2.0;
        }
        // no constraints.
        // We choose as max a value very unlikely to be exceeded.
        return detail::maxCPI;
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
