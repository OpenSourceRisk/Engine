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

/*! \file hwconstantparametrization.hpp
    \brief Hull White n factor parametrization with constant reversion and vol
    \ingroup models
*/

#pragma once

#include <qle/models/hwparametrization.hpp>

namespace QuantExt {

//! HW nF Parametrization with m driving Brownian motions and constant reversion, vol
/*! \ingroup models
 */
template <class TS> class HwConstantParametrization : public HwParametrization<TS> {
public:
    HwConstantParametrization(const QuantLib::Currency& currency, const QuantLib::Handle<TS>& termStructure,
                              const QuantLib::Matrix& sigma, const QuantLib::Array& kappa,
                              const std::string& name = std::string());

    QuantLib::Matrix sigma_x(const QuantLib::Time t) const override;
    QuantLib::Array kappa(const QuantLib::Time t) const override;
    QuantLib::Matrix y(const QuantLib::Time t) const override;
    QuantLib::Array g(const QuantLib::Time t, const QuantLib::Time T) const override;

    const QuantLib::ext::shared_ptr<Parameter> parameter(const Size) const override;

private:
    static constexpr QuantLib::Real zeroKappaCutoff_ = 1.0E-6;

    QuantLib::Size sigmaIndex(const QuantLib::Size i, const QuantLib::Size j) const;
    double sigmaComp(const QuantLib::Size i, const QuantLib::Size j) const;
    double kappaComp(const QuantLib::Size i) const;

    const QuantLib::ext::shared_ptr<PseudoParameter> sigma_, kappa_;
};

// implementation

template <class TS>
HwConstantParametrization<TS>::HwConstantParametrization(const QuantLib::Currency& currency,
                                                         const QuantLib::Handle<TS>& termStructure,
                                                         const QuantLib::Matrix& sigma, const QuantLib::Array& kappa,
                                                         const std::string& name)
    : HwParametrization<TS>(kappa.size(), sigma.rows(), currency, termStructure, name.empty() ? currency.code() : name),
      sigma_(QuantLib::ext::make_shared<PseudoParameter>(this->n_ * this->m_)),
      kappa_(QuantLib::ext::make_shared<PseudoParameter>(this->n_)) {

    QL_REQUIRE(sigma.columns() == kappa.size(), "HwConstantParametrization: sigma ("
                                                    << sigma.rows() << "x" << sigma.columns()
                                                    << ") not consistent with kappa (" << kappa.size() << ")");

    for (Size i = 0; i < sigma.rows(); ++i)
        for (Size j = 0; j < sigma.columns(); ++j)
            sigma_->setParam(sigmaIndex(i, j), sigma(i, j));

    for (Size i = 0; i < kappa.size(); ++i)
        kappa_->setParam(i, kappa[i]);
}

template <class TS>
QuantLib::Size HwConstantParametrization<TS>::sigmaIndex(const QuantLib::Size i, const QuantLib::Size j) const {
    return i * this->n_ + j;
}

template <class TS>
double HwConstantParametrization<TS>::sigmaComp(const QuantLib::Size i, const QuantLib::Size j) const {
    return sigma_->params()[sigmaIndex(i, j)];
}

template <class TS> double HwConstantParametrization<TS>::kappaComp(const QuantLib::Size i) const {
    return kappa_->params()[i];
}

template <class TS> QuantLib::Matrix HwConstantParametrization<TS>::sigma_x(const QuantLib::Time t) const {
    Matrix res(this->m_, this->n_);
    for (Size i = 0; i < this->m_; ++i)
        for (Size j = 0; j < this->n_; ++j)
            res(i, j) = sigmaComp(i, j);
    return res;
}

template <class TS> QuantLib::Array HwConstantParametrization<TS>::kappa(const QuantLib::Time t) const {
    Array res(this->n_);
    for (Size i = 0; i < this->n_; ++i)
        res[i] = kappaComp(i);
    return res;
}

template <class TS> QuantLib::Matrix HwConstantParametrization<TS>::y(const QuantLib::Time t) const {
    QuantLib::Matrix y(this->n_, this->n_, 0.0);
    for (Size i = 0; i < this->n_; ++i) {
        for (Size j = 0; j <= i; ++j) {
            QuantLib::Real tmp;
            if (std::abs(kappaComp(i) + kappaComp(j)) < zeroKappaCutoff_) {
                tmp = t;
            } else {
                tmp = (1.0 - std::exp(-(kappaComp(i) + kappaComp(j)) * t)) / (kappaComp(i) + kappaComp(j));
            }
            for (Size k = 0; k < this->m_; ++k) {
                y(i, j) += sigmaComp(k, i) * sigmaComp(k, j) * tmp;
            }
        }
    }
    for (Size i = 0; i < this->n_; ++i) {
        for (Size j = 0; j < i; ++j) {
            y(j, i) = y(i, j);
        }
    }
    return y;
}

template <class TS>
QuantLib::Array HwConstantParametrization<TS>::g(const QuantLib::Time t, const QuantLib::Time T) const {
    QL_REQUIRE(t <= T, "HwConstantParametrization::g(" << t << "," << T << ") invalid, expected t < T");
    QuantLib::Array g(this->n_, 0.0);
    for (Size i = 0; i < this->n_; ++i) {
        if (std::abs(kappaComp(i)) < zeroKappaCutoff_) {
            g[i] = T - t;
        } else {
            g[i] = (1.0 - std::exp(-kappaComp(i) * (T - t))) / kappaComp(i);
        }
    }
    return g;
}

template <class TS>
const QuantLib::ext::shared_ptr<Parameter> HwConstantParametrization<TS>::parameter(const Size i) const {
    if (i == 0)
        return sigma_;
    else
        return kappa_;
}

// typedef

typedef HwConstantParametrization<YieldTermStructure> IrHwConstantParametrization;

} // namespace QuantExt
