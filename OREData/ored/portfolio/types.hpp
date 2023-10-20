/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/types.hpp
    \brief payment lag
    \ingroup portfolio
*/

#pragma once

#include <ql/types.hpp>
#include <boost/variant.hpp>

namespace ore {
namespace data {

typedef boost::variant<QuantLib::Period, QuantLib::Natural> PaymentLag;

struct PaymentLagPeriod : public boost::static_visitor<QuantLib::Period> {
public:
    QuantLib::Period operator()(const QuantLib::Natural& n) const { return Period(n, Days); }
    QuantLib::Period operator()(const QuantLib::Period& p) const { return p; }
};

struct PaymentLagInteger : public boost::static_visitor<QuantLib::Natural> {
public:
    QuantLib::Natural operator()(const QuantLib::Natural& n) const { return n; }
    QuantLib::Natural operator()(const QuantLib::Period& p) const { return static_cast<QuantLib::Natural>(days(p)); }
};

} // namespace data
} // namespace ore
