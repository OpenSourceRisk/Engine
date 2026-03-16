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

/*! \file models/crlgmvectorised.hpp
    \brief vectorised crlgm model calculations
    \ingroup models
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/models/crlgm1fparametrization.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/irlgm1fparametrization.hpp>

namespace QuantExt {
using namespace QuantLib;
class CrLgmVectorised {
public:
    CrLgmVectorised(const QuantLib::ext::shared_ptr<CrossAssetModel>& model) : model_(model) {}

    std::pair<RandomVariable, RandomVariable> StStilde(const size_t i, const size_t ccy_idx, const Time t, const Time T,
                                                       const RandomVariable& z, const RandomVariable& y) const {
        QL_REQUIRE(t < T || QuantLib::close_enough(t, T), "crlgm1fS: t (" << t << ") <= T (" << T << ") required");
        cache_key k = {i, ccy_idx, t, T};
        boost::unordered_map<cache_key, std::pair<Real, Real>>::const_iterator it = cache_crlgm1fS_.find(k);
        Real V0, V_tilde;
        size_t n = z.size();
        const auto& lgmParam_ = model_->irlgm1f(ccy_idx);
        const auto& crlgmParam_ = model_->crlgm1f(ccy_idx);
        Real Hlt = lgmParam_->H(t);
        Real HlT = lgmParam_->H(T);

        if (it == cache_crlgm1fS_.end()) {
            if (ccy_idx == 0) {
                // domestic credit
                Real Hzt = lgmParam_->H(t);
                Real HzT = lgmParam_->H(T);
                Real zetal0 = crlgmParam_->zeta(t);
                Real zetal1 = model_->integrator()->operator()(
                    [&crlgmParam_](Real s) {
                        return crlgmParam_->H(s) * crlgmParam_->alpha(s) * crlgmParam_->alpha(s);
                    },
                    t, T);
                Real zetal2 = model_->integrator()->operator()(
                    [&crlgmParam_](Real s) {
                        return crlgmParam_->H(s) * crlgmParam_->H(s) * crlgmParam_->alpha(s) * crlgmParam_->alpha(s);
                    },
                    t, T);
                Real zetanl0 = model_->integrator()->operator()(
                    [this, &lgmParam_, &crlgmParam_](Real s) {
                        return model_->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::CR,
                                                   0) *
                               lgmParam_->alpha(s) * crlgmParam_->alpha(s);
                    },
                    t, T);
                Real zetanl1 = model_->integrator()->operator()(
                    [this, &lgmParam_, &crlgmParam_](Real s) {
                        return model_->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::CR,
                                                   0) *
                               crlgmParam_->H(s) * lgmParam_->alpha(s) * crlgmParam_->alpha(s);
                    },
                    t, T);
                // opposite signs for last two terms in the book
                V0 = 0.5 * Hlt * Hlt * zetal0 - Hlt * zetal1 + 0.5 * zetal2 + Hzt * Hlt * zetanl0 - Hzt * zetanl1;
                V_tilde = -0.5 * (HlT * HlT - Hlt * Hlt) * zetal0 + (HlT - Hlt) * zetal1 -
                          (HzT * HlT - Hzt * Hlt) * zetanl0 + (HzT - Hzt) * zetanl1;
                cache_crlgm1fS_.insert(std::make_pair(k, std::make_pair(V0, V_tilde)));
            } else {
                QL_FAIL("crlgm1fS: only domestic credit supported in vectorised version");
            }
        } else {
            // take V0 and V_tilde from cache
            V0 = it->second.first;
            V_tilde = it->second.second;
        }
        RandomVariable sp_t_vec(n, model_->crlgm1f(0)->termStructure()->survivalProbability(t));
        RandomVariable sp_T_vec(n, model_->crlgm1f(0)->termStructure()->survivalProbability(T));
        RandomVariable V0_vec(n, V0);
        RandomVariable V_tilde_vec(n, V_tilde);
        RandomVariable HlT_vec(n, HlT);
        RandomVariable Hlt_vec(n, Hlt);

        // compute final results depending on z and y
        // opposite sign for V0 in the book
        RandomVariable St = sp_t_vec * exp(-Hlt_vec * z + y - V0_vec);
        RandomVariable Stilde_t_T = sp_T_vec / sp_t_vec * exp(-(HlT_vec - Hlt_vec) * z + V_tilde_vec);
        return std::make_pair(St, Stilde_t_T);
    }

private:
    struct cache_key {
        Size i, ccy;
        double t, T;
        bool operator==(const cache_key& o) const { return (i == o.i) && (ccy == o.ccy) && (t == o.t) && (T == o.T); }
    };

    struct cache_hasher {
        std::size_t operator()(cache_key const& x) const {
            std::size_t seed = 0;
            boost::hash_combine(seed, x.i);
            boost::hash_combine(seed, x.ccy);
            boost::hash_combine(seed, x.t);
            boost::hash_combine(seed, x.T);
            return seed;
        }
    };
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    mutable boost::unordered_map<cache_key, std::pair<Real, Real>, cache_hasher> cache_crlgm1fS_;
};

} // namespace QuantExt
