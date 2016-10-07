/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/utilities/strike.hpp
    \brief strike description
    \ingroup utilities
*/

#pragma once

#include <ql/types.hpp>

#include <string>

namespace openriskengine {
namespace data {

struct Strike {
    enum class Type { ATM, ATMF, ATM_Offset, Absolute, Delta };
    Type type;
    QuantLib::Real value;
};

//! Convert text to Strike
/*!
\ingroup utilities
*/
Strike parseStrike(const std::string& s);

//! Convert Strike to text
/*!
\ingroup utilities
*/
std::ostream& operator<<(std::ostream& out, const Strike& s);
}
}
