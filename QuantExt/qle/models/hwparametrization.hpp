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

/*! \file hwparametrization.hpp
    \brief Hull White n factor parametrization
    \ingroup models
*/

#pragma once

#include <qle/models/parametrization.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>

namespace QuantExt {

//! HW nF Parametrization with m driving Brownian motions
/*! \ingroup models
 */
template <class TS> class HwParametrization : public Parametrization {
public:
    HwParametrization(const QuantLib::Size n, const QuantLib::Size m, const QuantLib::Currency& currency,
                      const QuantLib::Handle<TS>& termStructure,
                      const std::string& name = std::string());

    /*! sigma_x, 1.12, this is a m x n matrix */
    virtual QuantLib::Matrix sigma_x(const QuantLib::Time t) const = 0;
    /* kappa(t), this is an n-array representing the diag matrix in 1.8 */
    virtual QuantLib::Array kappa(const QuantLib::Time t) const = 0;

    /*! y(t), 1.19, this is an n x n matrix */
    virtual QuantLib::Matrix y(const QuantLib::Time t) const;
    /*! g(t,T), 1.21, this is an n-array */
    virtual QuantLib::Array g(const QuantLib::Time t, const QuantLib::Time T) const;

    /*! the underlying term structure */
    const QuantLib::Handle<TS> termStructure() const { return termStructure_; }

    /*! we have m x n (from g) + n (from kappa) parameters */
    QuantLib::Size numberOfParameters() const override { return m_ * n_ + n_; }

    QuantLib::Size n() const { return n_; }
    QuantLib::Size m() const { return m_; }

    void update() const override;

protected:
    QuantLib::Size n_, m_;

private:
    const QuantLib::Handle<TS> termStructure_;
};

// implementation

template <class TS>
HwParametrization<TS>::HwParametrization(const QuantLib::Size n, const QuantLib::Size m,
                                         const QuantLib::Currency& currency, const QuantLib::Handle<TS>& termStructure,
                                         const std::string& name)
    : Parametrization(currency, name.empty() ? currency.code() : name), n_(n), m_(m), termStructure_(termStructure) {}

template <class TS> QuantLib::Matrix HwParametrization<TS>::y(const Time t) const {
    QL_FAIL("HwParametrization::y(t) not implemented");
}

template <class TS> QuantLib::Array HwParametrization<TS>::g(const QuantLib::Time t, const QuantLib::Time T) const {
    QL_FAIL("HwParametrization::g(t, T) not implemented");
}

template <class TS> void HwParametrization<TS>::update() const { Parametrization::update(); }

// typedef

typedef HwParametrization<YieldTermStructure> IrHwParametrization;

} // namespace QuantExt
