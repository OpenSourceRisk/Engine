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

/*! \file ptdhestonprocess.hpp
    \brief piecewise time-dependent Heston process
    \ingroup processes
*/

#ifndef quantext_ptdhestonprocess_hpp
#define quantext_ptdhestonprocess_hpp

#include <ql/models/equity/piecewisetimedependenthestonmodel.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/quotes/simplequote.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Piecewise Time Dependent Heston process
/*! See ql/processes/hestonprocess.hpp (we base this version here on
    that class in QL 1.9.1). We allow for time dependent
    parameters. An important assumption that we make is that the
    parameters are constant on each discretisation interval, which
    greatly simplifies the implementation. This means the simulation
    time grid must contain the step times of the parameters.
*/
class PiecewiseTimeDependentHestonProcess : public StochasticProcess {
public:
    PiecewiseTimeDependentHestonProcess(const boost::shared_ptr<PiecewiseTimeDependentHestonModel>& model,
                     HestonProcess::Discretization d = HestonProcess::QuadraticExponentialMartingale);

    Size size() const;
    Size factors() const;

    Array initialValues() const;
    Array drift(Time t, const Array& x) const;
    Matrix diffusion(Time t, const Array& x) const;
    Array apply(const Array& x0, const Array& dx) const;
    Array evolve(Time t0, const Array& x0, Time dt, const Array& dw) const;

    boost::shared_ptr<PiecewiseTimeDependentHestonModel> model() const { return model_; }

    Time time(const Date&) const;

    HestonProcess::Discretization discretization() { return discretization_; }

private:
    boost::shared_ptr<HestonProcess> makeHestonProcess(const Real t, const Real s0, const Real v0) const;

    const boost::shared_ptr<PiecewiseTimeDependentHestonModel> model_;
    HestonProcess::Discretization discretization_;
    const Real smalldt_;
    const boost::shared_ptr<HestonProcess> process0_;
};  

} // namespace QuantExt


#endif
