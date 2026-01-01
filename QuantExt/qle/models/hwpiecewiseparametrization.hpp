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

/*! \file hwpiecewiseparametrization.hpp
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
                               const QuantLib::Array& times, const std::vector<Matrix>& sigma,
                               const std::vector<QuantLib::Array>& kappa, const std::string& name = std::string());

    const QuantLib::ext::shared_ptr<Parameter> parameter(const Size) const override;
    const Array& parameterTimes(const Size) const override;

    QuantLib::Matrix sigma_x(const QuantLib::Time t) const override;
    QuantLib::Array kappa(const QuantLib::Time t) const override;
    QuantLib::Matrix y(const QuantLib::Time t) const override;
    QuantLib::Array g(const QuantLib::Time t, const QuantLib::Time T) const override;

protected:
    HwPiecewiseParametrization(const QuantLib::Size n, const QuantLib::Size m, const QuantLib::Currency& currency,
                               const QuantLib::Handle<TS>& termStructure, const QuantLib::Array& times,
                               const std::string& name = std::string());

    static constexpr QuantLib::Real zeroKappaCutoff_ = 1.0E-6;

    QuantLib::Size timeIndex(const QuantLib::Time t) const;
    QuantLib::Size sigmaIndex(const QuantLib::Size i, const QuantLib::Size j, const QuantLib::Size timeIndex) const;
    virtual QuantLib::Size kappaIndex(const QuantLib::Size i, const QuantLib::Size timeIndex) const;

    virtual double sigmaComp(const QuantLib::Size i, const QuantLib::Size j, const QuantLib::Size timeIndex) const;
    virtual double kappaComp(const QuantLib::Size i, const QuantLib::Size timeIndex) const;

    QuantLib::Matrix sigma_x_ind(const QuantLib::Size timeIndex) const;
    QuantLib::Array kappa_ind(const QuantLib::Size timeIndex) const;

    double y_part(double a, double b, double t, const QuantLib::Array& kappa, const QuantLib::Matrix& sigma,
                  QuantLib::Size i, QuantLib::Size j) const;
    double g_part(double t, double a, double b, double kappa) const;

    QuantLib::Array times_;
    QuantLib::ext::shared_ptr<PseudoParameter> sigma_, kappa_;
};

// implementation

template <class TS> QuantLib::Size HwPiecewiseParametrization<TS>::timeIndex(const QuantLib::Time t) const {
    return std::distance(times_.begin(), std::upper_bound(times_.begin(), times_.end(), t));
}

template <class TS>
QuantLib::Size HwPiecewiseParametrization<TS>::sigmaIndex(const QuantLib::Size i, const QuantLib::Size j,
                                                          const QuantLib::Size timeIndex) const {
    return i * (this->n_ * (times_.size() + 1)) + j * (times_.size() + 1) + timeIndex;
}

template <class TS>
QuantLib::Size HwPiecewiseParametrization<TS>::kappaIndex(const QuantLib::Size i,
                                                          const QuantLib::Size timeIndex) const {
    return (times_.size() + 1) * i + timeIndex;
}

template <class TS>
double HwPiecewiseParametrization<TS>::sigmaComp(const QuantLib::Size i, const QuantLib::Size j,
                                                 const QuantLib::Size timeIndex) const {
    return sigma_->params()[sigmaIndex(i, j, timeIndex)];
}

template <class TS>
double HwPiecewiseParametrization<TS>::kappaComp(const QuantLib::Size i, const QuantLib::Size timeIndex) const {
    return kappa_->params()[kappaIndex(i, timeIndex)];
}

template <class TS>
HwPiecewiseParametrization<TS>::HwPiecewiseParametrization(
    const QuantLib::Currency& currency, const QuantLib::Handle<TS>& termStructure, const QuantLib::Array& times,
    const std::vector<Matrix>& sigma, const std::vector<QuantLib::Array>& kappa, const std::string& name)
    : HwParametrization<TS>(kappa.front().size(), sigma.front().rows(), currency, termStructure,
                            name.empty() ? currency.code() : name),
      times_(times), sigma_(QuantLib::ext::make_shared<PseudoParameter>(this->n_ * this->m_ * (times.size() + 1))),
      kappa_(QuantLib::ext::make_shared<PseudoParameter>(this->n_ * (times.size() + 1))) {

    for (Size i = 1; i < times_.size(); ++i) {
        QL_REQUIRE(times_[i] > times_[i - 1], "HwPiecewiseParametrization: times array must be strictly increasing");
    }

    QL_REQUIRE(sigma.size() == times_.size() + 1, "HwPiecewiseParametrization: sigma vector ("
                                                      << sigma.size() << ") not consistent with times ("
                                                      << times_.size() << ")");

    QL_REQUIRE(kappa.size() == times_.size() + 1, "HwPiecewiseParametrization: kappa vector ("
                                                      << kappa.size() << ") not consistent with times ("
                                                      << times_.size() << ")");

    QL_REQUIRE(sigma.front().columns() == kappa.front().size(),
               "HwPiecewiseParametrization: sigma (" << sigma.front().rows() << "x" << sigma.front().columns()
                                                     << ") not consistent with kappa (" << kappa.front().size() << ")");

    for (Size i = 0; i < times_.size(); ++i) {

        QL_REQUIRE(sigma[0].rows() == sigma[i + 1].rows(), "HwPiecewiseParametrization: sigm rows at time index "
                                                               << (i + 1) << " (" << sigma[i + 1].rows()
                                                               << ") inconsistent to time index 0 (" << sigma[0].rows()
                                                               << ")");
        QL_REQUIRE(sigma[0].columns() == sigma[i + 1].columns(), "HwPiecewiseParametrization: sigm rows at time index "
                                                                     << (i + 1) << " (" << sigma[i + 1].rows()
                                                                     << ") inconsistent to time index 0 ("
                                                                     << sigma[0].rows() << ")");
    }

    for (Size i = 0; i < sigma.front().rows(); ++i)
        for (Size j = 0; j < sigma.front().columns(); ++j)
            for (Size k = 0; k < times_.size() + 1; ++k)
                sigma_->setParam(sigmaIndex(i, j, k), sigma[k](i, j));

    for (Size i = 0; i < kappa.front().size(); ++i)
        for (Size k = 0; k < times_.size() + 1; ++k)
            kappa_->setParam(kappaIndex(i, k), kappa[k][i]);
}

template <class TS>
HwPiecewiseParametrization<TS>::HwPiecewiseParametrization(const QuantLib::Size n, const QuantLib::Size m,
                                                           const QuantLib::Currency& currency,
                                                           const QuantLib::Handle<TS>& termStructure,
                                                           const QuantLib::Array& times, const std::string& name)
    : HwParametrization<TS>(n, m, currency, termStructure, name.empty() ? currency.code() : name), times_(times) {
    for (Size i = 1; i < times_.size(); ++i) {
        QL_REQUIRE(times_[i] > times_[i - 1], "HwPiecewiseParametrization: times array must be strictly increasing");
    }
}

template <class TS> QuantLib::Matrix HwPiecewiseParametrization<TS>::sigma_x_ind(const QuantLib::Size k) const {
    Matrix res(this->m_, this->n_);
    for (Size i = 0; i < this->m_; ++i)
        for (Size j = 0; j < this->n_; ++j)
            res(i, j) = sigmaComp(i, j, k);
    return res;
}

template <class TS> QuantLib::Array HwPiecewiseParametrization<TS>::kappa_ind(const QuantLib::Size k) const {
    Array res(this->n_);
    for (Size i = 0; i < this->n_; ++i)
        res[i] = kappaComp(i, k);
    return res;
}

template <class TS> QuantLib::Matrix HwPiecewiseParametrization<TS>::sigma_x(const QuantLib::Time t) const {
    return sigma_x_ind(timeIndex(t));
}

template <class TS> QuantLib::Array HwPiecewiseParametrization<TS>::kappa(const QuantLib::Time t) const {
    return kappa_ind(timeIndex(t));
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
    Size k0 = timeIndex(t);

    for (Size k = 0; k < k0; ++k) {

        auto sigma = sigma_x_ind(k);
        auto kappa = kappa_ind(k);

        for (Size i = 0; i < this->n_; ++i) {
            for (Size j = 0; j <= i; ++j) {
                y(i, j) += y_part(k == 0 ? 0.0 : times_[k - 1], times_[k], t, kappa, sigma, i, j);
            }
        }
    }

    auto sigma = sigma_x_ind(k0);
    auto kappa = kappa_ind(k0);

    for (Size i = 0; i < this->n_; ++i) {
        for (Size j = 0; j <= i; ++j) {
            y(i, j) += y_part(k0 == 0 ? 0.0 : times_[k0 - 1], t, t, kappa, sigma, i, j);
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
    Size k0 = timeIndex(t);
    Size k1 = timeIndex(T);

    for (Size k = k0; k < k1; ++k) {
        auto kappa = kappa_ind(k);
        for (Size i = 0; i < this->n_; ++i) {
            g[i] += g_part(t, k == k0 ? t : times_[k - 1], times_[k], kappa[i]);
        }
    }

    auto kappa = kappa_ind(k1);
    for (Size i = 0; i < this->n_; ++i) {
        g[i] += g_part(t, k1 == k0 ? t : times_[k1 - 1], T, kappa[i]);
    }

    return g;
}

template <class TS>
const QuantLib::ext::shared_ptr<Parameter> HwPiecewiseParametrization<TS>::parameter(const Size i) const {
    if (i == 0)
        return sigma_;
    else
        return kappa_;
}

template <class TS> const Array& HwPiecewiseParametrization<TS>::parameterTimes(const Size) const { return times_; }

// typedef

typedef HwPiecewiseParametrization<YieldTermStructure> IrHwPiecewiseParametrization;

} // namespace QuantExt
