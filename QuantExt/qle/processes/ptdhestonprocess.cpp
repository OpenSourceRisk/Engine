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
#include <ql/shared_ptr.hpp>
#include <ql/processes/eulerdiscretization.hpp>
#include <qle/processes/ptdhestonprocess.hpp>
#include <qle/termstructures/impliedtermstructure.hpp>
#include <boost/make_shared.hpp>

namespace QuantExt {

PiecewiseTimeDependentHestonProcess::PiecewiseTimeDependentHestonProcess(const QuantLib::ext::shared_ptr<PiecewiseTimeDependentHestonModel>& model,
                                   HestonProcess::Discretization d)
    : StochasticProcess(QuantLib::ext::shared_ptr<StochasticProcess::discretization>(new EulerDiscretization)),
      model_(model), discretization_(d),
      smalldt_(1.0E-8), process0_(makeHestonProcess(0.0, model_->s0(), model_->v0())) {

    registerWith(model_);
}

QuantLib::ext::shared_ptr<HestonProcess>
PiecewiseTimeDependentHestonProcess::makeHestonProcess(const Real t, const Real s0, const Real v0) const {
    Real tmp = t + smalldt_;
    return QuantLib::ext::make_shared<HestonProcess>(
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<ImpliedTermStructure>(model_->riskFreeRate(), t)),
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<ImpliedTermStructure>(model_->dividendYield(), t)),
        Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(s0)), v0, model_->kappa(tmp), model_->theta(tmp),
        model_->sigma(tmp), model_->rho(tmp), discretization_);
}

Size PiecewiseTimeDependentHestonProcess::size() const { return process0_->size(); }

Size PiecewiseTimeDependentHestonProcess::factors() const { return process0_->factors(); }

Array PiecewiseTimeDependentHestonProcess::initialValues() const { return process0_->initialValues(); }

Array PiecewiseTimeDependentHestonProcess::drift(Time t, const Array& x) const {
    return makeHestonProcess(t, x[0], x[1])->drift(0.0, x);
}

Matrix PiecewiseTimeDependentHestonProcess::diffusion(Time t, const Array& x) const {
    return makeHestonProcess(t, x[0], x[1])->diffusion(0.0, x);
}

Array PiecewiseTimeDependentHestonProcess::apply(const Array& x0, const Array& dx) const { return process0_->apply(x0, dx); }

Array PiecewiseTimeDependentHestonProcess::evolve(Time t0, const Array& x0, Time dt, const Array& dw) const {
    return makeHestonProcess(t0, x0[0], x0[1])->evolve(0.0, x0, dt, dw);
}

Time PiecewiseTimeDependentHestonProcess::time(const Date& d) const { return process0_->time(d); }

} // namespace QuantExt
