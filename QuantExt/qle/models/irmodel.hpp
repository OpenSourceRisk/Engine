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

/*! \file irmodel.hpp
    \brief ir model base class
    \ingroup models
*/

#pragma once

#include <ql/math/array.hpp>
#include <ql/stochasticprocess.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>
#include <qle/models/parametrization.hpp>

namespace QuantExt {

class IrModel : public LinkableCalibratedModel {
public:
    enum class Measure { LGM, BA };

    /*! measure under which the model is operated */
    virtual Measure measure() const = 0;

    /*! parametrization (as base class) */
    virtual const QuantLib::ext::shared_ptr<Parametrization> parametrizationBase() const = 0;

    /*! yield term structure to which the IrModel is (initially) calibrated */
    virtual Handle<YieldTermStructure> termStructure() const = 0;

    /*! dimension of model state, excluding auxilliary states */
    virtual Size n() const = 0;

    /*! number of Brownians to evolve the state */
    virtual Size m() const = 0;

    /*! (effective) dimension of auxilliary state, typically to evaluate the numeraire in the BA-measure */
    virtual Size n_aux() const = 0;

    /*! (effective) number of Brownians required to evolve the auxilliary state, typcially for exact discretization
     * schemes */
    virtual Size m_aux() const = 0;

    /*! stochastic process, this has dimension n() + n_aux() and m() + m_aux() Brownian drivers */
    virtual QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const = 0;

    /*! discount bond depending on state (of dimension n()) */
    virtual QuantLib::Real discountBond(
        const QuantLib::Time t, const QuantLib::Time T, const QuantLib::Array& x,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve = Handle<YieldTermStructure>()) const = 0;

    /*! numeraire depending on state and aux state (of dimensions n(), n_aux() */
    virtual QuantLib::Real
    numeraire(const QuantLib::Time t, const QuantLib::Array& x,
              const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
              const QuantLib::Array& aux = Array()) const = 0;

    /*! short rate at t */
    virtual QuantLib::Real shortRate(
        const QuantLib::Time t, const QuantLib::Array& x,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve = Handle<YieldTermStructure>()) const = 0;
};

} // namespace QuantExt
