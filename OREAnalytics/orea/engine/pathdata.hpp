/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file pathdata.hpp
    \brief path data
*/

#pragma once

#include <qle/math/randomvariable.hpp>

#include <boost/serialization/vector.hpp>

namespace ore {
namespace analytics {

struct PathData {
    std::vector<std::vector<std::vector<QuantLib::Real>>> fxBuffer;
    std::vector<std::vector<std::vector<QuantLib::Real>>> irStateBuffer;
    std::vector<QuantLib::Real> pathTimes;
    std::vector<std::vector<QuantExt::RandomVariable>> paths;
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version) {
        ar& fxBuffer;
        ar& irStateBuffer;
        ar& pathTimes;
        ar& paths;
    }
};

} // namespace analytics
} // namespace ore
