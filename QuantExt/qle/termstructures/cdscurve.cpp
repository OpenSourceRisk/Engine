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

#include <qle/termstructures/cdscurve.hpp>
#include <qle/utilities/time.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>
#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

namespace {
class TermInterpolatedDefaultCurve : public SurvivalProbabilityStructure {
public:
    TermInterpolatedDefaultCurve(const Handle<DefaultProbabilityTermStructure>& c1,
                                 const Handle<DefaultProbabilityTermStructure>& c2, const Real alpha)
        : SurvivalProbabilityStructure(c1->dayCounter()), c1_(c1), c2_(c2), alpha_(alpha) {
        registerWith(c1_);
        registerWith(c2_);
    }
    Date maxDate() const override { return std::min(c1_->maxDate(), c2_->maxDate()); }
    Time maxTime() const override { return std::min(c1_->maxTime(), c2_->maxTime()); }
    const Date& referenceDate() const override { return c1_->referenceDate(); }
    Calendar calendar() const override { return c1_->calendar(); }
    Natural settlementDays() const override { return c1_->settlementDays(); }
    Probability survivalProbabilityImpl(Time t) const override {
        return std::pow(c1_->survivalProbability(t), alpha_) * std::pow(c2_->survivalProbability(t), 1.0 - alpha_);
    }

private:
    Handle<DefaultProbabilityTermStructure> c1_, c2_;
    Real alpha_;
};
} // namespace

CdsCurve::CdsCurve(const Handle<DefaultProbabilityTermStructure>& curve) : CdsCurve({0 * Days}, {}, {}) {}

CdsCurve::CdsCurve(const std::vector<Period>& terms,
                   const std::vector<Handle<DefaultProbabilityTermStructure>> termCurves, const RefData& refData)
    : terms_(terms), termCurves_(termCurves), refData_(refData) {
    QL_REQUIRE(!termCurves_.empty(), "CdsCurve: no term curves given");
    QL_REQUIRE(terms_.size() == termCurves_.size(), "CdsCurve: terms size (" << terms_.size()
                                                                             << ") must match term curves size ("
                                                                             << termCurves_.size() << ")");
    for (Size i = 0; i < terms_.size(); ++i) {
        QL_REQUIRE(i == terms_.size() - 1 || terms_[i] < terms_[i + 1], "CdsCurve: expected terms["
                                                                            << i << "] (" << terms_[i] << ") < terms["
                                                                            << i + 1 << "] (" << terms_[i + 1] << ").");
        termTimes_.push_back(periodToTime(terms_[i]));
    }
    for (auto const& c : termCurves_)
        registerWith(c);
}

void CdsCurve::update() { notifyObservers(); }

const CdsCurve::RefData& CdsCurve::refData() const { return refData_; }

Handle<DefaultProbabilityTermStructure> CdsCurve::curve(const Period& term) {
    if (termCurves_.size() == 1 || term == 0 * Days)
        return termCurves_.front();
    Real t = periodToTime(term);
    if (t < termTimes_.front())
        return termCurves_.front();
    else if (t > termTimes_.back())
        return termCurves_.back();
    else {
        Size index = std::distance(termTimes_.begin(),
                                   std::lower_bound(termTimes_.begin(), termTimes_.end(), t,
                                                    [](Real t1, Real t2) { return t1 < t2 && !close_enough(t1, t2); }));
        if (index >= termTimes_.size() - 1)
            return termCurves_.back();
        return Handle<DefaultProbabilityTermStructure>(boost::make_shared<TermInterpolatedDefaultCurve>(
            termCurves_[index], termCurves_[index + 1],
            (termTimes_[index + 1] - t) / (termTimes_[index + 1] - termTimes_[index])));
    }
}

} // namespace QuantExt
