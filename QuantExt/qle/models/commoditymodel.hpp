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

/*! \file commoditymodel.hpp
    \brief Commodity model base class
    \ingroup models
*/

#pragma once

#include <ql/math/array.hpp>
#include <ql/stochasticprocess.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>
#include <qle/models/parametrization.hpp>
#include <qle/termstructures/pricetermstructure.hpp>

namespace QuantExt {

class CommodityModel : public LinkableCalibratedModel {
public:
    /*! parametrization (as base class) */
    virtual const QuantLib::ext::shared_ptr<Parametrization> parametrizationBase() const = 0;

    /*! price term structure to which the model is (initially) calibrated */
    virtual Handle<PriceTermStructure> termStructure() const = 0;

    /*! currency of the commodity */
    virtual const Currency& currency() const = 0;

    /*! dimension of model state */
    virtual Size n() const = 0;

    /*! number of Brownians to evolve the state */
    virtual Size m() const = 0;

    /*! stochastic process, this has dimension n() and m() Brownian drivers */
    virtual QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const = 0;

    /*! stochastic forward price curve F(t,T) at future time t depending on state (of dimension n()) */
    virtual QuantLib::Real forwardPrice(
        const QuantLib::Time t, const QuantLib::Time T, const QuantLib::Array& x,
        const QuantLib::Handle<QuantExt::PriceTermStructure>& priceCurve
        = QuantLib::Handle<QuantExt::PriceTermStructure>()) const = 0;
};

} // namespace QuantExt
