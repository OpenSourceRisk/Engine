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

#include <qle/models/hwpiecewiseparametrization.hpp>

namespace QuantExt {

/*! HW nF Parametrization with m driving Brownian motions and piecewise reversion, vol

    sigma_x is parametrized by a single piecewise volatility

    sigma_0(t)

    It is this sigma_0(t) that is stored as the sigma_ parameter in this parametrization. From sigma_0
    we deduce the volatility of m principial components via sigma ratios

    sigma_i(t) = sigma_0(t) * sigmaRatio_i

    and (cf. eq 2.17 in the hw model documentation)

    sigma_x(t) = v sigma(t)

    with v = pca loadings. This way we are able to reproduce implied market volatilities
    of an option strip (e.g. coterminal strip) as in the 1F case and at the same time
    reproduce historical curve movements (via kappa) from a pca on historical data retaining
    the relative historical volatilities of the principal components.

    See section 2.3 "statistical calibration with risk neutral volatility" in the hw model
    documentation for more details. */

/*! \ingroup models
 */

template <class TS> class HwPiecewiseStatisticalParametrization : public HwPiecewiseParametrization<TS> {
public:
    HwPiecewiseStatisticalParametrization(const QuantLib::Currency& currency, const QuantLib::Handle<TS>& termStructure,
                                          const QuantLib::Array& times, const QuantLib::Array& sigma0,
                                          const QuantLib::Array& kappa, const QuantLib::Array& sigmaRatios,
                                          const std::vector<QuantLib::Array>& loadings,
                                          const std::string& name = std::string());

private:

    QuantLib::Size sigma0Index(const QuantLib::Size timeIndex) const;
    QuantLib::Size kappaTimeIndepIndex(const QuantLib::Size i) const;

    double sigmaComp(const QuantLib::Size i, const QuantLib::Size j, const QuantLib::Size timeIndex) const override;
    double kappaComp(const QuantLib::Size i, const QuantLib::Size timeIndex) const override;

    QuantLib::Array sigmaRatios_;
    std::vector<QuantLib::Array> loadings_;

    std::vector<Size> loadingIndex_;
};

// implementation

template <class TS>
QuantLib::Size HwPiecewiseStatisticalParametrization<TS>::sigma0Index(const QuantLib::Size timeIndex) const {
    return timeIndex;
}

template <class TS>
QuantLib::Size HwPiecewiseStatisticalParametrization<TS>::kappaTimeIndepIndex(const QuantLib::Size i) const {
    return i;
}

template <class TS>
double HwPiecewiseStatisticalParametrization<TS>::sigmaComp(const QuantLib::Size i, const QuantLib::Size j,
                                                            const QuantLib::Size timeIndex) const {
    if (j >= loadingIndex_[i] && j < loadingIndex_[i + 1]) {
        return loadings_[i][j - loadingIndex_[i]] * this->sigma_->params()[sigma0Index(timeIndex)] * sigmaRatios_[i];
    } else {
        return 0;
    }
}

template <class TS>
double HwPiecewiseStatisticalParametrization<TS>::kappaComp(const QuantLib::Size i,
                                                            const QuantLib::Size timeIndex) const {
    return this->kappa_->params()[this->kappaTimeIndepIndex(i)];
}

template <class TS>
HwPiecewiseStatisticalParametrization<TS>::HwPiecewiseStatisticalParametrization(
    const QuantLib::Currency& currency, const QuantLib::Handle<TS>& termStructure, const QuantLib::Array& times,
    const Array& sigma0, const QuantLib::Array& kappa, const QuantLib::Array& sigmaRatios,
    const std::vector<QuantLib::Array>& loadings, const std::string& name)
    : HwPiecewiseParametrization<TS>(kappa.size(), sigmaRatios.size(), currency, termStructure, times,
                                     name.empty() ? currency.code() : name),
      sigmaRatios_(sigmaRatios), loadings_(loadings) {

    this->sigma_ = QuantLib::ext::make_shared<PseudoParameter>(this->times_.size() + 1);
    this->kappa_ = QuantLib::ext::make_shared<PseudoParameter>(this->n_);

    for (Size i = 1; i < this->times_.size(); ++i) {
        QL_REQUIRE(this->times_[i] > this->times_[i - 1],
                   "HwPiecewiseParametrization: times array must be strictly increasing");
    }

    Size totalNumberLoadings = std::accumulate(loadings_.begin(), loadings_.end(), 0,
                                               [](double res, const Array& x) { return x.size() + res; });

    QL_REQUIRE(totalNumberLoadings == kappa.size(), "HwPiecewiseStatisticalParametrization: total number of loadings ("
                                                        << totalNumberLoadings << ") inconsistent to kappa ("
                                                        << kappa.size() << ")");

    QL_REQUIRE(loadings_.size() == sigmaRatios_.size(), "HwPiecewiseStatisticalParametrization: loading rows ("
                                                            << loadings_.size() << ") inconsistent to sigmaRatios ("
                                                            << sigmaRatios_.size() << ")");

    Size tmp = 0;
    for (auto const& l : loadings_) {
        loadingIndex_.push_back(tmp);
        tmp += l.size();
    }
    loadingIndex_.push_back(tmp);

    for (Size k = 0; k < this->times_.size() + 1; ++k) {
        this->sigma_->setParam(sigma0Index(k), sigma0[k]);
    }

    for (Size i = 0; i < this->n_; ++i)
        this->kappa_->setParam(kappaTimeIndepIndex(i), kappa[i]);
}

// typedef

typedef HwPiecewiseStatisticalParametrization<YieldTermStructure> IrHwPiecewiseStatisticalParametrization;

} // namespace QuantExt
