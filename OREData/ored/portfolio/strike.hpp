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

/*! \file ored/marketdata/strike.hpp
    \brief Classes for representing a strike using various conventions.
    \ingroup marketdata
*/

#pragma once

#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/option.hpp>
#include <ql/types.hpp>

#include <boost/optional.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/version.hpp>
#include <boost/shared_ptr.hpp>

namespace ore {
namespace data {

class Strike {
public:

    Strike();

    Strike(QuantLib::Real strike, std::string strikeCurrency);

    QuantLib::Real strike() const;
    std::string strikeCurrency() const;

private:
    QuantLib::Real strike_;
    std::string strikeCurrency_;

};

} // namespace data
} // namespace ore

// BOOST_CLASS_EXPORT_KEY(ore::data::AbsoluteStrike);