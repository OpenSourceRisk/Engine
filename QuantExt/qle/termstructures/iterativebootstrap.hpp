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

/*! \file qle/termstructures/iterativebootstrap.hpp
    \brief Straight copy of ql/termstructures/iterativebootstrap.hpp with minor changes
*/

#ifndef quantext_iterative_bootstrap_hpp
#define quantext_iterative_bootstrap_hpp

#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/math/solvers1d/finitedifferencenewtonsafe.hpp>
#include <ql/termstructures/bootstraperror.hpp>
#include <ql/termstructures/bootstraphelper.hpp>
#include <ql/utilities/dataformatters.hpp>

namespace QuantExt {

namespace detail {

/*! If \c dontThrow is \c true in QuantExt::IterativeBootstrap and on a given pillar the bootstrap fails when
    searching for a helper root between \c xMin and \c xMax, we use this function to return the value that gives
    the minimum absolute helper error in the interval between \c xMin and \c xMax inclusive.
*/
template <class Curve>
QuantLib::Real dontThrowFallback(const QuantLib::BootstrapError<Curve>& error, QuantLib::Real xMin, QuantLib::Real xMax,
                                 QuantLib::Size steps) {

    QL_REQUIRE(xMin < xMax, "Expected xMin to be less than xMax");

    QuantLib::Real result = xMin;
    QuantLib::Real minError = QL_MAX_REAL;
    QuantLib::Real stepSize = (xMax - xMin) / steps;

    for (QuantLib::Size i = 0; i <= steps; ++i) {
        QuantLib::Real x = xMin + stepSize * static_cast<double>(i);
        QuantLib::Real absError = QL_MAX_REAL;
        try {
            absError = std::abs(error(x));
        } catch (...) {
        }

        if (absError < minError) {
            result = x;
            minError = absError;
        }
    }

    return result;
}

} // namespace detail

/*! Straight copy of QuantLib::IterativeBootstrap with the following modifications
    - addition of a \c globalAccuracy parameter to allow the global bootstrap accuracy to be different than the
      \c accuracy specified in the \c Curve. In particular, allows for the \c globalAccuracy to be greater than the
      \c accuracy specified in the \c Curve which is useful in some situations e.g. cubic spline and optionlet
      stripping. If the \c globalAccuracy is set less than the \c accuracy in the \c Curve, the \c accuracy in the
      \c Curve is used instead.
*/
template <class Curve> class IterativeBootstrap {
    typedef typename Curve::traits_type Traits;
    typedef typename Curve::interpolator_type Interpolator;

public:
    /*! Constructor
        \param accuracy       Accuracy for the bootstrap. If \c Null<Real>(), its value is taken from the
                              termstructure's accuracy.
        \param globalAccuracy Accuracy for the global bootstrap stopping criterion. If it is set to
                              \c Null<Real>(), its value is taken from the termstructure's accuracy.
        \param dontThrow      If set to \c true, the bootstrap doesn't throw and returns a <em>fall back</em>
                              result
        \param maxAttempts    Number of attempts on each iteration. A number greater than implies retries.
        \param maxFactor      Factor for max value retry on each iteration if there is a failure.
        \param minFactor      Factor for min value retry on each iteration if there is a failure.
        \param dontThrowSteps If \p dontThrow is \c true, this gives the number of steps to use when searching
                              for a fallback curve pillar value that gives the minimum bootstrap helper error.
    */
    IterativeBootstrap(QuantLib::Real accuracy = QuantLib::Null<QuantLib::Real>(),
                       QuantLib::Real globalAccuracy = QuantLib::Null<QuantLib::Real>(), bool dontThrow = false,
                       QuantLib::Size maxAttempts = 1, QuantLib::Real maxFactor = 2.0, QuantLib::Real minFactor = 2.0,
                       QuantLib::Size dontThrowSteps = 10);

    void setup(Curve* ts);
    void calculate() const;

private:
    void initialize() const;
    Curve* ts_;
    QuantLib::Size n_;
    QuantLib::Brent firstSolver_;
    QuantLib::FiniteDifferenceNewtonSafe solver_;
    mutable bool initialized_, validCurve_, loopRequired_;
    mutable QuantLib::Size firstAliveHelper_, alive_;
    mutable std::vector<QuantLib::Real> previousData_;
    mutable std::vector<QuantLib::ext::shared_ptr<QuantLib::BootstrapError<Curve> > > errors_;
    QuantLib::Real accuracy_;
    QuantLib::Real globalAccuracy_;
    bool dontThrow_;
    QuantLib::Size maxAttempts_;
    QuantLib::Real maxFactor_;
    QuantLib::Real minFactor_;
    QuantLib::Size dontThrowSteps_;
};

template <class Curve>
IterativeBootstrap<Curve>::IterativeBootstrap(QuantLib::Real accuracy, QuantLib::Real globalAccuracy, bool dontThrow,
                                              QuantLib::Size maxAttempts, QuantLib::Real maxFactor,
                                              QuantLib::Real minFactor, QuantLib::Size dontThrowSteps)
    : ts_(0), n_(0), initialized_(false), validCurve_(false), loopRequired_(Interpolator::global),
      firstAliveHelper_(0), alive_(0), accuracy_(accuracy), globalAccuracy_(globalAccuracy), dontThrow_(dontThrow),
      maxAttempts_(maxAttempts), maxFactor_(maxFactor), minFactor_(minFactor), dontThrowSteps_(dontThrowSteps) {}

template <class Curve> void IterativeBootstrap<Curve>::setup(Curve* ts) {
    ts_ = ts;
    n_ = ts_->instruments_.size();
    QL_REQUIRE(n_ > 0, "no bootstrap helpers given");
    for (QuantLib::Size j = 0; j < n_; ++j)
        ts_->registerWith(ts_->instruments_[j]);
}

template <class Curve> void IterativeBootstrap<Curve>::initialize() const {

    // ensure helpers are sorted
    std::sort(ts_->instruments_.begin(), ts_->instruments_.end(), QuantLib::detail::BootstrapHelperSorter());

    // skip expired helpers
    QuantLib::Date firstDate = Traits::initialDate(ts_);
    QL_REQUIRE(ts_->instruments_[n_ - 1]->pillarDate() > firstDate, "all instruments expired");
    firstAliveHelper_ = 0;
    while (ts_->instruments_[firstAliveHelper_]->pillarDate() <= firstDate)
        ++firstAliveHelper_;
    alive_ = n_ - firstAliveHelper_;
    QL_REQUIRE(alive_ >= Interpolator::requiredPoints - 1,
               "not enough alive instruments: " << alive_ << " provided, " << Interpolator::requiredPoints - 1
                                                << " required");

    // calculate dates and times, create errors_
    std::vector<QuantLib::Date>& dates = ts_->dates_;
    std::vector<QuantLib::Time>& times = ts_->times_;
    dates.resize(alive_ + 1);
    times.resize(alive_ + 1);
    errors_.resize(alive_ + 1);
    dates[0] = firstDate;
    times[0] = ts_->timeFromReference(dates[0]);

    QuantLib::Date latestRelevantDate, maxDate = firstDate;

    // pillar counter: i
    // helper counter: j
    for (QuantLib::Size i = 1, j = firstAliveHelper_; j < n_; ++i, ++j) {

        const QuantLib::ext::shared_ptr<typename Traits::helper>& helper = ts_->instruments_[j];
        dates[i] = helper->pillarDate();
        times[i] = ts_->timeFromReference(dates[i]);

        // check for duplicated pillars
        QL_REQUIRE(dates[i - 1] != dates[i], "more than one instrument with pillar " << dates[i]);

        latestRelevantDate = helper->latestRelevantDate();
        // check that the helper is really extending the curve, i.e. that
        // pillar-sorted helpers are also sorted by latestRelevantDate
        QL_REQUIRE(latestRelevantDate > maxDate, QuantLib::io::ordinal(j + 1)
                                                     << " instrument (pillar: " << dates[i]
                                                     << ") has latestRelevantDate (" << latestRelevantDate
                                                     << ") before or equal to "
                                                        "previous instrument's latestRelevantDate ("
                                                     << maxDate << ")");
        maxDate = latestRelevantDate;

        // when a pillar date is different from the last relevant date the
        // convergence loop is required even if the Interpolator is local
        if (dates[i] != latestRelevantDate)
            loopRequired_ = true;

        errors_[i] = QuantLib::ext::make_shared<QuantLib::BootstrapError<Curve> >(ts_, helper, i);
    }
    ts_->maxDate_ = maxDate;

    // set initial guess only if the current curve cannot be used as guess
    if (!validCurve_ || ts_->data_.size() != alive_ + 1) {
        // ts_->data_[0] is the only relevant item,
        // but reasonable numbers might be needed for the whole data vector
        // because, e.g., of interpolation's early checks
        ts_->data_ = std::vector<QuantLib::Real>(alive_ + 1, Traits::initialValue(ts_));
        previousData_.resize(alive_ + 1);
    }
    initialized_ = true;
}

template <class Curve> void IterativeBootstrap<Curve>::calculate() const {

    // we might have to call initialize even if the curve is initialized
    // and not moving, just because helpers might be date relative and change
    // with evaluation date change.
    // anyway it makes little sense to use date relative helpers with a
    // non-moving curve if the evaluation date changes
    if (!initialized_ || ts_->moving_)
        initialize();

    // setup helpers
    for (QuantLib::Size j = firstAliveHelper_; j < n_; ++j) {
        const QuantLib::ext::shared_ptr<typename Traits::helper>& helper = ts_->instruments_[j];

        // check for valid quote
        QL_REQUIRE(helper->quote()->isValid(), QuantLib::io::ordinal(j + 1)
                                                   << " instrument (maturity: " << helper->maturityDate()
                                                   << ", pillar: " << helper->pillarDate() << ") has an invalid quote");

        // don't try this at home!
        // This call creates helpers, and removes "const".
        // There is a significant interaction with observability.
        helper->setTermStructure(const_cast<Curve*>(ts_));
    }

    const std::vector<QuantLib::Time>& times = ts_->times_;
    const std::vector<QuantLib::Real>& data = ts_->data_;
    QuantLib::Real accuracy = accuracy_ != QuantLib::Null<QuantLib::Real>() ? accuracy_ : ts_->accuracy_;
    QuantLib::Real globalAccuracy = globalAccuracy_ == QuantLib::Null<QuantLib::Real>() ? accuracy : globalAccuracy_;

    QuantLib::Size maxIterations = Traits::maxIterations() - 1;

    // there might be a valid curve state to use as guess
    bool validData = validCurve_;

    for (QuantLib::Size iteration = 0;; ++iteration) {
        previousData_ = ts_->data_;

        std::vector<QuantLib::Real> minValues(alive_, QuantLib::Null<QuantLib::Real>());
        std::vector<QuantLib::Real> maxValues(alive_, QuantLib::Null<QuantLib::Real>());
        std::vector<QuantLib::Size> attempts(alive_, 1);

        for (QuantLib::Size i = 1; i <= alive_; ++i) {

            // bracket root and calculate guess
            if (minValues[i - 1] == QuantLib::Null<QuantLib::Real>()) {
                minValues[i - 1] = Traits::minValueAfter(i, ts_, validData, firstAliveHelper_);
            } else {
                minValues[i - 1] =
                    minValues[i - 1] < 0.0 ? minFactor_ * minValues[i - 1] : minValues[i - 1] / minFactor_;
            }
            if (maxValues[i - 1] == QuantLib::Null<QuantLib::Real>()) {
                maxValues[i - 1] = Traits::maxValueAfter(i, ts_, validData, firstAliveHelper_);
            } else {
                maxValues[i - 1] =
                    maxValues[i - 1] > 0.0 ? maxFactor_ * maxValues[i - 1] : maxValues[i - 1] / maxFactor_;
            }
            QuantLib::Real guess = Traits::guess(i, ts_, validData, firstAliveHelper_);

            // adjust guess if needed
            if (guess >= maxValues[i - 1])
                guess = maxValues[i - 1] - (maxValues[i - 1] - minValues[i - 1]) / 5.0;
            else if (guess <= minValues[i - 1])
                guess = minValues[i - 1] + (maxValues[i - 1] - minValues[i - 1]) / 5.0;

            // extend interpolation if needed
            if (!validData) {
                try { // extend interpolation a point at a time
                      // including the pillar to be bootstrapped
                    ts_->interpolation_ =
                        ts_->interpolator_.interpolate(times.begin(), times.begin() + i + 1, data.begin());
                } catch (...) {
                    if (!Interpolator::global)
                        throw; // no chance to fix it in a later iteration

                    // otherwise use Linear while the target
                    // interpolation is not usable yet
                    ts_->interpolation_ =
                        QuantLib::Linear().interpolate(times.begin(), times.begin() + i + 1, data.begin());
                }
                ts_->interpolation_.update();
            }

            try {
                if (validData)
                    solver_.solve(*errors_[i], accuracy, guess, minValues[i - 1], maxValues[i - 1]);
                else
                    firstSolver_.solve(*errors_[i], accuracy, guess, minValues[i - 1], maxValues[i - 1]);
            } catch (std::exception& e) {
                if (validCurve_) {
                    // the previous curve state might have been a
                    // bad guess, so we retry without using it.
                    // This would be tricky to do here (we're
                    // inside multiple nested for loops, we need
                    // to re-initialize...), so we invalidate the
                    // curve, make a recursive call and then exit.
                    validCurve_ = initialized_ = false;
                    calculate();
                    return;
                }

                // If we have more attempts left on this iteration, try again. Note that the max and min
                // bounds will be widened on the retry.
                if (attempts[i - 1] < maxAttempts_) {
                    attempts[i - 1]++;
                    i--;
                    continue;
                }

                if (dontThrow_) {
                    // Use the fallback value
                    ts_->data_[i] =
                        detail::dontThrowFallback(*errors_[i], minValues[i - 1], maxValues[i - 1], dontThrowSteps_);

                    // Remember to update the interpolation. If we don't and we are on the last "i", we will still
                    // have the last attempted value in the solver being used in ts_->interpolation_.
                    ts_->interpolation_.update();
                } else {
                    QL_FAIL(QuantLib::io::ordinal(iteration + 1)
                            << " iteration: failed "
                               "at "
                            << QuantLib::io::ordinal(i)
                            << " alive instrument, "
                               "pillar "
                            << errors_[i]->helper()->pillarDate() << ", maturity "
                            << errors_[i]->helper()->maturityDate() << ", reference date " << ts_->dates_[0] << ": "
                            << e.what());
                }
            }
        }

        if (!loopRequired_)
            break;

        // exit condition
        QuantLib::Real change = std::fabs(data[1] - previousData_[1]);
        for (QuantLib::Size i = 2; i <= alive_; ++i)
            change = std::max(change, std::fabs(data[i] - previousData_[i]));

        if (change <= globalAccuracy || change <= accuracy)
            break;

        // If we hit the max number of iterations and dontThrow is true, just use what we have
        if (iteration == maxIterations) {
            if (dontThrow_) {
                break;
            } else {
                QL_FAIL("convergence not reached after " << iteration << " iterations; last improvement " << change
                                                         << ", required accuracy "
                                                         << std::max(globalAccuracy, accuracy));
            }
        }

        validData = true;
    }

    validCurve_ = true;
}

} // namespace QuantExt

#endif
