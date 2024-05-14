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

/*! \file ored/scripting/value.hpp
    \brief value type and operations
    \ingroup utilities
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_io.hpp>

#include <ql/time/date.hpp>

#include <boost/variant.hpp>

#include <ostream>
#include <string>

namespace ore {
namespace data {
using namespace QuantExt;

struct EventVec {
    Size size;
    Date value;
};

struct CurrencyVec {
    Size size;
    std::string value;
};

struct IndexVec {
    Size size;
    std::string value;
};

struct DaycounterVec {
    Size size;
    std::string value;
};

using ValueType = boost::variant<RandomVariable, EventVec, CurrencyVec, IndexVec, DaycounterVec, Filter>;

static const std::vector<std::string> valueTypeLabels = {"Number", "Event",      "Currency",
                                                         "Index",  "Daycounter", "Filter"};

struct ValueTypeWhich {
    enum which { Number = 0, Event = 1, Currency = 2, Index = 3, Daycounter = 4, Filter = 5 };
};

bool deterministic(const ValueType& v);
Size size(const ValueType& v);

bool operator==(const EventVec& a, const EventVec& b);
bool operator==(const CurrencyVec& a, const CurrencyVec& b);
bool operator==(const IndexVec& a, const IndexVec& b);
bool operator==(const DaycounterVec& a, const DaycounterVec& b);
std::ostream& operator<<(std::ostream& out, const EventVec& a);
std::ostream& operator<<(std::ostream& out, const CurrencyVec& a);
std::ostream& operator<<(std::ostream& out, const IndexVec& a);
std::ostream& operator<<(std::ostream& out, const DaycounterVec& a);

ValueType operator+(const ValueType& x, const ValueType& y);
ValueType operator-(const ValueType& x, const ValueType& y);
ValueType operator*(const ValueType& x, const ValueType& y);
ValueType operator/(const ValueType& x, const ValueType& y);
ValueType min(const ValueType& x, const ValueType& y);
ValueType max(const ValueType& x, const ValueType& y);
ValueType pow(const ValueType& x, const ValueType& y);

ValueType operator-(const ValueType& x);
ValueType abs(const ValueType& x);
ValueType exp(const ValueType& x);
ValueType log(const ValueType& x);
ValueType sqrt(const ValueType& x);
ValueType normalCdf(const ValueType& x);
ValueType normalPdf(const ValueType& x);

ValueType typeSafeAssign(ValueType& x, const ValueType& y);

Filter equal(const ValueType& x, const ValueType& y);
Filter notequal(const ValueType& x, const ValueType& y);
Filter lt(const ValueType& x, const ValueType& y);
Filter leq(const ValueType& x, const ValueType& y);
Filter gt(const ValueType& x, const ValueType& y);
Filter geq(const ValueType& x, const ValueType& y);

Filter logicalNot(const ValueType& x);
Filter logicalAnd(const ValueType& x, const ValueType& y);
Filter logicalOr(const ValueType& x, const ValueType& y);

} // namespace data
} // namespace ore
