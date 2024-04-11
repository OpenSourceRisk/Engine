/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/models/infdkvectorised.hpp>
#include <qle/utilities/inflation.hpp>

namespace QuantExt {

	InfDkVectorised::InfDkVectorised(const QuantLib::ext::shared_ptr<CrossAssetModel>& cam)
    : cam_(cam) {

	}

	std::pair<RandomVariable, RandomVariable> InfDkVectorised::infdkI(const Size i, const Time t, const Time T,
                                                                    const RandomVariable& z,
                                                                    const RandomVariable& y,
                                                                    bool indexIsInterpolated) const {
            //
            
            Size n_samples = z.size();

            std::pair<Real, Real> Vs = cam_->infdkV(i, t, T);
            RandomVariable V0(n_samples, Vs.first);
            RandomVariable V_tilde(n_samples, Vs.second);
            RandomVariable Hyt(n_samples, cam_->infdk(i)->H(t));
            RandomVariable HyT(n_samples, cam_->infdk(i)->H(T));

            // TODO account for seasonality ...
            // compute final results depending on z and y
            const auto& zts = cam_->infdk(i)->termStructure();
            auto dc = cam_->irlgm1f(0)->termStructure()->dayCounter();
            RandomVariable growth_t(n_samples, inflationGrowth(zts, t, dc, indexIsInterpolated));
            RandomVariable growth_T(n_samples, inflationGrowth(zts, T, dc, indexIsInterpolated));
            // Vectorize the scalars


            RandomVariable It = growth_t * exp(Hyt * z - y - V0);
            RandomVariable Itilde_t_T = growth_T / growth_t * exp((HyT - Hyt) * z + V_tilde);
            // concerning interpolation there is an inaccuracy here: if the index
            // is not interpolated, we still simulate the index value as of t
            // (and T), although we should go back to t, T which corresponds to
            // the last actual publication time of the index => is the approximation
            // here in this sense good enough that we can tolerate this?
            return std::make_pair(It, Itilde_t_T);
	}


} // namespace QuantExt
