/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file amccalculator.hpp
    \brief interface for amc calculator
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/processes/crossassetstateprocess.hpp>

#include <ql/currency.hpp>
#include <ql/math/array.hpp>
#include <ql/methods/montecarlo/multipath.hpp>

namespace QuantExt {

/*! base class for amc interfaces */
class AmcCalculator {
public:
    virtual ~AmcCalculator() {}

    /*! currency of simulated npvs */
    virtual QuantLib::Currency npvCurrency() = 0;
};

/*! interface 1: takes a single path and returns the npvs for this path */
class AmcCalculatorSinglePath : public AmcCalculator {
public:
    /*! simulate a path and get back the simulated npvs */
    virtual QuantLib::Array simulatePath(const QuantLib::MultiPath& path, const bool reuseLastEvents) = 0;
};

/*! interface 2: takes paths as an array, returns all npvs */
class AmcCalculatorMultiVariates : public AmcCalculator {
public:
    /*! - simulate paths on given times and return simulated npvs for all paths
        - isRelevantTime marks the entries in paths that should be simulated in the end
        - if stickyCloseOutRun is true, the simulation times should be taken from the previous index
     */
    virtual std::vector<QuantExt::RandomVariable>
    simulatePath(const std::vector<QuantLib::Real>& pathTimes,
                 std::vector<std::vector<QuantExt::RandomVariable>>& paths, const std::vector<bool>& isRelevantTime,
                 const bool stickyCloseOutRun) = 0;
};

} // namespace QuantExt
