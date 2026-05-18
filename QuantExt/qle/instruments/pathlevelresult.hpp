/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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

/*! \file pathlevelresult.hpp
    \brief class holding additional results on path level

    \ingroup instruments
*/

#pragma once

#include <ql/utilities/null.hpp>

namespace QuantExt {

struct PathLevelResult {
    std::string resultId;
    QuantLib::Size index = QuantLib::Null<Size>();
    QuantLib::Date date;
    double time = QuantLib::Null<double>();
    std::vector<double> values;
};

} // namespace QuantExt
