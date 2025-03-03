/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <ored/scripting/models/gaussiancamcg.hpp>
#include <ored/scripting/models/lgmcg.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/quotes/simplequote.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

namespace {
inline void setValue2(std::vector<std::vector<std::size_t>>& m, const std::size_t value,
                      const QuantLib::ext::shared_ptr<const QuantExt::CrossAssetModel>& model,
                      const QuantExt::CrossAssetModel::AssetType& t1, const Size& i1,
                      const QuantExt::CrossAssetModel::AssetType& t2, const Size& i2, const Size& offset1 = 0,
                      const Size& offset2 = 0) {
    m[model->pIdx(t1, i1, offset1)][model->wIdx(t2, i2, offset2)] = value;
}
} // namespace

GaussianCamCG::GaussianCamCG(
    const Handle<CrossAssetModel>& cam, const Size paths, const std::vector<std::string>& currencies,
    const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
    const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
    const std::set<Date>& simulationDates, const Size timeStepsPerYear, const IborFallbackConfig& iborFallbackConfig,
    const std::vector<Size>& projectedStateProcessIndices,
    const std::vector<std::string>& conditionalExpectationModelStates, const std::vector<Date>& stickyCloseOutDates)
    : ModelCGImpl(curves.front()->dayCounter(), paths, currencies, irIndices, infIndices, indices, indexCurrencies,
                  simulationDates, iborFallbackConfig),
      cam_(cam), curves_(curves), fxSpots_(fxSpots), timeStepsPerYear_(timeStepsPerYear),
      projectedStateProcessIndices_(projectedStateProcessIndices), stickyCloseOutDates_(stickyCloseOutDates) {

    // check inputs

    QL_REQUIRE(!cam_.empty(), "model is empty");

    // check restrictions on cam model (ir-fx with hw, bs and Euler disc only at the moment)

    QL_REQUIRE(cam_->discretization() == CrossAssetModel::Discretization::Euler,
               "GaussianCamCG requires discretization 'Euler'.");

    QL_REQUIRE(cam_->components(CrossAssetModel::AssetType::IR) > 0, "GaussianCamCG: no IR component given");
    QL_REQUIRE(cam_->components(CrossAssetModel::AssetType::INF) == 0, "GaussianCamCG: asset type INF not supported");
    QL_REQUIRE(cam_->components(CrossAssetModel::AssetType::CR) == 0, "GaussianCamCG: asset type CR not supported");
    QL_REQUIRE(cam_->components(CrossAssetModel::AssetType::EQ) == 0, "GaussianCamCG: asset type EQ not supported");
    QL_REQUIRE(cam_->components(CrossAssetModel::AssetType::COM) == 0, "GaussianCamCG: asset type COM not supported");

    for (Size i = 0; i < cam_->components(CrossAssetModel::AssetType::IR); ++i) {
        QL_REQUIRE(cam_->modelType(CrossAssetModel::AssetType::IR, i) == CrossAssetModel::ModelType::LGM1F,
                   "GaussianCamCG: IR model type LGM required.");
    }
    for (Size i = 0; i < cam_->components(CrossAssetModel::AssetType::FX); ++i) {
        QL_REQUIRE(cam_->modelType(CrossAssetModel::AssetType::FX, i) == CrossAssetModel::ModelType::BS,
                   "GaussianCamCG: FX model type BS required.");
    }

    // register with observables

    for (auto const& o : curves_)
        registerWith(o);
    for (auto const& o : fxSpots_)
        registerWith(o);

    registerWith(cam_);

    // set conditional expectation model states

    conditionalExpectationUseIr_ =
        conditionalExpectationModelStates.empty() ||
        std::find(conditionalExpectationModelStates.begin(), conditionalExpectationModelStates.end(), "IR") !=
            conditionalExpectationModelStates.end();
    conditionalExpectationUseInf_ =
        conditionalExpectationModelStates.empty() ||
        std::find(conditionalExpectationModelStates.begin(), conditionalExpectationModelStates.end(), "INF") !=
            conditionalExpectationModelStates.end();
    conditionalExpectationUseAsset_ =
        conditionalExpectationModelStates.empty() ||
        std::find(conditionalExpectationModelStates.begin(), conditionalExpectationModelStates.end(), "Asset") !=
            conditionalExpectationModelStates.end();

} // GaussianCamCG ctor

const Date& GaussianCamCG::referenceDate() const {
    calculate();
    return referenceDate_;
}

Size GaussianCamCG::size() const { return ModelCG::size(); }

void GaussianCamCG::performCalculations() const {

    // needed for base class performCalculations()

    referenceDate_ = curves_.front()->referenceDate();

    // update cg version if necessary (eval date changed)

    ModelCGImpl::performCalculations();

    // if cg version has changed => update time grid related members and clear paths, so that they
    // are populated in derived classes

    if (cgVersion() != underlyingPathsCgVersion_) {

        // set up eff sim dates and time grid

        effectiveSimulationDates_.clear();
        effectiveSimulationDates_.insert(referenceDate());
        for (auto const& d : simulationDates_) {
            if (d >= referenceDate())
                effectiveSimulationDates_.insert(d);
        }

        for (auto const& d : stickyCloseOutDates_) {
            if (d >= referenceDate())
                effectiveSimulationDates_.insert(d);
        }

        std::vector<Real> times;
        for (auto const& d : effectiveSimulationDates_) {
            times.push_back(curves_.front()->timeFromReference(d));
        }

        Size steps = std::max(std::lround(timeStepsPerYear_ * times.back() + 0.5), 1l);
        timeGrid_ = TimeGrid(times.begin(), times.end(), steps);

        positionInTimeGrid_.resize(times.size());
        for (Size i = 0; i < positionInTimeGrid_.size(); ++i)
            positionInTimeGrid_[i] = timeGrid_.index(times[i]);

        // clear underlying paths

        underlyingPaths_.clear();
        underlyingPathsCgVersion_ = cgVersion();
    }

    // noting to do if underlying paths are populated

    if (!underlyingPaths_.empty())
        return;

    // exit if there are no future simulation dates (i.e. only the reference date)

    if (effectiveSimulationDates_.size() == 1)
        return;

    // init underlying path where we map a date to a node representing the path value

    for (auto const& d : effectiveSimulationDates_) {
        underlyingPaths_[d] = std::vector<std::size_t>(indices_.size(), ComputationGraph::nan);
        irStates_[d] = std::vector<std::size_t>(currencies_.size(), ComputationGraph::nan);
        infStates_[d] = std::vector<std::pair<std::size_t, std::size_t>>(
            infIndices_.size(), std::make_pair(ComputationGraph::nan, ComputationGraph::nan));
    }

    // populate index mappings

    currencyPositionInProcess_.clear();
    currencyPositionInCam_.clear();
    for (Size i = 0; i < currencies_.size(); ++i) {
        currencyPositionInProcess_.push_back(
            cam_->pIdx(CrossAssetModel::AssetType::IR, cam_->ccyIndex(parseCurrency(currencies_[i]))));
        currencyPositionInCam_.push_back(
            cam_->idx(CrossAssetModel::AssetType::IR, cam_->ccyIndex(parseCurrency(currencies_[i]))));
    }

    irIndexPositionInCam_.clear();
    for (Size i = 0; i < irIndices_.size(); ++i) {
        irIndexPositionInCam_.push_back(cam_->ccyIndex(irIndices_[i].second->currency()));
    }

    infIndexPositionInProcess_.clear();
    infIndexPositionInCam_.clear();
    for (Size i = 0; i < infIndices_.size(); ++i) {
        Size infIdx = cam_->infIndex(infIndices_[i].first.infName());
        infIndexPositionInProcess_.push_back(cam_->pIdx(CrossAssetModel::AssetType::INF, infIdx));
        infIndexPositionInCam_.push_back(infIdx);
    }

    indexPositionInProcess_.clear();
    for (Size i = 0; i < indices_.size(); ++i) {
        if (indices_[i].isFx()) {
            // FX
            Size ccyIdx = cam_->ccyIndex(parseCurrency(indexCurrencies_[i]));
            QL_REQUIRE(ccyIdx > 0, "fx index '" << indices_[i] << "' has unexpected for ccy = base ccy");
            indexPositionInProcess_.push_back(cam_->pIdx(CrossAssetModel::AssetType::FX, ccyIdx - 1));
            eqIndexInCam_.push_back(Null<Size>());
        } else if (indices_[i].isEq()) {
            // EQ
            Size eqIdx = cam_->eqIndex(indices_[i].eq()->name());
            indexPositionInProcess_.push_back(cam_->pIdx(CrossAssetModel::AssetType::EQ, eqIdx));
            eqIndexInCam_.push_back(eqIdx);
        } else {
            QL_FAIL("index '" << indices_[i].name() << "' expected to be FX or EQ");
        }
    }

    // set the required random variables to evolve the stochastic process

    randomVariates_ = std::vector<std::vector<std::size_t>>(cam_->brownians() + cam_->auxBrownians(),
                                                            std::vector<std::size_t>(timeGrid_.size() - 1));
    for (Size j = 0; j < cam_->brownians() + cam_->auxBrownians(); ++j) {
        for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
            randomVariates_[j][i] = cg_insert(*g_);
        }
    }

    // evolve the stochastic process, for now we only evolve IR LGM processes + FX processes

    auto cam(cam_); // acquire ownership for our own model parameter calculations

    QL_REQUIRE(timeGrid_.size() == effectiveSimulationDates_.size(),
               "GaussianCamCG: time grid size ("
                   << timeGrid_.size() << ") does not match effective simulation dates size ("
                   << effectiveSimulationDates_.size()
                   << "), this is currently not supported. The parameter timeStepsPerYear (" << timeStepsPerYear_
                   << ") shoudl be 1");

    // add sqrt correlation model parameters

    std::vector<std::vector<std::size_t>> sqrtCorrelation(cam->correlation().rows(),
                                                          std::vector<std::size_t>(cam->correlation().columns()));

    for (Size i = 0; i < cam->correlation().rows(); ++i) {
        for (Size j = 0; j < cam->correlation().columns(); ++j) {
            sqrtCorrelation[i][j] = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::sqrtCorr, {}, {}, {}, {}, {}, i, j),
                [cam, i, j] { return cam->stateProcess()->sqrtCorrelation()(i, j); });
        }
    }

    // precompute diffusion on correlated brownians nodes

    std::vector<std::vector<std::vector<std::size_t>>> diffusionOnCorrelatedBrownians(
        timeGrid_.size() - 1, std::vector<std::vector<std::size_t>>(
                                  cam->dimension(), std::vector<std::size_t>(cam->brownians(), cg_const(*g_, 0.0))));

    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        QuantLib::Date date = *std::next(effectiveSimulationDates_.begin(), i);
        Real t = timeGrid_[i];
        for (Size j = 0; j < cam->components(CrossAssetModel::AssetType::IR); ++j) {
            std::size_t alpha = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_alpha, currencies_[j], {}, date),
                [cam, j, t] { return cam->irlgm1f(j)->alpha(t); });
            setValue2(diffusionOnCorrelatedBrownians[i], alpha, *cam, CrossAssetModel::AssetType::IR, j,
                      CrossAssetModel::AssetType::IR, j, 0, 0);
        }
        for (Size j = 0; j < cam->components(CrossAssetModel::AssetType::FX); ++j) {
            std::size_t sigma = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter ::Type::fxbs_sigma, currencies_[j + 1], {}, date),
                [cam, j, t] { return cam->fxbs(j)->sigma(t); });
            setValue2(diffusionOnCorrelatedBrownians[i], sigma, *cam, CrossAssetModel::AssetType::FX, j,
                      CrossAssetModel::AssetType::FX, j, 0, 0);
        }
        if (cam->measure() == IrModel::Measure::BA) {
            std::size_t H0 = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_H, currencies_[0], {}, date),
                [cam, t] { return cam->irlgm1f(0)->H(t); });
            std::size_t alpha0 = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_alpha, currencies_[0], {}, date),
                [cam, t] { return cam->irlgm1f(0)->alpha(t); });
            setValue2(diffusionOnCorrelatedBrownians[i], cg_mult(*g_, alpha0, H0), *cam, CrossAssetModel::AssetType::IR,
                      0, CrossAssetModel::AssetType::IR, 0, 1, 0);
        }
    }

    // precompute drift nodes (state independent parts)

    std::vector<std::vector<std::size_t>> drift(timeGrid_.size() - 1,
                                                std::vector<std::size_t>(cam->dimension(), cg_const(*g_, 0.0)));
    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        QuantLib::Date date = *std::next(effectiveSimulationDates_.begin(), i);
        QuantLib::Date date2 = *std::next(effectiveSimulationDates_.begin(), i + 1);
        Real t = timeGrid_[i];
        Real t2 = timeGrid_[i + 1];
        std::size_t H0 =
            addModelParameter(ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_H, currencies_[0], {}, date),
                              [cam, t] { return cam->irlgm1f(0)->H(t); });
        std::size_t alpha0 = addModelParameter(
            ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_alpha, currencies_[0], {}, date),
            [cam, t] { return cam->irlgm1f(0)->alpha(t); });
        for (Size j = 0; j < cam->components(CrossAssetModel::AssetType::IR); ++j) {
            std::size_t H = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_H, currencies_[j], {}, date),
                [cam, t, j] { return cam->irlgm1f(j)->H(t); });
            std::size_t alpha = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_alpha, currencies_[j], {}, date),
                [cam, t, j] { return cam->irlgm1f(j)->alpha(t); });
            if (j == 0) {
                if (cam->measure() == IrModel::Measure::BA) {
                    drift[i][cam->pIdx(CrossAssetModel::AssetType::IR, i, 0)] =
                        cg_negative(*g_, cg_mult(*g_, cg_mult(*g_, H, alpha), alpha));
                }
            } else {
                std::size_t sigma = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::fxbs_sigma, currencies_[j], {}, date),
                    [cam, t, j] { return cam->fxbs(j - 1)->sigma(t); });
                std::size_t rhozz0j = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::cam_corrzz, {}, {}, {}, {}, {}, 0, j),
                    [cam, j] {
                        return cam->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::IR, j);
                    });
                std::size_t rhozx0j = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::cam_corrzx, {}, {}, {}, {}, {}, 0, j - 1),
                    [cam, j] {
                        return cam->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::FX,
                                                j - 1);
                    });
                std::size_t rhozxjj = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::cam_corrzx, {}, {}, {}, {}, {}, j, j - 1),
                    [cam, j] {
                        return cam->correlation(CrossAssetModel::AssetType::IR, j, CrossAssetModel::AssetType::FX,
                                                j - 1);
                    });
                drift[i][cam->pIdx(CrossAssetModel::AssetType::IR, j, 0)] =
                    cg_add(*g_, {cg_negative(*g_, cg_mult(*g_, cg_mult(*g_, H, alpha), alpha)),
                                 cg_mult(*g_, cg_mult(*g_, cg_mult(*g_, H0, alpha0), alpha), rhozz0j),
                                 cg_negative(*g_, cg_mult(*g_, cg_mult(*g_, sigma, alpha), rhozxjj))});
                std::size_t dsc0a = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, currencies_[0], "default", date),
                    [cam, t] { return cam->irlgm1f(0)->termStructure()->discount(t); });
                std::size_t dsc0b = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, currencies_[0], "default", date2),
                    [cam, t2] { return cam->irlgm1f(0)->termStructure()->discount(t2); });
                std::size_t dscja = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, currencies_[j], "default", date),
                    [cam, t, j] { return cam->irlgm1f(j)->termStructure()->discount(t); });
                std::size_t dscjb = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, currencies_[j], "default", date2),
                    [cam, t2, j] { return cam->irlgm1f(j)->termStructure()->discount(t2); });
                std::size_t irDrift =
                    cg_mult(*g_, cg_const(*g_, -1.0 / (t2 - t)),
                            cg_log(*g_, cg_mult(*g_, cg_div(*g_, dsc0b, dsc0a), cg_div(*g_, dscja, dscjb))));
                drift[i][cam->pIdx(CrossAssetModel::AssetType::FX, j - 1, 0)] =
                    cg_add(*g_, {cg_mult(*g_, cg_mult(*g_, cg_mult(*g_, H0, alpha0), sigma), rhozx0j), irDrift,
                                 cg_negative(*g_, cg_mult(*g_, cg_mult(*g_, cg_const(*g_, 0.5), sigma), sigma))});
                if (cam->measure() == IrModel::Measure::BA) {
                    drift[i][cam->pIdx(CrossAssetModel::AssetType::IR, j, 0)] =
                        cg_subtract(*g_, drift[i][cam->pIdx(CrossAssetModel::AssetType::IR, j, 0)],
                                    cg_mult(*g_, cg_mult(*g_, cg_mult(*g_, H0, alpha0), alpha), rhozz0j));
                    drift[i][cam->pIdx(CrossAssetModel::AssetType::FX, j - 1, 0)] =
                        cg_subtract(*g_, drift[i][cam->pIdx(CrossAssetModel::AssetType::FX, j - 1, 0)],
                                    cg_mult(*g_, cg_mult(*g_, cg_mult(*g_, H0, alpha0), sigma), rhozx0j));
                }
            }
        }
    }

    // initialize state vector

    std::vector<std::size_t> state(cam->dimension(), cg_const(*g_, 0.0));

    for (Size j = 0; j < cam->components(CrossAssetModel::AssetType::FX); ++j) {
        state[cam->pIdx(CrossAssetModel::AssetType::FX, j, 0)] =
            addModelParameter(ModelCG::ModelParameter(ModelCG::ModelParameter::Type::logFxSpot, currencies_[j + 1]),
                              [cam, j] { return std::log(cam->fxbs(j)->fxSpotToday()->value()); });
    }

    // set initial model states

    for (Size j = 0; j < currencies_.size(); ++j) {
        irStates_[*effectiveSimulationDates_.begin()][j] = state[cam->pIdx(CrossAssetModel::AssetType::IR, j, 0)];
    }

    for (Size j = 0; j < indices_.size(); ++j) {
        underlyingPaths_[*effectiveSimulationDates_.begin()][j] = cg_exp(*g_, state[indexPositionInProcess_[j]]);
    }

    // evolve model state

    std::size_t dateIndex = 1;
    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {

        // state dependent drifts

        std::vector<std::size_t> drift2(cam->dimension(), cg_const(*g_, 0.0));

        QuantLib::Date date = *std::next(effectiveSimulationDates_.begin(), i);
        Real t = timeGrid_[i];

        for (Size j = 1; j < cam->components(CrossAssetModel::AssetType::IR); ++j) {
            std::size_t H = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_H, currencies_[j], {}, date, {}, {}, 0, 0),
                [cam, t, j] { return cam->irlgm1f(j)->H(t); });
            std::size_t H0 = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_H, currencies_[0], {}, date),
                [cam, t] { return cam->irlgm1f(0)->H(t); });
            std::size_t Hprime = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_Hprime, currencies_[j], {}, date),
                [cam, t, j] { return cam->irlgm1f(j)->Hprime(t); });
            std::size_t Hprime0 = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_Hprime, currencies_[0], {}, date),
                [cam, t] { return cam->irlgm1f(0)->Hprime(t); });
            std::size_t zeta = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_zeta, currencies_[j], {}, date),
                [cam, t, j] { return cam->irlgm1f(j)->zeta(t); });
            std::size_t zeta0 = addModelParameter(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_zeta, currencies_[0], {}, date),
                [cam, t] { return cam->irlgm1f(0)->zeta(t); });
            drift2[cam->pIdx(CrossAssetModel::AssetType::FX, j - 1, 0)] = cg_add(
                *g_, {cg_mult(*g_, state[cam->pIdx(CrossAssetModel::AssetType::IR, 0, 0)], Hprime0),
                      cg_mult(*g_, cg_mult(*g_, zeta0, Hprime0), H0),
                      cg_negative(*g_, cg_mult(*g_, state[cam->pIdx(CrossAssetModel::AssetType::IR, j, 0)], Hprime)),
                      cg_negative(*g_, cg_mult(*g_, cg_mult(*g_, zeta, Hprime), H))});
        }

        // state -> state + drift * dt + diffusion * dz * sqrt(dt), dz = sqrtCorrelation * dw

        std::vector<std::size_t> dz(cam->brownians());
        for (Size j = 0; j < cam->brownians(); ++j) {
            dz[j] = cg_const(*g_, 0.0);
            for (Size k = 0; k < cam->brownians(); ++k) {
                dz[j] = cg_add(*g_, dz[j],
                               cg_mult(*g_, cg_const(*g_, std::sqrt(timeGrid_[i + 1] - timeGrid_[i])),
                                       cg_mult(*g_, sqrtCorrelation[j][k], randomVariates_[k][i])));
            }
        }
        for (Size j = 0; j < cam->dimension(); ++j) {
            for (Size k = 0; k < cam->brownians(); ++k) {
                state[j] = cg_add(*g_, state[j], cg_mult(*g_, diffusionOnCorrelatedBrownians[i][j][k], dz[k]));
            }

            state[j] = cg_add(
                *g_, state[j],
                cg_mult(*g_, cg_const(*g_, timeGrid_[i + 1] - timeGrid_[i]), cg_add(*g_, drift[i][j], drift2[j])));
        }

        // set model states

        if (positionInTimeGrid_[dateIndex] == i + 1) {

            for (Size j = 0; j < currencies_.size(); ++j) {
                irStates_[*std::next(effectiveSimulationDates_.begin(), dateIndex)][j] =
                    state[cam->pIdx(CrossAssetModel::AssetType::IR, j, 0)];
            }

            for (Size j = 0; j < indices_.size(); ++j) {
                underlyingPaths_[*std::next(effectiveSimulationDates_.begin(), dateIndex)][j] =
                    cg_exp(*g_, state[indexPositionInProcess_[j]]);
            }

            ++dateIndex;
        }
    }

    QL_REQUIRE(dateIndex == effectiveSimulationDates_.size(),
               "GaussianCamCG:internal error, did not populate all irState time steps.");
}

Date GaussianCamCG::adjustForStickyCloseOut(const Date& d) const {
    if (useStickyCloseOutDates_) {
        std::size_t idx = std::distance(simulationDates_.begin(), simulationDates_.upper_bound(d));
        if (idx == 0)
            return d;
        std::size_t effIdx = std::min(idx - 1, simulationDates_.size() - 1);
        return d + (stickyCloseOutDates_[effIdx] - *std::next(simulationDates_.begin(), effIdx));
    }
    return d;
}

std::size_t GaussianCamCG::getInterpolatedUnderlyingPath(const Date& d, const Size indexNo) const {
    if (d > *effectiveSimulationDates_.rbegin())
        return underlyingPaths_.at(*effectiveSimulationDates_.rbegin()).at(indexNo);
    if (effectiveSimulationDates_.find(d) != effectiveSimulationDates_.end())
        return underlyingPaths_.at(d).at(indexNo);
    auto [d1, d2, w1, w2] = getInterpolationWeights(d, effectiveSimulationDates_);
    std::cout << "d   " << d << std::endl;
    std::cout << "d1  " << d1 << std::endl;
    std::cout << "d2  " << d2 << std::endl;
    return cg_add(*g_, cg_mult(*g_, w1, underlyingPaths_.at(d1).at(indexNo)),
                  cg_mult(*g_, w2, underlyingPaths_.at(d2).at(indexNo)));
}

std::size_t GaussianCamCG::getInterpolatedIrState(const Date& d, const Size ccyIndex) const {
    if (d > *effectiveSimulationDates_.rbegin())
        return irStates_.at(*effectiveSimulationDates_.rbegin()).at(ccyIndex);
    if (effectiveSimulationDates_.find(d) != effectiveSimulationDates_.end())
        return irStates_.at(d).at(ccyIndex);
    auto [d1, d2, w1, w2] = getInterpolationWeights(d, effectiveSimulationDates_);
    std::cout << "d   " << d << std::endl;
    std::cout << "d1  " << d1 << std::endl;
    std::cout << "d2  " << d2 << std::endl;
    return cg_add(*g_, cg_mult(*g_, w1, irStates_.at(d1).at(ccyIndex)),
                  cg_mult(*g_, w2, irStates_.at(d2).at(ccyIndex)));
}

std::size_t GaussianCamCG::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    QL_REQUIRE(fwd == Null<Date>(), "GaussianCamCG::getIndexValue(): fwd != null not implemented ("
                                        << indexNo << "," << d << "," << fwd << ")");
    return getInterpolatedUnderlyingPath(adjustForStickyCloseOut(d), indexNo);
}

std::size_t GaussianCamCG::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date fixingDate = d;
    if (fwd != Null<Date>())
        fixingDate = fwd;
    // ensure a valid fixing date
    fixingDate = irIndices_[indexNo].second->fixingCalendar().adjust(fixingDate);
    Size currencyIdx = irIndexPositionInCam_[indexNo];
    auto cam(cam_);
    LgmCG lgmcg(
        currencies_[currencyIdx], *g_, [cam, currencyIdx] { return cam->irlgm1f(currencyIdx); }, modelParameters_,
        derivedModelParameters_);
    return lgmcg.fixing(irIndices_[indexNo].second, fixingDate, d,
                        getInterpolatedIrState(adjustForStickyCloseOut(d), currencyIdx));
}

std::size_t GaussianCamCG::getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    QL_FAIL("GaussianCamCG::getInfIndexValue(): not implemented");
}

std::size_t GaussianCamCG::fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate,
                                      const Date& start, const Date& end, const Real spread, const Real gearing,
                                      const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                      const bool includeSpread, const Real cap, const Real floor,
                                      const bool nakedOption, const bool localCapFloor) const {
    calculate();
    QL_FAIL("GaussianCamCG::fwdCompAvg(): not implemented");
}

std::size_t GaussianCamCG::getDiscount(const Size idx, const Date& s, const Date& t) const {
    auto cam(cam_);
    Size cpidx = currencyPositionInCam_[idx];
    LgmCG lgmcg(
        currencies_[idx], *g_, [cam, cpidx] { return cam->irlgm1f(cpidx); }, modelParameters_, derivedModelParameters_);
    return lgmcg.discountBond(s, t, getInterpolatedIrState(adjustForStickyCloseOut(s), idx),
                              Handle<YieldTermStructure>(), "default");
}

std::size_t GaussianCamCG::numeraire(const Date& s) const {
    auto cam(cam_);
    Size cpidx = currencyPositionInCam_[0];
    LgmCG lgmcg(
        currencies_[0], *g_, [cam, cpidx] { return cam->irlgm1f(cpidx); }, modelParameters_, derivedModelParameters_);
    return lgmcg.numeraire(s, getInterpolatedIrState(adjustForStickyCloseOut(s), 0), Handle<YieldTermStructure>(),
                           "default");
}

std::size_t GaussianCamCG::getFxSpot(const Size idx) const {
    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::logFxSpot, currencies_[idx + 1]);
    auto c = fxSpots_.at(idx);
    return cg_exp(*g_, addModelParameter(id, [c] { return c->value(); }));
}

Real GaussianCamCG::getDirectFxSpotT0(const std::string& forCcy, const std::string& domCcy) const {
    auto c1 = std::find(currencies_.begin(), currencies_.end(), forCcy);
    auto c2 = std::find(currencies_.begin(), currencies_.end(), domCcy);
    QL_REQUIRE(c1 != currencies_.end(), "currency " << forCcy << " not handled");
    QL_REQUIRE(c2 != currencies_.end(), "currency " << domCcy << " not handled");
    Size cidx1 = std::distance(currencies_.begin(), c1);
    Size cidx2 = std::distance(currencies_.begin(), c2);
    Real fx = 1.0;
    if (cidx1 > 0)
        fx *= fxSpots_.at(cidx1 - 1)->value();
    if (cidx2 > 0)
        fx /= fxSpots_.at(cidx2 - 1)->value();
    return fx;
}

Real GaussianCamCG::getDirectDiscountT0(const Date& paydate, const std::string& currency) const {
    auto c = std::find(currencies_.begin(), currencies_.end(), currency);
    QL_REQUIRE(c != currencies_.end(), "currency " << currency << " not handled");
    Size cidx = std::distance(currencies_.begin(), c);
    return curves_.at(cidx)->discount(paydate);
}

std::set<std::size_t>
GaussianCamCG::npvRegressors(const Date& obsdate,
                             const std::optional<std::set<std::string>>& relevantCurrencies) const {

    std::optional<std::set<std::string>> effectiveRelevantCurrencies = relevantCurrencies;
    if (effectiveRelevantCurrencies) {
        effectiveRelevantCurrencies->insert(baseCcy());
    }

    std::set<std::size_t> state;

    if (obsdate == referenceDate()) {
        return state;
    }

    if (conditionalExpectationUseAsset_ && !underlyingPaths_.empty()) {
        for (Size i = 0; i < indices_.size(); ++i) {
            if (effectiveRelevantCurrencies && indices_[i].isFx()) {
                if (effectiveRelevantCurrencies->find(indices_[i].fx()->sourceCurrency().code()) ==
                    effectiveRelevantCurrencies->end())
                    continue;
            }
            state.insert(getInterpolatedUnderlyingPath(adjustForStickyCloseOut(obsdate), i));
        }
    }

    // TODO we include zero vol ir states here, we could exclude them
    if (conditionalExpectationUseIr_) {
        for (Size ccy = 0; ccy < currencies_.size(); ++ccy) {
            if (!effectiveRelevantCurrencies ||
                effectiveRelevantCurrencies->find(currencies_[ccy]) != effectiveRelevantCurrencies->end()) {
                state.insert(getInterpolatedIrState(adjustForStickyCloseOut(obsdate), ccy));
            }
        }
    }

    return state;
}

std::size_t GaussianCamCG::npv(const std::size_t amount, const Date& obsdate, const std::size_t filter,
                               const std::optional<long>& memSlot, const std::set<std::size_t> addRegressors,
                               const std::optional<std::set<std::size_t>>& overwriteRegressors) const {

    calculate();

    QL_REQUIRE(!memSlot, "GuassiaCamCG::npv() with memSlot not yet supported!");

    // if obsdate is today, take a plain expectation

    if (obsdate == referenceDate()) {
        return cg_conditionalExpectation(*g_, amount, {}, cg_const(*g_, 1.0));
    }

    // build the state

    std::vector<std::size_t> state;

    if (overwriteRegressors) {
        state.insert(state.end(), overwriteRegressors->begin(), overwriteRegressors->end());
    } else {
        std::set<std::size_t> r = npvRegressors(obsdate, std::nullopt);
        state.insert(state.end(), r.begin(), r.end());
    }

    for (auto const& r : addRegressors)
        if (r != ComputationGraph::nan)
            state.push_back(r);

    // if the state is empty, return the plain expectation (no conditioning)

    if (state.empty()) {
        return cg_conditionalExpectation(*g_, amount, {}, cg_const(*g_, 1.0));
    }

    // if a memSlot is given and coefficients are stored, we use them
    // TODO ...

    // otherwise compute coefficients and store them if a memSlot is given
    // TODO ...

    // compute conditional expectation and return the result

    return cg_conditionalExpectation(*g_, amount, state, filter);
}

void GaussianCamCG::useStickyCloseOutDates(const bool b) const { useStickyCloseOutDates_ = b; }

} // namespace data
} // namespace ore
