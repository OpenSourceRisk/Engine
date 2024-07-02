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
    const std::vector<std::string>& conditionalExpectationModelStates, const bool sloppySimDates)
    : ModelCGImpl(curves.front()->dayCounter(), paths, currencies, irIndices, infIndices, indices, indexCurrencies,
                  simulationDates, iborFallbackConfig),
      cam_(cam), curves_(curves), fxSpots_(fxSpots), timeStepsPerYear_(timeStepsPerYear),
      projectedStateProcessIndices_(projectedStateProcessIndices), sloppySimDates_(sloppySimDates) {

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
            sqrtCorrelation[i][j] =
                addModelParameter("__cam_sqrtCorr_" + std::to_string(i) + "_" + std::to_string(j),
                                  [cam, i, j] { return cam->stateProcess()->sqrtCorrelation()(i, j); });
        }
    }

    // precompute diffusion on correlated brownians nodes

    std::vector<std::vector<std::vector<std::size_t>>> diffusionOnCorrelatedBrownians(
        timeGrid_.size() - 1, std::vector<std::vector<std::size_t>>(
                                  cam->dimension(), std::vector<std::size_t>(cam->brownians(), cg_const(*g_, 0.0))));

    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        std::string dateStr = ore::data::to_string(*std::next(effectiveSimulationDates_.begin(), i));
        Real t = timeGrid_[i];
        for (Size j = 0; j < cam->components(CrossAssetModel::AssetType::IR); ++j) {
            std::size_t alpha = addModelParameter("__lgm_" + currencies_[j] + "_alpha_" + dateStr,
                                                  [cam, j, t] { return cam->irlgm1f(j)->alpha(t); });
            setValue2(diffusionOnCorrelatedBrownians[i], alpha, *cam, CrossAssetModel::AssetType::IR, j,
                      CrossAssetModel::AssetType::IR, j, 0, 0);
        }
        for (Size j = 0; j < cam->components(CrossAssetModel::AssetType::FX); ++j) {
            std::size_t sigma = addModelParameter("__fxbs_" + currencies_[j + 1] + "_sigma_" + dateStr,
                                                  [cam, j, t] { return cam->fxbs(j)->sigma(t); });
            setValue2(diffusionOnCorrelatedBrownians[i], sigma, *cam, CrossAssetModel::AssetType::FX, j,
                      CrossAssetModel::AssetType::FX, j, 0, 0);
        }
        if (cam->measure() == IrModel::Measure::BA) {
            std::size_t H0 = addModelParameter("__lgm_" + currencies_[0] + "_H_" + dateStr,
                                               [cam, t] { return cam->irlgm1f(0)->H(t); });
            std::size_t alpha0 = addModelParameter("__lgm_" + currencies_[0] + "_alpha_" + dateStr,
                                                   [cam, t] { return cam->irlgm1f(0)->alpha(t); });
            setValue2(diffusionOnCorrelatedBrownians[i], cg_mult(*g_, alpha0, H0), *cam, CrossAssetModel::AssetType::IR,
                      0, CrossAssetModel::AssetType::IR, 0, 1, 0);
        }
    }

    // precompute drift nodes (state independent parts)

    std::vector<std::vector<std::size_t>> drift(timeGrid_.size() - 1,
                                                std::vector<std::size_t>(cam->dimension(), cg_const(*g_, 0.0)));
    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        std::string dateStr = ore::data::to_string(*std::next(effectiveSimulationDates_.begin(), i));
        Real t = timeGrid_[i];
        std::size_t H0 =
            addModelParameter("__lgm_" + currencies_[0] + "_H_" + dateStr, [cam, t] { return cam->irlgm1f(0)->H(t); });
        std::size_t alpha0 = addModelParameter("__lgm_" + currencies_[0] + "_alpha_" + dateStr,
                                               [cam, t] { return cam->irlgm1f(0)->alpha(t); });
        for (Size j = 0; j < cam->components(CrossAssetModel::AssetType::IR); ++j) {
            std::size_t H = addModelParameter("__lgm_" + currencies_[j] + "_H_" + dateStr,
                                              [cam, t, j] { return cam->irlgm1f(j)->H(t); });
            std::size_t alpha = addModelParameter("__lgm_" + currencies_[j] + "_alpha_" + dateStr,
                                                  [cam, t, j] { return cam->irlgm1f(j)->alpha(t); });
            if (j == 0) {
                if (cam->measure() == IrModel::Measure::BA) {
                    drift[i][cam->pIdx(CrossAssetModel::AssetType::IR, i, 0)] =
                        cg_negative(*g_, cg_mult(*g_, cg_mult(*g_, H, alpha), alpha));
                }
            } else {
                std::size_t sigma = addModelParameter("__fxbs_" + currencies_[j - 1] + "_sigma_" + dateStr,
                                                      [cam, t, j] { return cam->fxbs(j - 1)->sigma(t); });
                std::size_t rhozz0j = addModelParameter("__cam_corr_zz_0_" + std::to_string(j), [cam, j] {
                    return cam->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::IR, j);
                });
                std::size_t rhozx0j = addModelParameter("__cam_corr_zx_0_" + std::to_string(j), [cam, j] {
                    return cam->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::FX, j - 1);
                });
                std::size_t rhozxjj =
                    addModelParameter("__cam_corr_zx_" + std::to_string(j) + "_" + std::to_string(j), [cam, j] {
                        return cam->correlation(CrossAssetModel::AssetType::IR, j, CrossAssetModel::AssetType::FX,
                                                j - 1);
                    });
                drift[i][cam->pIdx(CrossAssetModel::AssetType::IR, j, 0)] =
                    cg_add(*g_, {cg_negative(*g_, cg_mult(*g_, cg_mult(*g_, H, alpha), alpha)),
                                 cg_mult(*g_, cg_mult(*g_, cg_mult(*g_, H0, alpha0), alpha), rhozz0j),
                                 cg_mult(*g_, cg_mult(*g_, sigma, alpha), rhozxjj)});
                std::size_t fwd0 = addModelParameter("__lgm_" + currencies_[0] + "_fwd_" + dateStr, [cam, t] {
                    return cam->irlgm1f(0)->termStructure()->forwardRate(t, t, Continuous);
                });
                std::size_t fwdj = addModelParameter("__lgm_" + currencies_[j] + "_fwd_" + dateStr, [cam, j, t] {
                    return cam->irlgm1f(j)->termStructure()->forwardRate(t, t, Continuous);
                });
                drift[i][cam->pIdx(CrossAssetModel::AssetType::FX, i - 1, 0)] =
                    cg_add(*g_, {cg_mult(*g_, cg_mult(*g_, cg_mult(*g_, H0, alpha0), sigma), rhozx0j), fwd0,
                                 cg_negative(*g_, fwdj), cg_mult(*g_, cg_mult(*g_, cg_const(*g_, 0.5), sigma), sigma)});
                if (cam->measure() == IrModel::Measure::BA) {
                    drift[i][cam->pIdx(CrossAssetModel::AssetType::IR, i, 0)] =
                        cg_subtract(*g_, drift[i][cam->pIdx(CrossAssetModel::AssetType::IR, i, 0)],
                                    cg_mult(*g_, cg_mult(*g_, cg_mult(*g_, H0, alpha0), alpha), rhozz0j));
                    drift[i][cam->pIdx(CrossAssetModel::AssetType::FX, i - 1, 0)] =
                        cg_subtract(*g_, drift[i][cam->pIdx(CrossAssetModel::AssetType::FX, i - 1, 0)],
                                    cg_mult(*g_, cg_mult(*g_, cg_mult(*g_, H0, alpha0), sigma), rhozx0j));
                }
            }
        }
    }

    // initialize state vector

    std::vector<std::size_t> state(cam->dimension(), cg_const(*g_, 0.0));
    std::string dateStr0 = ore::data::to_string(*effectiveSimulationDates_.begin());

    for (Size j = 0; j < cam->components(CrossAssetModel::AssetType::FX); ++j) {
        state[cam->pIdx(CrossAssetModel::AssetType::FX, j, 0)] =
            addModelParameter("__fxbs_" + currencies_[j + 1] + "_" + dateStr0,
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

        std::string dateStr = ore::data::to_string(*std::next(effectiveSimulationDates_.begin(), i));
        Real t = timeGrid_[i];

        for (Size j = 1; j < cam->components(CrossAssetModel::AssetType::IR); ++j) {
            std::size_t H = addModelParameter("__lgm_" + currencies_[j] + "_H_" + dateStr,
                                              [cam, t, j] { return cam->irlgm1f(j)->H(t); });
            std::size_t H0 = addModelParameter("__lgm_" + currencies_[0] + "_H_" + dateStr,
                                               [cam, t] { return cam->irlgm1f(0)->H(t); });
            std::size_t Hprime = addModelParameter("__lgm_" + currencies_[j] + "_Hprime_" + dateStr,
                                                   [cam, t, j] { return cam->irlgm1f(j)->Hprime(t); });
            std::size_t Hprime0 = addModelParameter("__lgm_" + currencies_[0] + "_Hprime_" + dateStr,
                                                    [cam, t] { return cam->irlgm1f(0)->Hprime(t); });
            std::size_t zeta = addModelParameter("__lgm_" + currencies_[j] + "_zeta_" + dateStr,
                                                 [cam, t, j] { return cam->irlgm1f(j)->zeta(t); });
            std::size_t zeta0 = addModelParameter("__lgm_" + currencies_[0] + "_zeta_" + dateStr,
                                                  [cam, t] { return cam->irlgm1f(0)->zeta(t); });
            drift2[cam->pIdx(CrossAssetModel::AssetType::FX, i - 1, 0)] = cg_add(
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

std::size_t GaussianCamCG::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    QL_REQUIRE(fwd == Null<Date>(), "GaussianCamCG::getIndexValue(): fwd != null not implemented ("
                                        << indexNo << "," << d << "," << fwd << ")");
    return underlyingPaths_.at(d).at(indexNo);
}

std::size_t GaussianCamCG::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date fixingDate = d;
    if (fwd != Null<Date>())
        fixingDate = fwd;
    // ensure a valid fixing date
    fixingDate = irIndices_[indexNo].second->fixingCalendar().adjust(fixingDate);
    Size currencyIdx = irIndexPositionInCam_[indexNo];
    auto cam(cam_);
    Date sd = getSloppyDate(d, sloppySimDates_, effectiveSimulationDates_);
    LgmCG lgmcg(
        currencies_[currencyIdx], *g_, [cam, currencyIdx] { return cam->irlgm1f(currencyIdx); }, modelParameters_,
        sloppySimDates_, effectiveSimulationDates_);
    return lgmcg.fixing(irIndices_[indexNo].second, fixingDate, sd, irStates_.at(sd).at(currencyIdx));
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
    Date sd = getSloppyDate(s, sloppySimDates_, effectiveSimulationDates_);
    LgmCG lgmcg(
        currencies_[idx], *g_, [cam, cpidx] { return cam->irlgm1f(cpidx); }, modelParameters_, sloppySimDates_,
        effectiveSimulationDates_);
    return lgmcg.discountBond(sd, t, irStates_.at(sd)[idx]);
}

std::size_t GaussianCamCG::getNumeraire(const Date& s) const {
    auto cam(cam_);
    Size cpidx = currencyPositionInCam_[0];
    Date sd = getSloppyDate(s, sloppySimDates_, effectiveSimulationDates_);
    LgmCG lgmcg(
        currencies_[0], *g_, [cam, cpidx] { return cam->irlgm1f(cpidx); }, modelParameters_, sloppySimDates_,
        effectiveSimulationDates_);
    return lgmcg.numeraire(sd, irStates_.at(sd)[0]);
}

std::size_t GaussianCamCG::getFxSpot(const Size idx) const {
    std::string id = "__fxspot_" + std::to_string(idx);
    auto c = fxSpots_.at(idx);
    return addModelParameter(id, [c] { return c->value(); });
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

std::size_t GaussianCamCG::npv(const std::size_t amount, const Date& obsdate, const std::size_t filter,
                               const boost::optional<long>& memSlot, const std::size_t addRegressor1,
                               const std::size_t addRegressor2) const {

    calculate();

    QL_REQUIRE(!memSlot, "GuassiaCamCG::npv() with memSlot not yet supported!");

    // if obsdate is today, take a plain expectation

    if (obsdate == referenceDate()) {
        return cg_conditionalExpectation(*g_, amount, {}, cg_const(*g_, 1.0));
    }

    // build the state

    std::vector<std::size_t> state;

    Date sd = getSloppyDate(obsdate, sloppySimDates_, effectiveSimulationDates_);
    state.push_back(irStates_.at(sd).at(0));

    if (addRegressor1 != ComputationGraph::nan)
        state.push_back(addRegressor1);
    if (addRegressor2 != ComputationGraph::nan)
        state.push_back(addRegressor2);

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

} // namespace data
} // namespace ore
