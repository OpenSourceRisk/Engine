/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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

#include <ql/time/daycounters/actualactual.hpp>
#include <qle/time/yearcounter.hpp>

using QuantLib::Date;
using QuantLib::Time;

namespace QuantExt {
namespace {
    QuantLib::DayCounter underlyingDCF = QuantLib::ActualActual(QuantLib::ActualActual::ISDA);
}

Time YearCounter::Impl::yearFraction(const Date& d1, const Date& d2, const Date&, const Date&) const {
    int years =0;
    int months = 0;
    years = d2.year() - d1.year();
    months = d2.month() - d1.month();
    return static_cast<Time>(years * 12 + months) / 12.0;
}

} // namespace QuantExt
