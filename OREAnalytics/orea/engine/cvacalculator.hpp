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

/*! \file orea/engine/cvacalculator.hpp
    \brief base class for CVA calculation
*/

#pragma once

#include <ql/types.hpp>
#include <map>
#include <set>
#include <string>

namespace ore {
namespace analytics {

//! A base class for different CVA capital charge calculation methods
class CvaCalculator {
public:
    //! Constructor
    CvaCalculator() {};

    //! Destructor
    virtual ~CvaCalculator() {};

    //! a virtual function for calculating the CVA captial charge
    virtual void calculate() = 0;

    //! Give back the CVA results container for the given \p portfolioId
    QuantLib::Real cvaResult() const {
        return cvaResult_;
    }

    //! Return the calculator's calculation currency
    const std::string& calculationCurrency() const { return calculationCcy_; }

protected:
    //! The SIMM calculation currency i.e. the currency of the SIMM results
    std::string calculationCcy_;

    //! Container with a CvaResults object for each portfolio ID
    QuantLib::Real cvaResult_;

};

} // namespace analytics
} // namespace ore
