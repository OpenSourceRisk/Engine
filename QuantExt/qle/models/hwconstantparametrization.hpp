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
                              QuantLib::Matrix sigma, QuantLib::Array kappa, const std::string& name = std::string());

    QuantLib::Matrix sigma_x(const QuantLib::Time t) const override { return sigma_; }
    QuantLib::Array kappa(const QuantLib::Time t) const override { return kappa_; };
    QuantLib::Matrix y(const QuantLib::Time t) const override;
    QuantLib::Array g(const QuantLib::Time t, const QuantLib::Time T) const override;

private:
    static constexpr QuantLib::Real zeroKappaCutoff_ = 1.0E-6;
    QuantLib::Matrix sigma_;
    QuantLib::Array kappa_;
};

// implementation

template <class TS>
HwConstantParametrization<TS>::HwConstantParametrization(const QuantLib::Currency& currency,
                                                         const QuantLib::Handle<TS>& termStructure,
                                                         QuantLib::Matrix sigma, QuantLib::Array kappa,
                                                         const std::string& name)
    : HwParametrization<TS>(kappa.size(), sigma.rows(), currency, termStructure, name.empty() ? currency.code() : name),
      sigma_(std::move(sigma)), kappa_(std::move(kappa)) {
    QL_REQUIRE(sigma.columns() == kappa.size(), "HwConstantParametrization: sigma ("
                                                    << sigma.rows() << "x" << sigma.columns()
                                                    << ") not consistent with kappa (" << kappa.size() << ")");
}

template <class TS> QuantLib::Matrix HwConstantParametrization<TS>::y(const QuantLib::Time t) const {
    QuantLib::Matrix y(this->n_, this->n_, 0.0);
    for (Size i = 0; i < this->n_; ++i) {
        for (Size j = 0; j <= i; ++j) {
            QuantLib::Real tmp;
            if (std::abs(kappa_[i] + kappa_[j]) < zeroKappaCutoff_) {
                tmp = t;
            } else {
                tmp = (1.0 - std::exp(-(kappa_[i] + kappa_[j]) * t)) / (kappa_[i] + kappa_[j]);
            }
            for (Size k = 0; k < this->m_; ++k) {
                y(i, j) += sigma_x(t)(k, i) * sigma_x(t)(k, j) * tmp;
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
        if (std::abs(kappa_[i]) < zeroKappaCutoff_) {
            g[i] = T - t;
        } else {
            g[i] = (1.0 - std::exp(-kappa_[i] * (T - t))) / kappa_[i];
        }
    }
    return g;
}

// typedef

typedef HwConstantParametrization<YieldTermStructure> IrHwConstantParametrization;

} // namespace QuantExt
