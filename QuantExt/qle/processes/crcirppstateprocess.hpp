/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file crcirppstateprocess.hpp
    \brief CIR++ model state process
    \ingroup processes
*/

#ifndef quantext_crcirpp_stateprocess_hpp
#define quantext_crcirpp_stateprocess_hpp

#include <ql/stochasticprocess.hpp>

namespace QuantExt {
using namespace QuantLib;

class CrCirpp;

//! CIR++ Model State Process
/*! \ingroup processes
 */
class CrCirppStateProcess : public StochasticProcess {
public:
    enum Discretization { BrigoAlfonsi };

    CrCirppStateProcess(
        CrCirpp* const model,
        CrCirppStateProcess::Discretization disc = CrCirppStateProcess::Discretization::BrigoAlfonsi);

    /*! StochasticProcess interface */
    Size size() const;
    Disposable<Array> initialValues() const;
    Disposable<Array> drift(Time t, const Array& x) const;
    Disposable<Matrix> diffusion(Time t, const Array& x) const;
    Disposable<Array> evolve(Time t0, const Array& x0, Time dt, const Array& dw) const;

    // inspector
    const CrCirpp* model() const { return model_; }
    const CrCirppStateProcess::Discretization discretization() { return discretization_; }

protected:
    const CrCirpp* model_;
    const CrCirppStateProcess::Discretization discretization_;
};
} // namespace QuantExt

#endif
