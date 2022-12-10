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

/*! \file qle/termstructures/multisectiondefaultcurve.hpp
    \brief default curve with an instantaneous hazard rate given by a vector of underlying curves in specific date
   ranges \ingroup termstructures
*/

#pragma once

#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! multi section default ts
/*! the instantaneous hazard rate is defined by the ith source curve for dates before the ith switch date and after
 the (i-1)th switch date; all source curves must be consistently floating or fixed and have the same reference date
 always; the day counter of all source curves should coincide with the dc of this curve
*/
class MultiSectionDefaultCurve : public SurvivalProbabilityStructure {
public:
    MultiSectionDefaultCurve(const std::vector<Handle<DefaultProbabilityTermStructure> >& sourceCurves,
                             const std::vector<Handle<Quote> > recoveryRates, const std::vector<Date>& switchDates,
                             const Handle<Quote> recoveryRate, const DayCounter& dayCounter, const bool extrapolate)
        : SurvivalProbabilityStructure(dayCounter), sourceCurves_(sourceCurves), recoveryRates_(recoveryRates),
          switchDates_(switchDates), recoveryRate_(recoveryRate) {
        QL_REQUIRE(!sourceCurves_.empty(), "no source curves given");
        QL_REQUIRE(sourceCurves_.size() - 1 == switchDates_.size(),
                   "source curve size (" << sourceCurves_.size() << ") minus 1 and switch dates size ("
                                         << switchDates_.size() << ") do not match");
        QL_REQUIRE(sourceCurves_.size() == recoveryRates_.size(),
                   "source curve size (" << sourceCurves_.size() << ") must match recovery rates size ("
                                         << recoveryRates_.size() << ")");
        switchTimes_.resize(switchDates_.size());
        for (Size i = 1; i < switchDates_.size(); ++i) {
            QL_REQUIRE(switchDates_[i] > switchDates_[i - 1], "switch dates must be strictly ascending, got "
                                                                  << switchDates_[i - 1] << ", " << switchDates_[i]
                                                                  << " at indices " << i - 1 << ", " << i);
        }
        for (auto const& s : sourceCurves_)
            registerWith(s);
        for (auto const& r : recoveryRates_)
            registerWith(r);
        enableExtrapolation(extrapolate);
        update();
    }

    Date maxDate() const override { return sourceCurves_.back()->maxDate(); }
    const Date& referenceDate() const override { return sourceCurves_.front()->referenceDate(); }

protected:
    Real survivalProbabilityImpl(Time t) const override {
        Size idx = std::lower_bound(switchTimes_.begin(), switchTimes_.end(), t,
                                    [](const Real s, const Real t) { return s < t && !QuantLib::close_enough(s, t); }) -
                   switchTimes_.begin();
        QL_REQUIRE(idx < sourceCurves_.size(), "internal error: source curve index is "
                                                   << idx << ", number of source curves is " << sourceCurves_.size());
        Real prob = 1.0;
        for (Size i = 0; i < idx; ++i) {
            Real t0 = i == 0 ? 0.0 : std::max(switchTimes_[i - 1], 0.0);
            Real t1 = switchTimes_[i];
            if (t1 > 0.0)
                prob *= std::pow(sourceCurves_[i]->survivalProbability(t1) / sourceCurves_[i]->survivalProbability(t0),
                                 1.0 - recoveryRates_[i]->value());
        }
        // we know that t > 0
        Real t0 = idx == 0 ? 0.0 : std::max(switchTimes_[idx - 1], 0.0);
        prob *= std::pow(sourceCurves_[idx]->survivalProbability(t) / sourceCurves_[idx]->survivalProbability(t0),
                         1.0 - recoveryRates_[idx]->value());
        prob = std::pow(prob, 1.0 / (1.0 - recoveryRate_->value()));
        return prob;
    }
    void update() override {
        SurvivalProbabilityStructure::update();
        for (Size i = 0; i < switchDates_.size(); ++i)
            switchTimes_[i] = timeFromReference(switchDates_[i]);
    }
    const std::vector<Handle<DefaultProbabilityTermStructure> > sourceCurves_;
    const std::vector<Handle<Quote> > recoveryRates_;
    const std::vector<Date> switchDates_;
    const Handle<Quote> recoveryRate_;
    mutable std::vector<Time> switchTimes_;
};
} // namespace QuantExt
