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

/*! \file hwcpiecewisearametrization.hpp
    \brief Hull White n factor parametrization with piecewise constant reversion and vol
    \ingroup models
*/

#pragma once

#include <qle/models/hwparametrization.hpp>

namespace QuantExt {

//! HW nF Parametrization with m driving Brownian motions and piecewise reversion, vol
/*! \ingroup models
 */
template <class TS> class HwPiecewiseParametrization : public HwParametrization<TS> {
public:
    HwPiecewiseParametrization(const QuantLib::Currency& currency, const QuantLib::Handle<TS>& termStructure,
                               const QuantLib::Array times, std::vector<Matrix> sigma, std::vector<QuantLib::Array> kappa,
                               const std::string& name = std::string());

    QuantLib::Matrix sigma_x(const QuantLib::Time t) const override;
    QuantLib::Array kappa(const QuantLib::Time t) const override;
    QuantLib::Matrix y(const QuantLib::Time t) const override;
    QuantLib::Array g(const QuantLib::Time t, const QuantLib::Time T) const override;

private:
    static constexpr QuantLib::Real zeroKappaCutoff_ = 1.0E-6;

    QuantLib::Size get_index(const QuantLib::Time t) const;

    QuantLib::Array times_;
    std::vector<Matrix> sigma_;
    std::vector<QuantLib::Array> kappa_;
};

// implementation

template <class TS>
HwPiecewiseParametrization<TS>::HwPiecewiseParametrization(const QuantLib::Currency& currency,
                                                           const QuantLib::Handle<TS>& termStructure,
                                                           const QuantLib::Array times, std::vector<Matrix> sigma,
                                                           std::vector<QuantLib::Array> kappa, const std::string& name)
    : HwParametrization<TS>(kappa.front().size(), sigma.front().rows(), currency, termStructure,
                            name.empty() ? currency.code() : name),
      times_(std::move(times)), sigma_(std::move(sigma)), kappa_(std::move(kappa)) {

    QL_REQUIRE(sigma_.front().columns() == kappa_.front().size(), "HwPiecewiseParametrization: sigma ("
                                                      << sigma_.front().rows() << "x" << sigma_.front().columns()
                                                      << ") not consistent with kappa (" << kappa_.front().size() << ")");

    // Ensure times is increasing
    for (Size i = 1; i < times_.size(); ++i) {
        QL_REQUIRE(times_[i] > times_[i - 1], "HwPiecewiseParametrization: times array must be strictly increasing");
    }
}

template <class TS> QuantLib::Size HwPiecewiseParametrization<TS>::get_index(const QuantLib::Time t) const {
    // If times_ is empty revert to constant parametrization
    if (times_.size() == 0)
        return 0;

    // Check if given time belongs to the right-bound
    if (t > times_.back())
        return times_.size();
    // Othrwise return the corresponding param for that time bucket
    return std::lower_bound(times_.begin(), times_.end(), t) - times_.begin();
}

template <class TS> QuantLib::Matrix HwPiecewiseParametrization<TS>::sigma_x(const QuantLib::Time t) const {
    QuantLib::Size i = get_index(t);
    if (i >= sigma_.size()) {
        i = sigma_.size() - 1;
    }
    return sigma_[i];
}

template <class TS> QuantLib::Array HwPiecewiseParametrization<TS>::kappa(const QuantLib::Time t) const {
    QuantLib::Size i = get_index(t);
    if (i >= kappa_.size()) {
        i = kappa_.size() - 1;
    }
    return kappa_[i];
}

template <class TS> QuantLib::Matrix HwPiecewiseParametrization<TS>::y(const QuantLib::Time t) const {
    QuantLib::Matrix y(this->n_, this->n_, 0.0);

    auto currSigma = sigma_x(t);
    auto currKappa = kappa(t);

    for (Size i = 0; i < this->n_; ++i) {
        for (Size j = 0; j <= i; ++j) {
            QuantLib::Real tmp;
            if (std::abs(currKappa[i] + currKappa[j]) < zeroKappaCutoff_) {
                tmp = t;
            } else {
                tmp = (1.0 - std::exp(-(currKappa[i] + currKappa[j]) * t)) / (currKappa[i] + currKappa[j]);
            }
            for (Size k = 0; k < this->m_; ++k) {
                y(i, j) += currSigma(k, i) * currSigma(k, j) * tmp;
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
QuantLib::Array HwPiecewiseParametrization<TS>::g(const QuantLib::Time t, const QuantLib::Time T) const {
    QL_REQUIRE(t <= T, "HwPiecewiseParametrization::g(" << t << "," << T << ") invalid, expected t < T");
    QuantLib::Array g(this->n_, 0.0);
    auto currKappa = kappa(t);
    for (Size i = 0; i < this->n_; ++i) {
        if (std::abs(currKappa[i]) < zeroKappaCutoff_) {
            g[i] = T - t;
        } else {
            g[i] = (1.0 - std::exp(-currKappa[i] * (T - t))) / currKappa[i];
        }
    }
    return g;
}

// typedef

typedef HwPiecewiseParametrization<YieldTermStructure> IrHwPiecewiseParametrization;

} // namespace QuantExt
