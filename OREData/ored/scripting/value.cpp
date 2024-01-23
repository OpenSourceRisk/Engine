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

#include <ored/scripting/value.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, const EventVec& a) {
    out << a.value;
    return out;
}

std::ostream& operator<<(std::ostream& out, const CurrencyVec& a) {
    out << a.value;
    return out;
}

std::ostream& operator<<(std::ostream& out, const IndexVec& a) {
    out << a.value;
    return out;
}

std::ostream& operator<<(std::ostream& out, const DaycounterVec& a) {
    out << a.value;
    return out;
}

namespace {
struct IsDet : public boost::static_visitor<bool> {
    bool operator()(const RandomVariable& v) const { return v.deterministic(); }
    bool operator()(const Filter& v) const { return v.deterministic(); }
    template <typename C> bool operator()(const C& c) const { return true; }
};

struct SizeGetter : public boost::static_visitor<Size> {
    Size operator()(const RandomVariable& v) const { return v.size(); }
    Size operator()(const Filter& v) const { return v.size(); }
    template <typename C> Size operator()(const C& c) const { return c.size; }
};

struct BinaryOp : public boost::static_visitor<ValueType> {
    BinaryOp(const std::function<RandomVariable(const RandomVariable&, const RandomVariable&)>& op) : op_(op) {}
    ValueType operator()(const RandomVariable& x, const RandomVariable& y) const { return op_(x, y); }
    template <typename T, typename U> ValueType operator()(const T&, const U&) const {
        QL_FAIL("binary operation on invalid types");
    }
    const std::function<RandomVariable(const RandomVariable&, const RandomVariable&)> op_;
};

struct UnaryOp : public boost::static_visitor<ValueType> {
    UnaryOp(const std::function<RandomVariable(const RandomVariable&)>& op) : op_(op) {}
    ValueType operator()(const RandomVariable& x) const { return op_(x); }
    template <typename T> ValueType operator()(const T&) const { QL_FAIL("unary operation on invalid type"); }
    const std::function<RandomVariable(const RandomVariable&)> op_;
};

struct Assignment : public boost::static_visitor<ValueType> {
    ValueType operator()(RandomVariable& x, const RandomVariable& y) const {
        x = y;
        return x;
    }
    template <typename T> ValueType operator()(T& x, const T& y) const {
        x = y;
        return x;
    }
    template <typename T, typename U> ValueType operator()(T& x, const U& y) const {
        QL_FAIL("invalid assignment between incompatible types");
    }
};

struct BinaryComp : public boost::static_visitor<Filter> {
    BinaryComp(const std::function<Filter(const RandomVariable&, const RandomVariable&)>& op_rv,
               const std::function<Filter(const EventVec&, const EventVec&)>& op_event,
               const std::function<Filter(const IndexVec&, const IndexVec&)>& op_index,
               const std::function<Filter(const CurrencyVec&, const CurrencyVec&)>& op_currency,
               const std::function<Filter(const DaycounterVec&, const DaycounterVec&)>& op_daycounter,
               const std::function<Filter(const Filter&, const Filter&)>& op_filter)
        : op_rv(op_rv), op_event(op_event), op_index(op_index), op_currency(op_currency), op_daycounter(op_daycounter),
          op_filter(op_filter) {}
    Filter operator()(const RandomVariable& x, const RandomVariable& y) const { return op_rv(x, y); }
    Filter operator()(const EventVec& x, const EventVec& y) const { return op_event(x, y); }
    Filter operator()(const IndexVec& x, const IndexVec& y) const { return op_index(x, y); }
    Filter operator()(const CurrencyVec& x, const CurrencyVec& y) const { return op_currency(x, y); }
    Filter operator()(const DaycounterVec& x, const DaycounterVec& y) const { return op_daycounter(x, y); }
    Filter operator()(const Filter& x, const Filter& y) const { return op_filter(x, y); }
    template <typename T, typename U> Filter operator()(const T&, const U&) const {
        QL_FAIL("invalid comparison between incompatible types");
    }
    const std::function<Filter(const RandomVariable&, const RandomVariable&)> op_rv;
    const std::function<Filter(const EventVec&, const EventVec&)> op_event;
    const std::function<Filter(const IndexVec&, const IndexVec&)> op_index;
    const std::function<Filter(const CurrencyVec&, const CurrencyVec&)> op_currency;
    const std::function<Filter(const DaycounterVec&, const DaycounterVec&)> op_daycounter;
    const std::function<Filter(const Filter&, const Filter&)> op_filter;
};

struct UnaryComp : public boost::static_visitor<Filter> {
    UnaryComp(const std::function<Filter(const RandomVariable&)>& op_rv,
              const std::function<Filter(const EventVec&)>& op_event,
              const std::function<Filter(const IndexVec&)>& op_index,
              const std::function<Filter(const CurrencyVec&)>& op_currency,
              const std::function<Filter(const DaycounterVec&)>& op_daycounter,
              const std::function<Filter(const Filter&)>& op_filter)
        : op_rv(op_rv), op_event(op_event), op_index(op_index), op_currency(op_currency), op_daycounter(op_daycounter),
          op_filter(op_filter) {}
    Filter operator()(const RandomVariable& x) const { return op_rv(x); }
    Filter operator()(const EventVec& x) const { return op_event(x); }
    Filter operator()(const IndexVec& x) const { return op_index(x); }
    Filter operator()(const CurrencyVec& x) const { return op_currency(x); }
    Filter operator()(const DaycounterVec& x) const { return op_daycounter(x); }
    Filter operator()(const Filter& x) const { return op_filter(x); }
    template <typename T, typename U> Filter operator()(const T&, const U&) const {
        QL_FAIL("invalid comparison between incompatible types");
    }
    const std::function<Filter(const RandomVariable&)> op_rv;
    const std::function<Filter(const EventVec&)> op_event;
    const std::function<Filter(const IndexVec&)> op_index;
    const std::function<Filter(const CurrencyVec&)> op_currency;
    const std::function<Filter(const DaycounterVec&)> op_daycounter;
    const std::function<Filter(const Filter&)> op_filter;
};

} // namespace

bool deterministic(const ValueType& v) { return boost::apply_visitor(IsDet(), v); }
Size size(const ValueType& v) { return boost::apply_visitor(SizeGetter(), v); }

bool operator==(const EventVec& a, const EventVec& b) { return a.value == b.value && a.size == b.size; }
bool operator==(const CurrencyVec& a, const CurrencyVec& b) { return a.value == b.value && a.size == b.size; }
bool operator==(const IndexVec& a, const IndexVec& b) { return a.value == b.value && a.size == b.size; }
bool operator==(const DaycounterVec& a, const DaycounterVec& b) { return a.value == b.value && a.size == b.size; }

ValueType operator+(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryOp([](const RandomVariable& x, const RandomVariable& y) -> RandomVariable { return x + y; }), x, y);
}

ValueType operator-(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryOp([](const RandomVariable& x, const RandomVariable& y) -> RandomVariable { return x - y; }), x, y);
}

ValueType operator*(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryOp([](const RandomVariable& x, const RandomVariable& y) -> RandomVariable { return x * y; }), x, y);
}

ValueType operator/(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryOp([](const RandomVariable& x, const RandomVariable& y) -> RandomVariable { return x / y; }), x, y);
}

ValueType min(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryOp([](const RandomVariable& x, const RandomVariable& y) -> RandomVariable { return min(x, y); }), x, y);
}

ValueType max(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryOp([](const RandomVariable& x, const RandomVariable& y) -> RandomVariable { return max(x, y); }), x, y);
}

ValueType pow(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryOp([](const RandomVariable& x, const RandomVariable& y) -> RandomVariable { return pow(x, y); }), x, y);
}

ValueType operator-(const ValueType& x) { return boost::apply_visitor(UnaryOp(std::negate<RandomVariable>()), x); }

ValueType abs(const ValueType& x) {
    return boost::apply_visitor(UnaryOp([](const RandomVariable& x) -> RandomVariable { return abs(x); }), x);
}

ValueType exp(const ValueType& x) {
    return boost::apply_visitor(UnaryOp([](const RandomVariable& x) -> RandomVariable { return exp(x); }), x);
}

ValueType log(const ValueType& x) {
    return boost::apply_visitor(UnaryOp([](const RandomVariable& x) -> RandomVariable { return log(x); }), x);
}

ValueType sqrt(const ValueType& x) {
    return boost::apply_visitor(UnaryOp([](const RandomVariable& x) -> RandomVariable { return sqrt(x); }), x);
}

ValueType normalCdf(const ValueType& x) {
    return boost::apply_visitor(UnaryOp([](const RandomVariable& x) -> RandomVariable { return normalCdf(x); }), x);
}

ValueType normalPdf(const ValueType& x) {
    return boost::apply_visitor(UnaryOp([](const RandomVariable& x) -> RandomVariable { return normalPdf(x); }), x);
}

ValueType typeSafeAssign(ValueType& x, const ValueType& y) { return boost::apply_visitor(Assignment(), x, y); }

Filter equal(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryComp(
            [](const RandomVariable& x, const RandomVariable& y) -> Filter { return close_enough(x, y); },
            [](const EventVec& x, const EventVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size EventVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value == y.value);
            },
            [](const IndexVec& x, const IndexVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size IndexVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value == y.value);
            },
            [](const CurrencyVec& x, const CurrencyVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size CurrencyVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value == y.value);
            },
            [](const DaycounterVec& x, const DaycounterVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size DaycounterVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value == y.value);
            },
            [](const Filter& x, const Filter& y) -> Filter { return equal(x, y); }),
        x, y);
}

Filter notequal(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryComp(
            [](const RandomVariable& x, const RandomVariable& y) -> Filter { return !close_enough(x, y); },
            [](const EventVec& x, const EventVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size EventVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value != y.value);
            },
            [](const IndexVec& x, const IndexVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size IndexVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value != y.value);
            },
            [](const CurrencyVec& x, const CurrencyVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size CurrencyVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value != y.value);
            },
            [](const DaycounterVec& x, const DaycounterVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size DaycounterVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value != y.value);
            },
            [](const Filter& x, const Filter& y) -> Filter { return !equal(x, y); }),
        x, y);
}

Filter lt(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryComp(
            [](const RandomVariable& x, const RandomVariable& y) -> Filter { return x < y; },
            [](const EventVec& x, const EventVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size EventVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value < y.value);
            },
            [](const IndexVec& x, const IndexVec& y) -> Filter { QL_FAIL("invalid comparison lt for IndexVec"); },
            [](const CurrencyVec& x, const CurrencyVec& y) -> Filter {
                QL_FAIL("invalid comparison lt for CurrencyVec");
            },
            [](const DaycounterVec& x, const DaycounterVec& y) -> Filter {
                QL_FAIL("invalid comparison lt for DaycounterVec");
            },
            [](const Filter& x, const Filter& y) -> Filter { QL_FAIL("invalid comparison lt for Filter"); }),
        x, y);
}

Filter gt(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryComp(
            [](const RandomVariable& x, const RandomVariable& y) -> Filter { return x > y; },
            [](const EventVec& x, const EventVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size EventVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value > y.value);
            },
            [](const IndexVec& x, const IndexVec& y) -> Filter { QL_FAIL("invalid comparison gt for IndexVec"); },
            [](const CurrencyVec& x, const CurrencyVec& y) -> Filter {
                QL_FAIL("invalid comparison gt for CurrencyVec");
            },
            [](const DaycounterVec& x, const DaycounterVec& y) -> Filter {
                QL_FAIL("invalid comparison gt for DaycounterVec");
            },
            [](const Filter& x, const Filter& y) -> Filter { QL_FAIL("invalid comparison gt for Filter"); }),
        x, y);
}

Filter leq(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryComp(
            [](const RandomVariable& x, const RandomVariable& y) -> Filter { return x <= y; },
            [](const EventVec& x, const EventVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size EventVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value <= y.value);
            },
            [](const IndexVec& x, const IndexVec& y) -> Filter { QL_FAIL("invalid comparison leq for IndexVec"); },
            [](const CurrencyVec& x, const CurrencyVec& y) -> Filter {
                QL_FAIL("invalid comparison leq for CurrencyVec");
            },
            [](const DaycounterVec& x, const DaycounterVec& y) -> Filter {
                QL_FAIL("invalid comparison leq for DaycounterVec");
            },
            [](const Filter& x, const Filter& y) -> Filter { QL_FAIL("invalid comparison leq for Filter"); }),

        x, y);
}

Filter geq(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryComp(
            [](const RandomVariable& x, const RandomVariable& y) -> Filter { return x >= y; },
            [](const EventVec& x, const EventVec& y) -> Filter {
                QL_REQUIRE(x.size == y.size, "inconsistent size EventVec (" << x.size << ", " << y.size << ")");
                return Filter(x.size, x.value >= y.value);
            },
            [](const IndexVec& x, const IndexVec& y) -> Filter { QL_FAIL("invalid comparison geq for IndexVec"); },
            [](const CurrencyVec& x, const CurrencyVec& y) -> Filter {
                QL_FAIL("invalid comparison geq for CurrencyVec");
            },
            [](const DaycounterVec& x, const DaycounterVec& y) -> Filter {
                QL_FAIL("invalid comparison geq for DaycounterVec");
            },
            [](const Filter& x, const Filter& y) -> Filter { QL_FAIL("invalid comparison geq for Filter"); }),

        x, y);
}

Filter logicalNot(const ValueType& x) {
    return boost::apply_visitor(
        UnaryComp([](const RandomVariable& x) -> Filter { QL_FAIL("invalid logicalNot for RandomVariable"); },
                  [](const EventVec& x) -> Filter { QL_FAIL("invalid logicalNot for EventVec"); },
                  [](const IndexVec& x) -> Filter { QL_FAIL("invalid logicalNot for IndexVec"); },
                  [](const CurrencyVec& x) -> Filter { QL_FAIL("invalid logicalNot for CurrencyVec"); },
                  [](const DaycounterVec& x) -> Filter { QL_FAIL("invalid logicalNot for DaycounterVec"); },
                  [](const Filter& x) -> Filter { return !x; }),
        x);
}

Filter logicalAnd(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryComp(
            [](const RandomVariable& x, const RandomVariable& y) -> Filter {
                QL_FAIL("invalid logicalAnd for RandomVariable");
            },
            [](const EventVec& x, const EventVec& y) -> Filter { QL_FAIL("invalid logicalAnd for EventVec"); },
            [](const IndexVec& x, const IndexVec& y) -> Filter { QL_FAIL("invalid logicalAnd for IndexVec"); },
            [](const CurrencyVec& x, const CurrencyVec& y) -> Filter { QL_FAIL("invalid logicalAnd for CurrencyVec"); },
            [](const DaycounterVec& x, const DaycounterVec& y) -> Filter {
                QL_FAIL("invalid logicalAnd for DaycounterVec");
            },
            [](const Filter& x, const Filter& y) -> Filter { return x && y; }),
        x, y);
}

Filter logicalOr(const ValueType& x, const ValueType& y) {
    return boost::apply_visitor(
        BinaryComp(
            [](const RandomVariable& x, const RandomVariable& y) -> Filter {
                QL_FAIL("invalid logicalOr for RandomVariable");
            },
            [](const EventVec& x, const EventVec& y) -> Filter { QL_FAIL("invalid logicalOr for EventVec"); },
            [](const IndexVec& x, const IndexVec& y) -> Filter { QL_FAIL("invalid logicalOr for IndexVec"); },
            [](const CurrencyVec& x, const CurrencyVec& y) -> Filter { QL_FAIL("invalid logicalOr for CurrencyVec"); },
            [](const DaycounterVec& x, const DaycounterVec& y) -> Filter {
                QL_FAIL("invalid logicalOr for DaycounterVec");
            },
            [](const Filter& x, const Filter& y) -> Filter { return x || y; }),
        x, y);
}

} // namespace data
} // namespace ore
