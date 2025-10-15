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
                               const QuantLib::Array times, std::vector<Matrix> sigma,
                               std::vector<QuantLib::Array> kappa, const std::string& name = std::string());

    QuantLib::Matrix sigma_x(const QuantLib::Time t) const override;
    QuantLib::Array kappa(const QuantLib::Time t) const override;
    QuantLib::Matrix y(const QuantLib::Time t) const override;
    QuantLib::Array g(const QuantLib::Time t, const QuantLib::Time T) const override;

private:
    static constexpr QuantLib::Real zeroKappaCutoff_ = 1.0E-6;

    QuantLib::Size get_index(const QuantLib::Time t) const;
    double y_part(double a, double b, double t, const QuantLib::Array& kappa, const QuantLib::Matrix& sigma,
                  QuantLib::Size i, QuantLib::Size j) const;
    double g_part(double t, double a, double b, double kappa) const;

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

    QL_REQUIRE(sigma_.front().columns() == kappa_.front().size(),
               "HwPiecewiseParametrization: sigma (" << sigma_.front().rows() << "x" << sigma_.front().columns()
                                                     << ") not consistent with kappa (" << kappa_.front().size()
                                                     << ")");

    QL_REQUIRE(sigma_.size() == times_.size() + 1, "HwPiecewiseParametrization: sigma vector ("
                                                       << sigma_.size() << ") not consistent with times ("
                                                       << times_.size() << ")");

    QL_REQUIRE(kappa_.size() == times_.size() + 1, "HwPiecewiseParametrization: kappa vector ("
                                                       << kappa_.size() << ") not consistent with times ("
                                                       << times_.size() << ")");

    // Ensure times is increasing
    for (Size i = 1; i < times_.size(); ++i) {
        QL_REQUIRE(times_[i] > times_[i - 1], "HwPiecewiseParametrization: times array must be strictly increasing");
    }
}

template <class TS> QuantLib::Size HwPiecewiseParametrization<TS>::get_index(const QuantLib::Time t) const {
    return std::distance(times_.begin(), std::upper_bound(times_.begin(), times_.end(), t));
}

template <class TS> QuantLib::Matrix HwPiecewiseParametrization<TS>::sigma_x(const QuantLib::Time t) const {
    return sigma_[get_index(t)];
}

template <class TS> QuantLib::Array HwPiecewiseParametrization<TS>::kappa(const QuantLib::Time t) const {
    return kappa_[get_index(t)];
}

template <class TS>
double HwPiecewiseParametrization<TS>::y_part(double a, double b, double t, const QuantLib::Array& kappa,
                                              const QuantLib::Matrix& sigma, Size i, Size j) const {
    double tmp;
    if (std::abs(kappa[i] + kappa[j]) < zeroKappaCutoff_) {
        tmp = b - a;
    } else {
        tmp = (std::exp(-(kappa[i] + kappa[j]) * (t - b)) - std::exp(-(kappa[i] + kappa[j]) * (t - a))) /
              (kappa[i] + kappa[j]);
    }

    double res = 0.0;
    for (Size k = 0; k < this->m_; ++k) {
        res += sigma(k, i) * sigma(k, j) * tmp;
    }

    return res;
}

template <class TS> double HwPiecewiseParametrization<TS>::g_part(double t, double a, double b, double kappa) const {
    double tmp;
    if (std::abs(kappa) < zeroKappaCutoff_) {
        tmp = b - a;
    } else {
        tmp = (std::exp(-kappa * (a - t)) - std::exp(-kappa * (b - t))) / kappa;
    }
    return tmp;
}

template <class TS> QuantLib::Matrix HwPiecewiseParametrization<TS>::y(const QuantLib::Time t) const {

    QuantLib::Matrix y(this->n_, this->n_, 0.0);
    Size k0 = get_index(t);

    for (Size i = 0; i < this->n_; ++i) {
        for (Size j = 0; j <= i; ++j) {

            for (Size k = 0; k < k0; ++k) {
                y(i, j) += y_part(k == 0 ? 0.0 : times_[k - 1], times_[k], t, kappa_[k], sigma_[k], i, j);
            }

            y(i, j) += y_part(k0 == 0 ? 0.0 : times_[k0 - 1], t, t, kappa_[k0], sigma_[k0], i, j);
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
    Size k0 = get_index(t);
    Size k1 = get_index(T);

    for (Size i = 0; i < this->n_; ++i) {

        for (Size k = k0; k < k1; ++k) {
            g[i] += g_part(t, k == k0 ? t : times_[k - 1], times_[k], kappa_[k][i]);
        }

        g[i] += g_part(t, k1 == k0 ? t : times_[k1 - 1], T, kappa_[k1][i]);
    }

    return g;
}

// typedef

typedef HwPiecewiseParametrization<YieldTermStructure> IrHwPiecewiseParametrization;

} // namespace QuantExt
