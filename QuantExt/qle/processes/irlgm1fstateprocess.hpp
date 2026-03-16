/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file irlgm1fstateprocess.hpp
    \brief ir LGM 1f model state process
    \ingroup processes
*/

#ifndef quantext_irlgm1f_stateprocess_hpp
#define quantext_irlgm1f_stateprocess_hpp

#include <ql/stochasticprocess.hpp>
#include <qle/models/irlgm1fparametrization.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Ir Lgm 1f State Process
/*! \ingroup processes
 */
class IrLgm1fStateProcess : public StochasticProcess1D {
public:
    IrLgm1fStateProcess(const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& parametrization);
    //! \name StochasticProcess interface
    //@{
    Real x0() const override;
    Real drift(Time t, Real x) const override;
    Real diffusion(Time t, Real x) const override;
    Real expectation(Time t0, Real x0, Time dt) const override;
    Real stdDeviation(Time t0, Real x0, Time dt) const override;
    Real variance(Time t0, Real x0, Time dt) const override;

    // enables and resets the cache, once enabled the simulated times must stay the stame
    void resetCache(const Size timeSteps) const;
    //@}
private:
    const QuantLib::ext::shared_ptr<IrLgm1fParametrization> p_;
    mutable bool cacheNotReady_d_ = true;
    mutable bool cacheNotReady_v_ = true;
    mutable Size timeStepsToCache_d_ = 0;
    mutable Size timeStepsToCache_v_ = 0;
    mutable Size timeStepCache_d_ = 0;
    mutable Size timeStepCache_v_ = 0;
    mutable std::vector<Real> cache_d_;
    mutable std::vector<Real> cache_v_;
};

// inline

inline Real IrLgm1fStateProcess::x0() const { return 0.0; }

inline Real IrLgm1fStateProcess::drift(Time, Real) const { return 0.0; }

inline Real IrLgm1fStateProcess::diffusion(Time t, Real) const {
    if (cacheNotReady_d_) {
        Real tmp = p_->alpha(t);
        if (timeStepsToCache_d_ > 0) {
            cache_d_.push_back(tmp);
            if (cache_d_.size() == timeStepsToCache_d_)
                cacheNotReady_d_ = false;
        }
        return tmp;
    } else {
        Real tmp = cache_d_[timeStepCache_d_++];
        if (timeStepCache_d_ == timeStepsToCache_d_)
            timeStepCache_d_ = 0;
        return tmp;
    }
}

inline Real IrLgm1fStateProcess::expectation(Time, Real x0, Time) const { return x0; }

inline Real IrLgm1fStateProcess::variance(Time t0, Real, Time dt) const {
    if (cacheNotReady_v_) {
        Real tmp = p_->zeta(t0 + dt) - p_->zeta(t0);
        if (timeStepsToCache_v_ > 0) {
            cache_v_.push_back(tmp);
            if (cache_v_.size() == timeStepsToCache_v_)
                cacheNotReady_v_ = false;
        }
        return tmp;
    } else {
        Real tmp = cache_v_[timeStepCache_v_++];
        if (timeStepCache_v_ == timeStepsToCache_d_)
            timeStepCache_v_ = 0;
        return tmp;
    }
}

inline Real IrLgm1fStateProcess::stdDeviation(Time t0, Real x0, Time dt) const {
    return std::sqrt(variance(t0, x0, dt));
}

inline void IrLgm1fStateProcess::resetCache(const Size timeSteps) const {
    cacheNotReady_d_ = cacheNotReady_v_ = true;
    timeStepsToCache_d_ = timeStepsToCache_v_ = timeSteps;
    cache_d_.clear();
    cache_v_.clear();
    p_->update();
}

} // namespace QuantExt

#endif
