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

/*! \file orea/engine/simpleim.hpp
    \brief simple im model for dynamic im calculations
*/

#include <orea/engine/simpledynamicsimm.hpp>

#include <boost/algorithm/string.hpp>

namespace ore {
namespace analytics {

using namespace QuantLib;
using namespace QuantExt;

SimpleDynamicSimm::SimpleDynamicSimm(const std::size_t n, const std::vector<std::string>& currencies,
                                     const std::vector<QuantLib::Period> irVegaTerms,
                                     const std::vector<QuantLib::Period> fxVegaTerms,
                                     const boost::shared_ptr<SimmConfiguration>& simmConfiguration)
    : n_(n), currencies_(currencies), irVegaTerms_(irVegaTerms), fxVegaTerms_(fxVegaTerms),
      simmConfiguration_(simmConfiguration) {

    corrIrFx_ = simmConfiguration_->correlationRiskClasses(SimmConfiguration::RiskClass::InterestRate,
                                                           SimmConfiguration::RiskClass::FX);
    irDeltaRw_ = simmConfiguration_->weight(CrifRecord::RiskType::IRCurve, std::string("USD"), std::string("30y"));

    irVegaRw_ = simmConfiguration_->weight(CrifRecord::RiskType::IRVol, std::string("USD"));

    irGamma_ = simmConfiguration_->correlation(CrifRecord::RiskType::IRVol, "USD", std::string(), std::string(),
                                               CrifRecord::RiskType::IRVol, "GBP", std::string(), std::string());

    irCurvatureScaling_ = simmConfiguration_->curvatureMarginScaling();

    irVegaCorrelations_ = Matrix(irVegaTerms.size(), irVegaTerms.size(), 0.0);
    for (std::size_t i = 0; i < irVegaTerms.size(); ++i) {
        auto p1 = ore::data::to_string(irVegaTerms[i]);
        boost::algorithm::to_lower(p1);
        irVegaCorrelations_(i, i) = 1.0;
        for (std::size_t j = 0; j < i; ++j) {
            auto p2 = ore::data::to_string(irVegaTerms[j]);
            boost::algorithm::to_lower(p2);
            irVegaCorrelations_(i, j) = irVegaCorrelations_(j, i) =
                simmConfiguration_->correlation(CrifRecord::RiskType::IRVol, "USD", p1, std::string(),
                                                CrifRecord::RiskType::IRVol, "USD", p2, std::string());
        }
    }

    irCurvatureWeights_ = Array(irVegaTerms.size());
    for (std::size_t i = 0; i < irVegaTerms.size(); ++i) {
        auto p1 = ore::data::to_string(irVegaTerms[i]);
        boost::algorithm::to_lower(p1);
        irCurvatureWeights_[i] = simmConfiguration_->curvatureWeight(CrifRecord::RiskType::IRVol, p1);
    }

    fxDeltaRw_ =
        simmConfiguration_->weight(CrifRecord::RiskType::FX, std::string("GBP"), boost::none, std::string("USD"));

    fxCorr_ = simmConfiguration_->correlation(CrifRecord::RiskType::FX, "GBP", std::string(), std::string(),
                                              CrifRecord::RiskType::FX, "GBP", std::string(), std::string(),
                                              std::string("USD"));

    std::cout << "corrIrFx             = " << corrIrFx_ << std::endl;
    std::cout << "irDeltaRw            = " << irDeltaRw_ << std::endl;
    std::cout << "irVegaRw             = " << irVegaRw_ << std::endl;
    std::cout << "irGamma              = " << irGamma_ << std::endl;
    std::cout << "irCurvatureScaling   = " << irCurvatureScaling_ << std::endl;
    std::cout << "irVegaCorr           = \n" << irVegaCorrelations_ << std::endl;
    std::cout << "irCurvatureWeights   = " << irCurvatureWeights_ << std::endl;
    std::cout << "fxDeltaRw            = " << fxDeltaRw_ << std::endl;
    std::cout << "fxCorr               = " << fxCorr_ << std::endl;
}

QuantExt::RandomVariable SimpleDynamicSimm::value(const std::vector<QuantExt::RandomVariable>& irDelta,
                                                  const std::vector<std::vector<QuantExt::RandomVariable>>& irVega,
                                                  const std::vector<QuantExt::RandomVariable>& fxDelta,
                                                  const std::vector<std::vector<QuantExt::RandomVariable>>& fxVega) {

    // DeltaMargin_IR

    RandomVariable deltaMarginIr(n_, 0.0);

    {
        std::vector<RandomVariable> Kb(currencies_.size(), RandomVariable(n_,0.0));

        for (std::size_t ccy = 0; ccy < currencies_.size(); ++ccy) {
            Kb[ccy] = irDelta[ccy] * RandomVariable(n_, irDeltaRw_);
        }

        for (std::size_t i = 0; i < currencies_.size(); ++i) {
            deltaMarginIr += Kb[i] * Kb[i];
            for (std::size_t j = 0; j < i; ++j) {
                deltaMarginIr += RandomVariable(n_, 2.0 * irGamma_) * Kb[i] * Kb[j];
            }
        }

        deltaMarginIr = sqrt(deltaMarginIr);
    }

    // VegaMargin_IR

    RandomVariable vegaMarginIr(n_, 0.0);

    {
        std::vector<RandomVariable> Kb(currencies_.size(), RandomVariable(n_,0.0)), Sb(currencies_.size(), RandomVariable(n_,0.0));

        for (std::size_t ccy = 0; ccy < currencies_.size(); ++ccy) {
            for (std::size_t i = 0; i < irVegaTerms_.size(); ++i) {
                auto tmp = RandomVariable(n_, irVegaRw_) * irVega[ccy][i];
                Kb[ccy] += tmp * tmp;
                Sb[ccy] += tmp;
                for (std::size_t j = 0; i < j; ++j) {
                    auto tmp2 = RandomVariable(n_, irVegaRw_) * irVega[ccy][j];
                    Kb[ccy] += RandomVariable(n_, 2.0 * irVegaCorrelations_(i, j)) * tmp * tmp2;
                }
            }
            Kb[ccy] = sqrt(Kb[ccy]);
            Sb[ccy] = max(min(Sb[ccy], Kb[ccy]), -Kb[ccy]);
        }

        for (std::size_t i = 0; i < currencies_.size(); ++i) {
            vegaMarginIr += Sb[i] * Sb[i];
            for (std::size_t j = 0; j < i; ++j) {
                vegaMarginIr += RandomVariable(n_, 2.0 * irGamma_) * Sb[i] * Sb[j];
            }
        }

        vegaMarginIr = sqrt(vegaMarginIr);
    }

    // CurvatureMargin_IR

    RandomVariable curvatureMarginIr(n_, 0.0);

    {
        std::vector<RandomVariable> Kb(currencies_.size(), RandomVariable(n_, 0.0)),
            Sb(currencies_.size(), RandomVariable(n_, 0.0));

        RandomVariable S(n_, 0.0), Sabs(n_, 0.0);

        for (std::size_t ccy = 0; ccy < currencies_.size(); ++ccy) {
            for (std::size_t i = 0; i < irVegaTerms_.size(); ++i) {
                auto tmp = RandomVariable(n_, irCurvatureWeights_[i]) * irVega[ccy][i];
                Kb[ccy] += tmp * tmp;
                Sb[ccy] += tmp;
                S += tmp;
                Sabs += abs(tmp);
                for (std::size_t j = 0; i < j; ++j) {
                    auto tmp2 = RandomVariable(n_, irVegaRw_) * irVega[ccy][j];
                    Kb[ccy] += RandomVariable(n_, 2.0 * irVegaCorrelations_(i, j)) * tmp * tmp2;
                }
            }
            Kb[ccy] = sqrt(Kb[ccy]);
            Sb[ccy] = max(min(Sb[ccy], Kb[ccy]), -Kb[ccy]);
        }

        for (std::size_t i = 0; i < currencies_.size(); ++i) {
            curvatureMarginIr += Sb[i] * Sb[i];
            for (std::size_t j = 0; j < i; ++j) {
                vegaMarginIr += RandomVariable(n_, 2.0 * irGamma_) * Sb[i] * Sb[j];
            }
        }

        RandomVariable theta = min(RandomVariable(n_, 0.0), S / Sabs);
        RandomVariable lambda = RandomVariable(n_, 5.634896601) * (RandomVariable(n_, 1.0) + theta) - theta;
        curvatureMarginIr =
            max(RandomVariable(n_, 0.0), S + lambda * sqrt(vegaMarginIr)) * RandomVariable(n_, irCurvatureScaling_);
    }

    // SIMM_IR

    RandomVariable imIr = deltaMarginIr + vegaMarginIr + curvatureMarginIr;

    // DeltaMargin_FX

    RandomVariable deltaMarginFx(n_, 0.0);

    {
        std::vector<RandomVariable> Kb(currencies_.size(), RandomVariable(n_,0.0));

        for (std::size_t ccy = 1; ccy < currencies_.size(); ++ccy) {
            Kb[ccy] = fxDelta[ccy - 1] * RandomVariable(n_, fxDeltaRw_);
        }

        for (std::size_t i = 1; i < currencies_.size(); ++i) {
            deltaMarginIr += Kb[i - 1] * Kb[i - 1];
            for (std::size_t j = 1; j < i ; ++j) {
                deltaMarginFx += RandomVariable(n_, 2.0 * fxCorr_) * Kb[i - 1] * Kb[j - 1];
            }
        }

        deltaMarginFx = sqrt(deltaMarginFx);
    }

    // VegaMargin_FX

    // CurvatureMargin_FX

    // SIMM_FX

    RandomVariable imFx = deltaMarginFx /* + vegaMarginFx + curvatureMarginFx */;

    // SIMM_RatesFX

    RandomVariable imProductRatesFx =
        sqrt(imIr * imIr + imFx * imFx + RandomVariable(n_, 2.0 * corrIrFx_) * imIr * imFx);

    // SIMM = SIMM_product = SIMM_RatesFX

    return imProductRatesFx;
}

} // namespace analytics
} // namespace ore
