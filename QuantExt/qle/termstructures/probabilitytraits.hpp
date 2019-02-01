/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/probabilitytraits.hpp
    \brief default probability bootstrap traits for QuantExt
*/

#ifndef quantext_probability_traits_hpp
#define quantext_probability_traits_hpp

#include <ql/termstructures/bootstraphelper.hpp>
#include <ql/termstructures/credit/interpolateddefaultdensitycurve.hpp>
#include <ql/termstructures/credit/interpolatedhazardratecurve.hpp>
#include <ql/termstructures/credit/interpolatedsurvivalprobabilitycurve.hpp>

namespace QuantExt {

namespace detail {
const QuantLib::Real avgHazardRate = 0.01;
const QuantLib::Real maxHazardRate = 3.0;
} // namespace detail

//! Survival probability curve traits
struct SurvivalProbability {

    // interpolated curve type
    template <class Interpolator> struct curve {
        typedef QuantLib::InterpolatedSurvivalProbabilityCurve<Interpolator> type;
    };

    // helper class
    typedef QuantLib::BootstrapHelper<QuantLib::DefaultProbabilityTermStructure> helper;

    // start of curve data
    static QuantLib::Date initialDate(const QuantLib::DefaultProbabilityTermStructure* c) { return c->referenceDate(); }

    // value at reference date
    static QuantLib::Real initialValue(const QuantLib::DefaultProbabilityTermStructure*) { return 1.0; }

    // guesses
    template <class C> static QuantLib::Real guess(QuantLib::Size i, const C* c, bool validData, QuantLib::Size) {

        // If have already bootstrapped some points, use the previous point
        if (validData)
            return c->data()[i];

        // If haven't already bootstrapped some points, initial guess
        if (i == 1)
            return 1.0 / (1.0 + detail::avgHazardRate * 0.25);

        // extrapolate
        Date d = c->dates()[i];
        return c->survivalProbability(d, true);
    }

    // constraints
    template <class C>
    static QuantLib::Real minValueAfter(QuantLib::Size i, const C* c, bool validData, QuantLib::Size) {

        if (validData) {
            return c->data().back() / 2.0;
        }

        Time dt = c->times()[i] - c->times()[i - 1];
        return c->data()[i - 1] * std::exp(-detail::maxHazardRate * dt);
    }

    template <class C>
    static QuantLib::Real maxValueAfter(QuantLib::Size i, const C* c, bool validData, QuantLib::Size) {
        // survival probability cannot increase
        return c->data()[i - 1];
    }

    // root-finding update
    static void updateGuess(std::vector<QuantLib::Real>& data, QuantLib::Probability p, QuantLib::Size i) {
        data[i] = p;
    }

    // upper bound for convergence loop
    static QuantLib::Size maxIterations() { return 50; }
};

} // namespace QuantExt

#endif
