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
template <typename T> T getDateValue(const std::map<Date, T>& m, const Date& d, const bool sloppySimDates) {
    auto it = std::lower_bound(m.begin(), m.end(), d,
                               [](const std::pair<Date, T>& d1, const Date& d2) { return d1.first < d2; });
    QL_REQUIRE(sloppySimDates || (it != m.end() && it->first == d),
               "GaussianCamCG::getDateValue(): date " << d << " not found. Exact match required.");
    return it != m.end() ? it->second : std::next(it, -1)->second;
}
} // namespace

GaussianCamCG::GaussianCamCG(
    const Handle<CrossAssetModel>& cam, const Size paths, const std::vector<std::string>& currencies,
    const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, boost::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, boost::shared_ptr<ZeroInflationIndex>>>& infIndices,
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

Size GaussianCamCG::size() const {
    if (injectedPathTimes_ == nullptr)
        return ModelCG::size();
    else {
        return overwriteModelSize_;
    }
}

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
            randomVariates_[j][i] = cg_var(*g_, "__rv_" + std::to_string(j) + "_" + std::to_string(i),
                                           ComputationGraph::VarDoesntExist::Create);
        }
    }

    // evolve the stochastic process, for now we only evolve IR LGM process #0 as a PoC (!)

    auto cam(cam_);

    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        std::string id = "__diffusion_0_" + std::to_string(i);
        Real t = timeGrid_[i];
        Real dt = timeGrid_.dt(i);
        addModelParameter(id,
                          [cam, t, dt] { return cam->stateProcess()->diffusion(t, Array())(0, 0) * std::sqrt(dt); });
    }

    std::string id = "__z_0";
    std::size_t irState;
    irState = addModelParameter(id, [cam] { return cam->stateProcess()->initialValues()[0]; });
    irStates_[*effectiveSimulationDates_.begin()][0] = irState;

    std::size_t dateIndex = 1;
    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        irState = cg_add(*g_, irState,
                         cg_mult(*g_, cg_var(*g_, "__diffusion_0_" + std::to_string(i)),
                                 cg_var(*g_, "__rv_0_" + std::to_string(i))));
        if (positionInTimeGrid_[dateIndex] == i + 1) {
            irStates_[*std::next(effectiveSimulationDates_.begin(), dateIndex)][0] = irState;
            ++dateIndex;
        }
    }
    QL_REQUIRE(dateIndex == effectiveSimulationDates_.size(),
               "GaussianCamCG:internal error, did not populate all irState time steps.");

    // populate path values
}

std::size_t GaussianCamCG::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    QL_FAIL("GaussianCamCG::getIndexValue(): not implemented");
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
        currencies_[currencyIdx], *g_, [cam, currencyIdx] { return cam->irlgm1f(currencyIdx); }, modelParameters_);
    return lgmcg.fixing(irIndices_[indexNo].second, fixingDate, d,
                        getDateValue(irStates_, d, sloppySimDates_).at(currencyIdx));
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
        currencies_[idx], *g_, [cam, cpidx] { return cam->irlgm1f(cpidx); }, modelParameters_);
    return lgmcg.discountBond(s, t, getDateValue(irStates_, s, sloppySimDates_)[idx]);
}

std::size_t GaussianCamCG::getNumeraire(const Date& s) const {
    auto cam(cam_);
    Size cpidx = currencyPositionInCam_[0];
    LgmCG lgmcg(
        currencies_[0], *g_, [cam, cpidx] { return cam->irlgm1f(cpidx); }, modelParameters_);
    return lgmcg.numeraire(s, getDateValue(irStates_, s, sloppySimDates_)[0]);
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

    state.push_back(getDateValue(irStates_, obsdate, sloppySimDates_).at(0));

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
