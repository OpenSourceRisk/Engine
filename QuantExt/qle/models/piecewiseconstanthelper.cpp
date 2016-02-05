/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/piecewiseconstanthelper.hpp>

#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

namespace {

void checkTimes(const Array &t) {
    if (t.size() == 0)
        return;
    QL_REQUIRE(t.front() > 0.0, "first time (" << t.front()
                                               << ") must be positive");
    for (Size i = 0; i < t.size() - 1; ++i) {
        QL_REQUIRE(t[i] < t[i + 1],
                   "times must be strictly increasing, entries at ("
                       << i << "," << i + 1 << ") are (" << t[i] << ","
                       << t[i + 1]);
    }
}

Disposable<Array> datesToTimes(const std::vector<Date> &dates,
                               const Handle<YieldTermStructure> &yts) {
    Array res(dates.size());
    for (Size i = 0; i < dates.size(); ++i) {
        res[i] = yts->timeFromReference(dates[i]);
    }
    return res;
}

} // anonymous namespace

PiecewiseConstantHelper1::PiecewiseConstantHelper1(const Array &t)
    : t_(t), y_(boost::make_shared<PseudoParameter>(t.size() + 1)) {
    checkTimes(t_);
}

PiecewiseConstantHelper1::PiecewiseConstantHelper1(
    const std::vector<Date> &dates, const Handle<YieldTermStructure> &yts)
    : t_(datesToTimes(dates, yts)),
      y_(boost::make_shared<PseudoParameter>(dates.size() + 1)) {
    checkTimes(t_);
}

PiecewiseConstantHelper11::PiecewiseConstantHelper11(const Array &t1,
                                                     const Array &t2)
    : h1_(t1), h2_(t2) {}

PiecewiseConstantHelper11::PiecewiseConstantHelper11(
    const std::vector<Date> &dates1, const std::vector<Date> &dates2,
    const Handle<YieldTermStructure> &yts)
    : h1_(dates1, yts), h2_(dates2, yts) {}

PiecewiseConstantHelper2::PiecewiseConstantHelper2(const Array &t)
    : zeroCutoff_(1.0E-6), t_(t),
      y_(boost::make_shared<PseudoParameter>(t.size() + 1)) {
    checkTimes(t_);
}

PiecewiseConstantHelper2::PiecewiseConstantHelper2(
    const std::vector<Date> &dates, const Handle<YieldTermStructure> &yts)
    : zeroCutoff_(1.0E-6), t_(datesToTimes(dates, yts)),
      y_(boost::make_shared<PseudoParameter>(dates.size() + 1)) {
    checkTimes(t_);
}

PiecewiseConstantHelper3::PiecewiseConstantHelper3(const Array &t)
    : zeroCutoff_(1.0E-6), t_(t),
      y1_(boost::make_shared<PseudoParameter>(t.size() + 1)),
      y2_(boost::make_shared<PseudoParameter>(t.size() + 1)) {
    checkTimes(t_);
}

PiecewiseConstantHelper3::PiecewiseConstantHelper3(
    const std::vector<Date> &dates, const Handle<YieldTermStructure> &yts)
    : zeroCutoff_(1.0E-6), t_(datesToTimes(dates, yts)),
      y1_(boost::make_shared<PseudoParameter>(dates.size() + 1)),
      y2_(boost::make_shared<PseudoParameter>(dates.size() + 1)) {
    checkTimes(t_);
}

} // namespace QuantExt
