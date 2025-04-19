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
                                     const std::vector<QuantLib::Period>& irDeltaTerms,
                                     const std::vector<Period>& irVegaTerms, const std::vector<Period>& fxVegaTerms,
                                     const boost::shared_ptr<SimmConfiguration>& simmConfiguration)
    : n_(n), currencies_(currencies), irDeltaTerms_(irDeltaTerms), irVegaTerms_(irVegaTerms), fxVegaTerms_(fxVegaTerms),
      simmConfiguration_(simmConfiguration) {

    corrIrFx_ = simmConfiguration_->correlationRiskClasses(SimmConfiguration::RiskClass::InterestRate,
                                                           SimmConfiguration::RiskClass::FX);

    irDeltaRw_ = Array(irDeltaTerms_.size());
    for (std::size_t i = 0; i < irDeltaTerms_.size(); ++i) {
        auto p1 = ore::data::to_string(irDeltaTerms[i]);
        boost::algorithm::to_lower(p1);
        irDeltaRw_[i] = simmConfiguration_->weight(CrifRecord::RiskType::IRCurve, std::string("USD"), p1);
    }

    irDeltaCorrelations_ = Matrix(irDeltaTerms.size(), irDeltaTerms.size(), 0.0);
    for (std::size_t i = 0; i < irDeltaTerms.size(); ++i) {
        auto p1 = ore::data::to_string(irDeltaTerms[i]);
        boost::algorithm::to_lower(p1);
        irDeltaCorrelations_(i, i) = 1.0;
        for (std::size_t j = 0; j < i; ++j) {
            auto p2 = ore::data::to_string(irDeltaTerms[j]);
            boost::algorithm::to_lower(p2);
            irDeltaCorrelations_(i, j) = irDeltaCorrelations_(j, i) =
                simmConfiguration_->correlation(CrifRecord::RiskType::IRCurve, "USD", p1, std::string(),
                                                CrifRecord::RiskType::IRCurve, "USD", p2, std::string());
        }
    }

    irGamma_ = simmConfiguration_->correlation(CrifRecord::RiskType::IRCurve, "USD", std::string(), std::string(),
                                               CrifRecord::RiskType::IRCurve, "GBP", std::string(), std::string());

    irVegaRw_ = simmConfiguration_->weight(CrifRecord::RiskType::IRVol, std::string("USD"));

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

    fxVegaRw_ =
        simmConfiguration_->weight(CrifRecord::RiskType::FXVol, std::string("GBPUSD"), boost::none, std::string("USD"));

    fxSigma_ =
        simmConfiguration_->sigma(CrifRecord::RiskType::FXVol, std::string("GBPUSD"), boost::none, std::string("USD"));

    fxHvr_ = simmConfiguration_->historicalVolatilityRatio(CrifRecord::RiskType::FXVol);

    fxCorr_ = simmConfiguration_->correlation(CrifRecord::RiskType::FX, "GBP", std::string(), std::string(),
                                              CrifRecord::RiskType::FX, "GBP", std::string(), std::string(),
                                              std::string("USD"));

    fxVegaCorrelations_ = Matrix(fxVegaTerms.size(), fxVegaTerms.size(), 0.0);
    for (std::size_t i = 0; i < fxVegaTerms.size(); ++i) {
        auto p1 = ore::data::to_string(fxVegaTerms[i]);
        boost::algorithm::to_lower(p1);
        fxVegaCorrelations_(i, i) = 1.0;
        for (std::size_t j = 0; j < i; ++j) {
            auto p2 = ore::data::to_string(fxVegaTerms[j]);
            boost::algorithm::to_lower(p2);
            fxVegaCorrelations_(i, j) = fxVegaCorrelations_(j, i) =
                simmConfiguration_->correlation(CrifRecord::RiskType::FXVol, "USD", p1, std::string(),
                                                CrifRecord::RiskType::FXVol, "USD", p2, std::string());
        }
    }

    fxCurvatureWeights_ = Array(fxVegaTerms.size());
    for (std::size_t i = 0; i < fxVegaTerms.size(); ++i) {
        auto p1 = ore::data::to_string(fxVegaTerms[i]);
        boost::algorithm::to_lower(p1);
        fxCurvatureWeights_[i] = simmConfiguration_->curvatureWeight(CrifRecord::RiskType::FXVol, p1);
    }

    // debug output
    // std::cout << "corrIrFx             = " << corrIrFx_ << std::endl;
    // std::cout << "irDeltaRw            = " << irDeltaRw_ << std::endl;
    // std::cout << "irVegaRw             = " << irVegaRw_ << std::endl;
    // std::cout << "irGamma              = " << irGamma_ << std::endl;
    // std::cout << "irDeltaCorr          = \n" << irDeltaCorrelations_ << std::endl;
    // std::cout << "irVegaCorr           = \n" << irVegaCorrelations_ << std::endl;
    // std::cout << "irCurvatureScaling   = " << irCurvatureScaling_ << std::endl;
    // std::cout << "irCurvatureWeights   = " << irCurvatureWeights_ << std::endl;
    // std::cout << "fxDeltaRw            = " << fxDeltaRw_ << std::endl;
    // std::cout << "fxVegaRw             = " << fxVegaRw_ << std::endl;
    // std::cout << "fxSigma              = " << fxSigma_ << std::endl;
    // std::cout << "fxHvr                = " << fxHvr_ << std::endl;
    // std::cout << "fxCorr               = " << fxCorr_ << std::endl;
    // std::cout << "fxVegaCorr           = \n" << fxVegaCorrelations_ << std::endl;
    // std::cout << "fxCurvatureWeights   = " << fxCurvatureWeights_ << std::endl;
    // end debug output
}

QuantExt::RandomVariable SimpleDynamicSimm::value(const std::vector<std::vector<QuantExt::RandomVariable>>& irDelta,
                                                  const std::vector<std::vector<QuantExt::RandomVariable>>& irVega,
                                                  const std::vector<QuantExt::RandomVariable>& fxDelta,
                                                  const std::vector<std::vector<QuantExt::RandomVariable>>& fxVega) {

    // DeltaMargin_IR

    RandomVariable deltaMarginIr(n_, 0.0);

    {
        std::vector<RandomVariable> Kb(currencies_.size(), RandomVariable(n_, 0.0)),
            Sb(currencies_.size(), RandomVariable(n_, 0.0));

        for (std::size_t ccy = 0; ccy < currencies_.size(); ++ccy) {
            for (std::size_t i = 0; i < irDeltaTerms_.size(); ++i) {
                auto tmp = RandomVariable(n_, irDeltaRw_[i]) * irDelta[ccy][i];
                Kb[ccy] += tmp * tmp;
                Sb[ccy] += tmp;
                for (std::size_t j = 0; j < i; ++j) {
                    auto tmp2 = RandomVariable(n_, irDeltaRw_[j]) * irDelta[ccy][j];
                    Kb[ccy] += RandomVariable(n_, 2.0 * irDeltaCorrelations_(i, j)) * tmp * tmp2;
                }
            }
            Kb[ccy] = sqrt(Kb[ccy]);
            Sb[ccy] = max(min(Sb[ccy], Kb[ccy]), -Kb[ccy]);
        }

        for (std::size_t i = 0; i < currencies_.size(); ++i) {
            deltaMarginIr += Kb[i] * Kb[i];
            for (std::size_t j = 0; j < i; ++j) {
                deltaMarginIr += RandomVariable(n_, 2.0 * irGamma_) * Sb[i] * Sb[j];
            }
        }

        deltaMarginIr = sqrt(deltaMarginIr);
    }

    // VegaMargin_IR

    RandomVariable vegaMarginIr(n_, 0.0);

    {
        std::vector<RandomVariable> Kb(currencies_.size(), RandomVariable(n_, 0.0)),
            Sb(currencies_.size(), RandomVariable(n_, 0.0));

        for (std::size_t ccy = 0; ccy < currencies_.size(); ++ccy) {
            for (std::size_t i = 0; i < irVegaTerms_.size(); ++i) {
                auto tmp = RandomVariable(n_, irVegaRw_) * irVega[ccy][i];
                Kb[ccy] += tmp * tmp;
                Sb[ccy] += tmp;
                for (std::size_t j = 0; j < i; ++j) {
                    auto tmp2 = RandomVariable(n_, irVegaRw_) * irVega[ccy][j];
                    Kb[ccy] += RandomVariable(n_, 2.0 * irVegaCorrelations_(i, j)) * tmp * tmp2;
                }
            }
            Kb[ccy] = sqrt(Kb[ccy]);
            Sb[ccy] = max(min(Sb[ccy], Kb[ccy]), -Kb[ccy]);
        }

        for (std::size_t i = 0; i < currencies_.size(); ++i) {
            vegaMarginIr += Kb[i] * Kb[i];
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
                for (std::size_t j = 0; j < i; ++j) {
                    auto tmp2 = RandomVariable(n_, irCurvatureWeights_[j]) * irVega[ccy][j];
                    Kb[ccy] += RandomVariable(n_, 2.0 * irVegaCorrelations_(i, j)) * tmp * tmp2;
                }
            }
            Kb[ccy] = sqrt(Kb[ccy]);
            Sb[ccy] = max(min(Sb[ccy], Kb[ccy]), -Kb[ccy]);
        }

        for (std::size_t i = 0; i < currencies_.size(); ++i) {
            curvatureMarginIr += Kb[i] * Kb[i];
            for (std::size_t j = 0; j < i; ++j) {
                curvatureMarginIr += RandomVariable(n_, 2.0 * irGamma_) * Sb[i] * Sb[j];
            }
        }

        RandomVariable theta = min(RandomVariable(n_, 0.0), S / Sabs);
        RandomVariable lambda = RandomVariable(n_, 5.634896601) * (RandomVariable(n_, 1.0) + theta) - theta;
        curvatureMarginIr = max(RandomVariable(n_, 0.0), S + lambda * sqrt(curvatureMarginIr)) *
                            RandomVariable(n_, irCurvatureScaling_);
    }

    // SIMM_IR

    RandomVariable imIr = deltaMarginIr + vegaMarginIr + curvatureMarginIr;

    // DeltaMargin_FX

    RandomVariable deltaMarginFx(n_, 0.0);

    {
        std::vector<RandomVariable> Kb(currencies_.size(), RandomVariable(n_, 0.0));

        for (std::size_t ccy = 1; ccy < currencies_.size(); ++ccy) {
            Kb[ccy - 1] = fxDelta[ccy - 1] * RandomVariable(n_, fxDeltaRw_);
        }

        for (std::size_t i = 1; i < currencies_.size(); ++i) {
            deltaMarginFx += Kb[i - 1] * Kb[i - 1];
            for (std::size_t j = 1; j < i; ++j) {
                deltaMarginFx += RandomVariable(n_, 2.0 * fxCorr_) * Kb[i - 1] * Kb[j - 1];
            }
        }

        deltaMarginFx = sqrt(deltaMarginFx);
    }

    // VegaMargin_FX

    RandomVariable vegaMarginFx(n_, 0.0);

    {
        std::vector<RandomVariable> Kb(currencies_.size() - 1, RandomVariable(n_, 0.0)),
            Sb(currencies_.size() - 1, RandomVariable(n_, 0.0));

        for (std::size_t ccy = 1; ccy < currencies_.size(); ++ccy) {
            for (std::size_t i = 0; i < fxVegaTerms_.size(); ++i) {
                auto tmp = RandomVariable(n_, fxVegaRw_ * fxSigma_ * fxHvr_) * fxVega[ccy - 1][i];
                Kb[ccy - 1] += tmp * tmp;
                Sb[ccy - 1] += tmp;
                for (std::size_t j = 0; j < i; ++j) {
                    auto tmp2 = RandomVariable(n_, fxVegaRw_ * fxSigma_ * fxHvr_) * fxVega[ccy - 1][j];
                    Kb[ccy - 1] += RandomVariable(n_, 2.0 * fxVegaCorrelations_(i, j)) * tmp * tmp2;
                }
            }
            Kb[ccy - 1] = sqrt(Kb[ccy - 1]);
            Sb[ccy - 1] = max(min(Sb[ccy - 1], Kb[ccy - 1]), -Kb[ccy - 1]);
        }

        for (std::size_t i = 1; i < currencies_.size(); ++i) {
            vegaMarginFx += Kb[i - 1] * Kb[i - 1];
            for (std::size_t j = 1; j < i; ++j) {
                vegaMarginFx += RandomVariable(n_, 2.0 * fxCorr_) * Sb[i - 1] * Sb[j - 1];
            }
        }

        vegaMarginFx = sqrt(vegaMarginFx);
    }

    // CurvatureMargin_FX

    RandomVariable curvatureMarginFx(n_, 0.0);

    {
        std::vector<RandomVariable> Kb(currencies_.size() - 1, RandomVariable(n_, 0.0)),
            Sb(currencies_.size() - 1, RandomVariable(n_, 0.0));

        RandomVariable S(n_, 0.0), Sabs(n_, 0.0);

        for (std::size_t ccy = 1; ccy < currencies_.size(); ++ccy) {
            for (std::size_t i = 0; i < fxVegaTerms_.size(); ++i) {
                auto tmp = RandomVariable(n_, fxCurvatureWeights_[i] * fxSigma_ * fxHvr_) * fxVega[ccy - 1][i];
                Kb[ccy - 1] += tmp * tmp;
                Sb[ccy - 1] += tmp;
                S += tmp;
                Sabs += abs(tmp);
                for (std::size_t j = 0; j < i; ++j) {
                    auto tmp2 = RandomVariable(n_, fxCurvatureWeights_[j] * fxSigma_ * fxHvr_) * fxVega[ccy - 1][j];
                    Kb[ccy - 1] += RandomVariable(n_, 2.0 * fxVegaCorrelations_(i, j)) * tmp * tmp2;
                }
            }
            Kb[ccy - 1] = sqrt(Kb[ccy - 1]);
            Sb[ccy - 1] = max(min(Sb[ccy - 1], Kb[ccy - 1]), -Kb[ccy - 1]);
        }

        for (std::size_t i = 1; i < currencies_.size(); ++i) {
            curvatureMarginFx += Kb[i - 1] * Kb[i - 1];
            for (std::size_t j = 1; j < i; ++j) {
                curvatureMarginFx += RandomVariable(n_, 2.0 * irGamma_) * Sb[i - 1] * Sb[j - 1];
            }
        }

        RandomVariable theta = min(RandomVariable(n_, 0.0), S / Sabs);
        RandomVariable lambda = RandomVariable(n_, 5.634896601) * (RandomVariable(n_, 1.0) + theta) - theta;
        curvatureMarginFx = max(RandomVariable(n_, 0.0), S + lambda * sqrt(curvatureMarginFx));
    }

    // SIMM_FX

    RandomVariable imFx = deltaMarginFx + vegaMarginFx + curvatureMarginFx;

    // SIMM_RatesFX

    RandomVariable imProductRatesFx =
        sqrt(imIr * imIr + imFx * imFx + RandomVariable(n_, 2.0 * corrIrFx_) * imIr * imFx);

    // SIMM = SIMM_product = SIMM_RatesFX

    // debug output
    // std::cout << expectation(imProductRatesFx).at(0) << "," << expectation(imIr).at(0) << ","
    //           << expectation(deltaMarginIr).at(0) << "," << expectation(vegaMarginIr).at(0) << ","
    //           << expectation(curvatureMarginIr).at(0) << "," << expectation(imFx).at(0) << ","
    //           << expectation(deltaMarginFx).at(0) << "," << expectation(vegaMarginFx).at(0) << ","
    //           << expectation(curvatureMarginFx).at(0) << std::endl;

    // std::cout << imProductRatesFx.at(7) << "," << imIr.at(7) << "," << deltaMarginIr.at(7) << "," << vegaMarginIr.at(7)
    //           << "," << curvatureMarginIr.at(7) << "," << imFx.at(7) << "," << deltaMarginFx.at(7) << ","
    //           << vegaMarginFx.at(7) << "," << curvatureMarginFx.at(7) << std::endl;

    // std::cout << "SIMM_RatesFX         : " << expectation(imProductRatesFx).at(0) << std::endl;
    // std::cout << "  SIMM_Rates         : " << expectation(imIr).at(0) << std::endl;
    // std::cout << "    delta            : " << expectation(deltaMarginIr).at(0) << std::endl;
    // std::cout << "    vega             : " << expectation(vegaMarginIr).at(0) << std::endl;
    // std::cout << "    curvature        : " << expectation(curvatureMarginIr).at(0) << std::endl;
    // std::cout << "  SIMM_FX            : " << expectation(imFx).at(0) << std::endl;
    // std::cout << "    delta            : " << expectation(deltaMarginFx).at(0) << std::endl;
    // std::cout << "    vega             : " << expectation(vegaMarginFx).at(0) << std::endl;
    // std::cout << "    curvature        : " << expectation(curvatureMarginFx).at(0) << std::endl;
    // debug debg output

    return imProductRatesFx;
}

} // namespace analytics
} // namespace ore
