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

/*! \file fxmodel.hpp
    \brief fx model base class
    \ingroup models
*/

#pragma once

#include <ql/stochasticprocess.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>
#include <qle/models/parametrization.hpp>

namespace QuantExt {

class FxModel : public LinkableCalibratedModel {
public:
    /*! parametrization (as base class) */
    virtual const QuantLib::ext::shared_ptr<Parametrization> parametrizationBase() const = 0;

    /*! today's fx rate on which the model is based */
    virtual Handle<Quote> fxSpotToday() const = 0;

    /*! dimension of model state, excluding auxilliary states */
    virtual Size n() const = 0;

    /*! number of Brownians to evolve the state */
    virtual Size m() const = 0;

    /*! perform an Euler step given short rates for the rates */
    virtual Array eulerStep(const Time t0, const Array& x0, const Time dt, const Array& dw, const Real r_dom,
                            const Real r_for) const = 0;
};

} // namespace QuantExt
