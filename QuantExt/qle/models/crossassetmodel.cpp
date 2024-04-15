/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/models/crossassetanalytics.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/hwmodel.hpp>
#include <qle/models/pseudoparameter.hpp>
#include <qle/utilities/inflation.hpp>

#include <ql/experimental/math/piecewiseintegral.hpp>
#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/processes/eulerdiscretization.hpp>

using namespace QuantExt::CrossAssetAnalytics;
using std::map;
using std::vector;

namespace QuantExt {

std::ostream& operator<<(std::ostream& out, const CrossAssetModel::AssetType& type) {
    switch (type) {
    case CrossAssetModel::AssetType::IR:
        return out << "IR";
    case CrossAssetModel::AssetType::FX:
        return out << "FX";
    case CrossAssetModel::AssetType::INF:
        return out << "INF";
    case CrossAssetModel::AssetType::CR:
        return out << "CR";
    case CrossAssetModel::AssetType::EQ:
        return out << "EQ";
    case CrossAssetModel::AssetType::COM:
        return out << "COM";
    case CrossAssetModel::AssetType::CrState:
        return out << "CrState";
    default:
        QL_FAIL("Did not recognise cross asset model type " << static_cast<int>(type) << ".");
    }
}

namespace {

/* derive marginal model discretizations from cam discretization
   - "cam / Euler" should always map to "marginal model / Euler"
   - "cam / Exact" should always map to "marginal model / Exact" which is only possible for a subset of models
   - "cam / BestMarginalDiscretization" is to combine a global Euler scheme with the "best" marginal
     scheme that is available, e.g. QuadraticExponentialMartingale for a Heston component */

HwModel::Discretization getHwDiscretization(CrossAssetModel::Discretization discretization) {
    if (discretization == CrossAssetModel::Discretization::Euler)
        return HwModel::Discretization::Euler;
    else
        return HwModel::Discretization::Exact;
}

LinearGaussMarkovModel::Discretization getLgm1fDiscretization(CrossAssetModel::Discretization discretization) {
    if (discretization == CrossAssetModel::Discretization::Euler)
        return LinearGaussMarkovModel::Discretization::Euler;
    else
        return LinearGaussMarkovModel::Discretization::Exact;
}

CommoditySchwartzModel::Discretization getComSchwartzDiscretization(CrossAssetModel::Discretization discretization) {
    if (discretization == CrossAssetModel::Discretization::Euler)
        return CommoditySchwartzModel::Discretization::Euler;
    else
        return CommoditySchwartzModel::Discretization::Exact;
}
} // namespace

CrossAssetModel::CrossAssetModel(const std::vector<QuantLib::ext::shared_ptr<Parametrization>>& parametrizations,
                                 const Matrix& correlation, const SalvagingAlgorithm::Type salvaging,
                                 const IrModel::Measure measure, const Discretization discretization)
    : LinkableCalibratedModel(), p_(parametrizations), rho_(correlation), salvaging_(salvaging), measure_(measure),
      discretization_(discretization) {
    initialize();
}

CrossAssetModel::CrossAssetModel(const std::vector<QuantLib::ext::shared_ptr<IrModel>>& currencyModels,
                                 const std::vector<QuantLib::ext::shared_ptr<FxBsParametrization>>& fxParametrizations,
                                 const Matrix& correlation, const SalvagingAlgorithm::Type salvaging,
                                 const IrModel::Measure measure, const Discretization discretization)
    : LinkableCalibratedModel(), irModels_(currencyModels), rho_(correlation), salvaging_(salvaging), measure_(measure),
      discretization_(discretization) {
    for (Size i = 0; i < currencyModels.size(); ++i) {
        p_.push_back(currencyModels[i]->parametrizationBase());
    }
    for (Size i = 0; i < fxParametrizations.size(); ++i) {
        p_.push_back(fxParametrizations[i]);
    }
    initialize();
}

QuantLib::ext::shared_ptr<CrossAssetStateProcess> CrossAssetModel::stateProcess() const {
    if (stateProcess_ == nullptr) {
        stateProcess_ = QuantLib::ext::make_shared<CrossAssetStateProcess>(shared_from_this());
    }
    return stateProcess_;
}

Size CrossAssetModel::components(const AssetType t) const { return components_[(Size)t]; }

Size CrossAssetModel::ccyIndex(const Currency& ccy) const {
    Size i = 0;
    while (i < components(CrossAssetModel::AssetType::IR) && ir(i)->currency() != ccy)
        ++i;
    QL_REQUIRE(i < components(CrossAssetModel::AssetType::IR),
               "currency " << ccy.code() << " not present in cross asset model");
    return i;
}

Size CrossAssetModel::eqIndex(const std::string& name) const {
    Size i = 0;
    while (i < components(CrossAssetModel::AssetType::EQ) && eq(i)->name() != name)
        ++i;
    QL_REQUIRE(i < components(CrossAssetModel::AssetType::EQ),
               "equity name " << name << " not present in cross asset model");
    return i;
}

Size CrossAssetModel::comIndex(const std::string& name) const {
    Size i = 0;
    while (i < components(CrossAssetModel::AssetType::COM) && com(i)->name() != name)
        ++i;
    QL_REQUIRE(i < components(CrossAssetModel::AssetType::COM),
               "commodity name " << name << " not present in cross asset model");
    return i;
}

Size CrossAssetModel::infIndex(const std::string& index) const {
    Size i = 0;
    while (i < components(CrossAssetModel::AssetType::INF) && inf(i)->name() != index)
        ++i;
    QL_REQUIRE(i < components(CrossAssetModel::AssetType::INF),
               "inflation index " << index << " not present in cross asset model");
    return i;
}

Size CrossAssetModel::crName(const std::string& name) const {
    Size i = 0;
    while (i < components(CrossAssetModel::AssetType::CR) && cr(i)->name() != name)
        ++i;
    QL_REQUIRE(i < components(CrossAssetModel::AssetType::INF),
               "credit name " << name << " not present in cross asset model");
    return i;
}

void CrossAssetModel::update() {
    cache_crlgm1fS_.clear();
    cache_infdkI_.clear();
    for (Size i = 0; i < p_.size(); ++i) {
        p_[i]->update();
    }
    stateProcess()->resetCache(0); // disable cache
    notifyObservers();
}

void CrossAssetModel::generateArguments() { update(); }

Size CrossAssetModel::brownians(const AssetType t, const Size i) const {
    QL_REQUIRE(brownians_[(Size)t].size() > i,
               "CrossAssetModel::brownians(): asset class " << t << ", component " << i << " not known.");
    return brownians_[(Size)t][i];
}

Size CrossAssetModel::auxBrownians(const AssetType t, const Size i) const {
    QL_REQUIRE(auxBrownians_[(Size)t].size() > i,
               "CrossAssetModel::auxBrownians(): asset class " << t << ", component " << i << " not known.");
    return auxBrownians_[(Size)t][i];
}

Size CrossAssetModel::stateVariables(const AssetType t, const Size i) const {
    QL_REQUIRE(stateVariables_[(Size)t].size() > i,
               "CrossAssetModel::stateVariables(): asset class " << t << ", component " << i << " not known.");
    return stateVariables_[(Size)t][i];
}

Size CrossAssetModel::arguments(const AssetType t, const Size i) const {
    QL_REQUIRE(numArguments_[(Size)t].size() > i,
               "CrossAssetModel::arguments(): asset class " << t << ", component " << i << " not known.");
    return numArguments_[(Size)t][i];
}

CrossAssetModel::ModelType CrossAssetModel::modelType(const AssetType t, const Size i) const {
    QL_REQUIRE(modelType_[(Size)t].size() > i,
               "CrossAssetModel::modelType(): asset class " << t << ", component " << i << " not known.");
    return modelType_[(Size)t][i];
}

Size CrossAssetModel::idx(const AssetType t, const Size i) const {
    QL_REQUIRE(idx_[(Size)t].size() > i,
               "CrossAssetModel::idx(): asset class " << t << ", component " << i << " not known.");
    return idx_[(Size)t][i];
}

Size CrossAssetModel::cIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < brownians(t, i), "c-offset (" << offset << ") for asset class " << t << " and index " << i
                                                      << " must be in 0..." << brownians(t, i) - 1);
    QL_REQUIRE(cIdx_[(Size)t].size() > i,
               "CrossAssetModel::cIdx(): asset class " << t << ", component " << i << " not known.");
    return cIdx_[(Size)t][i] + offset;
}

Size CrossAssetModel::wIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < brownians(t, i) + auxBrownians(t, i), "c-offset (" << offset << ") for asset class " << t
                                                                           << " and index " << i << " must be in 0..."
                                                                           << brownians(t, i) + auxBrownians(t, i) - 1);
    QL_REQUIRE(wIdx_[(Size)t].size() > i,
               "CrossAssetModel::wIdx(): asset class " << t << ", component " << i << " not known.");
    return wIdx_[(Size)t][i] + offset;
}

Size CrossAssetModel::pIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < stateVariables(t, i), "p-offset (" << offset << ") for asset class " << t << " and index " << i
                                                           << " must be in 0..." << stateVariables(t, i) - 1);
    QL_REQUIRE(pIdx_[(Size)t].size() > i,
               "CrossAssetModel::pIdx(): asset class " << t << ", component " << i << " not known.");
    return pIdx_[(Size)t][i] + offset;
}

Size CrossAssetModel::aIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < arguments(t, i), "a-offset (" << offset << ") for asset class " << t << " and index " << i
                                                      << " must be in 0..." << arguments(t, i) - 1);
    QL_REQUIRE(aIdx_[(Size)t].size() > i,
               "CrossAssetModel::aIdx(): asset class " << t << ", component " << i << " not known.");
    return aIdx_[(Size)t][i];
}

Real CrossAssetModel::correlation(const AssetType s, const Size i, const AssetType t, const Size j, const Size iOffset,
                                  const Size jOffset) const {
    return rho_(cIdx(s, i, iOffset), cIdx(t, j, jOffset));
}

void CrossAssetModel::setCorrelation(const AssetType s, const Size i, const AssetType t, const Size j, const Real value,
                                     const Size iOffset, const Size jOffset) {
    Size row = cIdx(s, i, iOffset);
    Size column = cIdx(t, j, jOffset);
    QL_REQUIRE(row != column || close_enough(value, 1.0), "correlation must be 1 at (" << row << "," << column << ")");
    QL_REQUIRE(value >= -1.0 && value <= 1.0, "correlation must be in [-1,1] at (" << row << "," << column << ")");
    // we can not check for non-negative eigenvalues, since we do not
    // know when the correlation matrix setup is finished, but this
    // is effectively one in the state process later on anyway and
    // the user can also use checkCorrelationMatrix() to verify this
    rho_(row, column) = rho_(column, row) = value;
    update();
}

void CrossAssetModel::initialize() {
    initializeParametrizations();
    initializeCorrelation();
    initializeArguments();
    finalizeArguments();
    checkModelConsistency();
    initDefaultIntegrator();
}

void CrossAssetModel::initDefaultIntegrator() {
    setIntegrationPolicy(QuantLib::ext::make_shared<SimpsonIntegral>(1.0E-8, 100), true);
}

void CrossAssetModel::setIntegrationPolicy(const QuantLib::ext::shared_ptr<Integrator> integrator,
                                           const bool usePiecewiseIntegration) const {

    if (!usePiecewiseIntegration) {
        integrator_ = integrator;
        return;
    }

    // collect relevant times from parametrizations, we don't have to sort them or make them unique,
    // this is all done in PiecewiseIntegral for us

    std::vector<Time> allTimes;
    for (Size i = 0; i < p_.size(); ++i) {
        for (Size j = 0; j < getNumberOfParameters(i); ++j)
            allTimes.insert(allTimes.end(), p_[i]->parameterTimes(j).begin(), p_[i]->parameterTimes(j).end());
    }

    // use piecewise integrator avoiding the step points
    integrator_ = QuantLib::ext::make_shared<PiecewiseIntegral>(integrator, allTimes, true);
}

std::pair<CrossAssetModel::AssetType, CrossAssetModel::ModelType>
CrossAssetModel::getComponentType(const Size i) const {
    if (QuantLib::ext::dynamic_pointer_cast<IrHwParametrization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::IR, CrossAssetModel::ModelType::HW);
    if (QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::IR, CrossAssetModel::ModelType::LGM1F);
    if (QuantLib::ext::dynamic_pointer_cast<FxBsParametrization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::FX, CrossAssetModel::ModelType::BS);
    if (QuantLib::ext::dynamic_pointer_cast<InfDkParametrization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::INF, CrossAssetModel::ModelType::DK);
    if (QuantLib::ext::dynamic_pointer_cast<InfJyParameterization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::INF, CrossAssetModel::ModelType::JY);
    if (QuantLib::ext::dynamic_pointer_cast<CrLgm1fParametrization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::CR, CrossAssetModel::ModelType::LGM1F);
    if (QuantLib::ext::dynamic_pointer_cast<CrCirppParametrization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::CR, CrossAssetModel::ModelType::CIRPP);
    if (QuantLib::ext::dynamic_pointer_cast<EqBsParametrization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::EQ, CrossAssetModel::ModelType::BS);
    if (QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzParametrization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::COM, CrossAssetModel::ModelType::BS);
    if (QuantLib::ext::dynamic_pointer_cast<CrStateParametrization>(p_[i]))
        return std::make_pair(CrossAssetModel::AssetType::CrState, CrossAssetModel::ModelType::GENERIC);
    QL_FAIL("parametrization " << i << " has unknown type");
}

Size CrossAssetModel::getNumberOfParameters(const Size i) const { return p_[i]->numberOfParameters(); }

Size CrossAssetModel::getNumberOfBrownians(const Size i) const {
    if (auto p = QuantLib::ext::dynamic_pointer_cast<IrHwParametrization>(p_[i])) {
        return p->m();
    }
    if (QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i])) {
        return 1;
    }
    if (QuantLib::ext::dynamic_pointer_cast<FxBsParametrization>(p_[i]))
        return 1;
    if (QuantLib::ext::dynamic_pointer_cast<InfDkParametrization>(p_[i]))
        return 1;
    if (QuantLib::ext::dynamic_pointer_cast<InfJyParameterization>(p_[i]))
        return 2;
    if (QuantLib::ext::dynamic_pointer_cast<CrLgm1fParametrization>(p_[i]))
        return 1;
    if (QuantLib::ext::dynamic_pointer_cast<CrCirppParametrization>(p_[i]))
        return 1;
    if (QuantLib::ext::dynamic_pointer_cast<EqBsParametrization>(p_[i]))
        return 1;
    if (QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzParametrization>(p_[i]))
        return 1;
    if (QuantLib::ext::dynamic_pointer_cast<CrStateParametrization>(p_[i]))
        return 1;
    QL_FAIL("parametrization " << i << " has unknown type");
}

Size CrossAssetModel::getNumberOfAuxBrownians(const Size i) const {
    if (auto p = QuantLib::ext::dynamic_pointer_cast<IrHwParametrization>(p_[i])) {
        return HwModel(p, measure_, getHwDiscretization(discretization_), i == 0).m_aux();
    }
    if (auto p = QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i])) {
        return LGM(p, measure_, getLgm1fDiscretization(discretization_), i == 0).m_aux();
    }
    if (QuantLib::ext::dynamic_pointer_cast<FxBsParametrization>(p_[i]))
        return 0;
    if (QuantLib::ext::dynamic_pointer_cast<InfDkParametrization>(p_[i]))
        return discretization_ == Discretization::Exact ? 1 : 0;
    if (QuantLib::ext::dynamic_pointer_cast<InfJyParameterization>(p_[i]))
        return 0;
    if (QuantLib::ext::dynamic_pointer_cast<CrLgm1fParametrization>(p_[i]))
        return discretization_ == Discretization::Exact ? 1 : 0;
    if (QuantLib::ext::dynamic_pointer_cast<CrCirppParametrization>(p_[i]))
        return 0;
    if (QuantLib::ext::dynamic_pointer_cast<EqBsParametrization>(p_[i]))
        return 0;
    if (QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzParametrization>(p_[i]))
        return 0;
    if (QuantLib::ext::dynamic_pointer_cast<CrStateParametrization>(p_[i]))
        return 0;
    QL_FAIL("parametrization " << i << " has unknown type");
}

Size CrossAssetModel::getNumberOfStateVariables(const Size i) const {
    if (auto p = QuantLib::ext::dynamic_pointer_cast<IrHwParametrization>(p_[i])) {
        HwModel m(p, measure_, getHwDiscretization(discretization_), i == 0);
        return m.n() + m.n_aux();
    }
    if (auto p = QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i])) {
        LGM m(p, measure_, getLgm1fDiscretization(discretization_), i == 0);
        return m.n() + m.n_aux();
    }
    if (QuantLib::ext::dynamic_pointer_cast<FxBsParametrization>(p_[i]))
        return 1;
    if (QuantLib::ext::dynamic_pointer_cast<InfDkParametrization>(p_[i]))
        return 2;
    if (QuantLib::ext::dynamic_pointer_cast<InfJyParameterization>(p_[i]))
        return 2;
    if (QuantLib::ext::dynamic_pointer_cast<CrLgm1fParametrization>(p_[i]))
        return 2;
    if (QuantLib::ext::dynamic_pointer_cast<CrCirppParametrization>(p_[i]))
        return 2;
    if (QuantLib::ext::dynamic_pointer_cast<EqBsParametrization>(p_[i]))
        return 1;
    if (QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzParametrization>(p_[i]))
        return 1;
    if (QuantLib::ext::dynamic_pointer_cast<CrStateParametrization>(p_[i]))
        return 1;
    QL_FAIL("parametrization " << i << " has unknown type");
}

void CrossAssetModel::updateIndices(const AssetType& t, const Size i, const Size cIdx, const Size wIdx, const Size pIdx,
                                    const Size aIdx) {
    idx_[(Size)t].push_back(i);
    modelType_[(Size)t].push_back(getComponentType(i).second);
    brownians_[(Size)t].push_back(getNumberOfBrownians(i));
    auxBrownians_[(Size)t].push_back(getNumberOfAuxBrownians(i));
    stateVariables_[(Size)t].push_back(getNumberOfStateVariables(i));
    numArguments_[(Size)t].push_back(getNumberOfParameters(i));
    cIdx_[(Size)t].push_back(cIdx);
    wIdx_[(Size)t].push_back(wIdx);
    pIdx_[(Size)t].push_back(pIdx);
    aIdx_[(Size)t].push_back(aIdx);
    if (discretization_ == Discretization::Euler) {
        QL_REQUIRE(wIdx_[(Size)t].back() == cIdx_[(Size)t].back(),
                   "CrossAssetModel::updateIndices(): assertion error, wIdx ("
                       << wIdx_[(Size)t].back() << ") != cIdx (" << cIdx_[(Size)t].back() << ") for asset type " << t
                       << " at index " << wIdx_[(Size)t].size() << " for Euler discretization");
    } else {
        QL_REQUIRE(wIdx_[(Size)t].back() == pIdx_[(Size)t].back(),
                   "CrossAssetModel::updateIndices(): assertion error, wIdx ("
                       << wIdx_[(Size)t].back() << ") != pIdx (" << pIdx_[(Size)t].back() << ") for asset type " << t
                       << " at index " << wIdx_[(Size)t].size() << " for Exact discretization");
    }
}

void CrossAssetModel::initializeParametrizations() {

    // count the parametrizations and check their order and their support

    Size i = 0, j;
    Size cIdxTmp = 0, wIdxTmp = 0, pIdxTmp = 0, aIdxTmp = 0;
    components_.resize(numberOfAssetTypes, 0);
    idx_.resize(numberOfAssetTypes);
    cIdx_.resize(numberOfAssetTypes);
    wIdx_.resize(numberOfAssetTypes);
    pIdx_.resize(numberOfAssetTypes);
    aIdx_.resize(numberOfAssetTypes);
    brownians_.resize(numberOfAssetTypes);
    auxBrownians_.resize(numberOfAssetTypes);
    stateVariables_.resize(numberOfAssetTypes);
    numArguments_.resize(numberOfAssetTypes);
    modelType_.resize(numberOfAssetTypes);

    // IR parametrizations

    bool genericCtor = irModels_.empty();
    j = 0;
    while (i < p_.size() && getComponentType(i).first == CrossAssetModel::AssetType::IR) {
        QL_REQUIRE(j == 0 || getComponentType(i).second == getComponentType(0).second,
                   "All IR models must be of the same type (HW, LGM can not be mixed)");
        // initialize ir model, if generic constructor was used
        // evaluate bank account for j = 0 (domestic process
        if (genericCtor) {
            if (getComponentType(i).second == ModelType::LGM1F) {
                irModels_.push_back(QuantLib::ext::make_shared<LinearGaussMarkovModel>(
                    QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i]), measure_,
                    getLgm1fDiscretization(discretization_), j == 0));
            } else if (getComponentType(i).second == ModelType::HW) {
                irModels_.push_back(QuantLib::ext::make_shared<HwModel>(QuantLib::ext::dynamic_pointer_cast<IrHwParametrization>(p_[i]),
                                                                measure_, getHwDiscretization(discretization_),
                                                                j == 0));
            } else {
                irModels_.push_back(nullptr);
            }
        }
        updateIndices(CrossAssetModel::AssetType::IR, i, cIdxTmp, wIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        wIdxTmp += getNumberOfBrownians(i) + getNumberOfAuxBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
    }
    components_[(Size)CrossAssetModel::AssetType::IR] = j;

    // FX parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == CrossAssetModel::AssetType::FX) {
        fxModels_.push_back(QuantLib::ext::make_shared<FxBsModel>(QuantLib::ext::dynamic_pointer_cast<FxBsParametrization>(p_[i])));
        updateIndices(CrossAssetModel::AssetType::FX, i, cIdxTmp, wIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        wIdxTmp += getNumberOfBrownians(i) + getNumberOfAuxBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
    }
    components_[(Size)CrossAssetModel::AssetType::FX] = j;

    QL_REQUIRE(components_[(Size)CrossAssetModel::AssetType::IR] > 0, "at least one ir parametrization must be given");

    QL_REQUIRE(components_[(Size)CrossAssetModel::AssetType::FX] ==
                   components_[(Size)CrossAssetModel::AssetType::IR] - 1,
               "there must be n-1 fx "
               "for n ir parametrizations, found "
                   << components_[(Size)CrossAssetModel::AssetType::IR] << " ir and "
                   << components_[(Size)CrossAssetModel::AssetType::FX] << " fx parametrizations");

    // check currencies

    // without an order or a hash function on Currency this seems hard
    // to do in a simpler way ...
    Size uniqueCurrencies = 0;
    std::vector<Currency> currencies;
    for (Size i = 0; i < components_[(Size)CrossAssetModel::AssetType::IR]; ++i) {
        Size tmp = 1;
        for (Size j = 0; j < i; ++j) {
            if (ir(i)->currency() == currencies[j])
                tmp = 0;
        }
        uniqueCurrencies += tmp;
        currencies.push_back(ir(i)->currency());
    }
    QL_REQUIRE(uniqueCurrencies == components_[(Size)CrossAssetModel::AssetType::IR], "there are duplicate currencies "
                                                                                      "in the set of ir "
                                                                                      "parametrizations");
    for (Size i = 0; i < components_[(Size)CrossAssetModel::AssetType::FX]; ++i) {
        QL_REQUIRE(fx(i)->currency() == ir(i + 1)->currency(),
                   "fx parametrization #" << i << " must be for currency of ir parametrization #" << (i + 1)
                                          << ", but they are " << fx(i)->currency() << " and " << ir(i + 1)->currency()
                                          << " respectively");
    }

    // Inf parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == CrossAssetModel::AssetType::INF) {
        updateIndices(CrossAssetModel::AssetType::INF, i, cIdxTmp, wIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        wIdxTmp += getNumberOfBrownians(i) + getNumberOfAuxBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
        // we do not check the currency, if not present among the model's
        // currencies, it will throw below
    }
    components_[(Size)CrossAssetModel::AssetType::INF] = j;

    // Cr parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == CrossAssetModel::AssetType::CR) {

        if (getComponentType(i).second == CrossAssetModel::ModelType::CIRPP) {
            auto tmp = QuantLib::ext::dynamic_pointer_cast<CrCirppParametrization>(p_[i]);
            QL_REQUIRE(tmp, "CrossAssetModelPlus::initializeParametrizations(): expected CrCirppParametrization");
            crcirppModel_.push_back(QuantLib::ext::make_shared<CrCirpp>(tmp));
        } else
            crcirppModel_.push_back(QuantLib::ext::shared_ptr<CrCirpp>());

        updateIndices(CrossAssetModel::AssetType::CR, i, cIdxTmp, wIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        wIdxTmp += getNumberOfBrownians(i) + getNumberOfAuxBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
        // we do not check the currency, if not present among the model's
        // currencies, it will throw below
    }
    components_[(Size)CrossAssetModel::AssetType::CR] = j;

    // Eq parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == CrossAssetModel::AssetType::EQ) {
        updateIndices(CrossAssetModel::AssetType::EQ, i, cIdxTmp, wIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        wIdxTmp += getNumberOfBrownians(i) + getNumberOfAuxBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
    }
    components_[(Size)CrossAssetModel::AssetType::EQ] = j;

    // check the equity currencies to ensure they are covered by CrossAssetModel
    for (Size i = 0; i < components(CrossAssetModel::AssetType::EQ); ++i) {
        Currency eqCcy = eq(i)->currency();
        try {
            Size eqCcyIdx = ccyIndex(eqCcy);
            QL_REQUIRE(eqCcyIdx < components_[(Size)CrossAssetModel::AssetType::IR],
                       "Invalid currency for equity " << eqbs(i)->name());
        } catch (...) {
            QL_FAIL("Invalid currency (" << eqCcy.code() << ") for equity " << eqbs(i)->name());
        }
    }

    // COM parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == CrossAssetModel::AssetType::COM) {
        QuantLib::ext::shared_ptr<CommoditySchwartzParametrization> csp =
            QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzParametrization>(p_[i]);
        QuantLib::ext::shared_ptr<CommoditySchwartzModel> csm =
            csp ? QuantLib::ext::make_shared<CommoditySchwartzModel>(csp, getComSchwartzDiscretization(discretization_))
                : nullptr;
        comModels_.push_back(csm);
        updateIndices(CrossAssetModel::AssetType::COM, i, cIdxTmp, wIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        wIdxTmp += getNumberOfBrownians(i) + getNumberOfAuxBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
    }
    components_[(Size)CrossAssetModel::AssetType::COM] = j;

    // CrState parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == CrossAssetModel::AssetType::CrState) {
        updateIndices(CrossAssetModel::AssetType::CrState, i, cIdxTmp, wIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        wIdxTmp += getNumberOfBrownians(i) + getNumberOfAuxBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
    }
    components_[(Size)CrossAssetModel::AssetType::CrState] = j;

    // check the equity currencies to ensure they are covered by CrossAssetModel
    for (Size i = 0; i < components(CrossAssetModel::AssetType::COM); ++i) {
        Currency comCcy = com(i)->currency();
        try {
            Size comCcyIdx = ccyIndex(comCcy);
            QL_REQUIRE(comCcyIdx < components_[(Size)CrossAssetModel::AssetType::IR],
                       "Invalid currency for commodity " << combs(i)->name());
        } catch (...) {
            QL_FAIL("Invalid currency (" << comCcy.code() << ") for commodity " << combs(i)->name());
        }
    }

    // Summary statistics

    totalDimension_ = pIdxTmp;
    totalNumberOfBrownians_ = cIdxTmp;
    totalNumberOfAuxBrownians_ = wIdxTmp - cIdxTmp;

} // initParametrizations

void CrossAssetModel::initializeCorrelation() {
    Size n = brownians();
    if (rho_.empty()) {
        rho_ = Matrix(n, n, 0.0);
        for (Size i = 0; i < n; ++i)
            rho_(i, i) = 1.0;
        return;
    }
    QL_REQUIRE(rho_.rows() == n && rho_.columns() == n, "correlation matrix is " << rho_.rows() << " x "
                                                                                 << rho_.columns() << " but should be "
                                                                                 << n << " x " << n);
    checkCorrelationMatrix();
}

void CrossAssetModel::checkCorrelationMatrix() const {
    Size n = rho_.rows();
    Size m = rho_.columns();
    QL_REQUIRE(rho_.columns() == n, "correlation matrix (" << n << " x " << m << " must be square");
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < m; ++j) {
            QL_REQUIRE(close_enough(rho_[i][j], rho_[j][i]), "correlation matrix is not symmetric, for (i,j)=("
                                                                 << i << "," << j << ") rho(i,j)=" << rho_[i][j]
                                                                 << " but rho(j,i)=" << rho_[j][i]);
            QL_REQUIRE(close_enough(std::abs(rho_[i][j]), 1.0) || (rho_[i][j] > -1.0 && rho_[i][j] < 1.0),
                       "correlation matrix has invalid entry at (i,j)=(" << i << "," << j << ") equal to "
                                                                         << rho_[i][j]);
        }
        QL_REQUIRE(close_enough(rho_[i][i], 1.0), "correlation matrix must have unit diagonal elements, "
                                                  "but rho(i,i)="
                                                      << rho_[i][i] << " for i=" << i);
    }

    // if we salvage the matrix there is no point in checking for negative eigenvalues prior to that
    if (salvaging_ == SalvagingAlgorithm::None) {
        SymmetricSchurDecomposition ssd(rho_);
        for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
            QL_REQUIRE(ssd.eigenvalues()[i] >= 0.0,
                       "correlation matrix has negative eigenvalue at " << i << " (" << ssd.eigenvalues()[i] << ")");
        }
    }
}

void CrossAssetModel::initializeArguments() {
    for (Size i = 0; i < p_.size(); ++i) {
        for (Size k = 0; k < getNumberOfParameters(i); ++k) {
            arguments_.push_back(p_[i]->parameter(k));
        }
    }
}

void CrossAssetModel::finalizeArguments() {
    totalNumberOfParameters_ = 0;
    for (Size i = 0; i < arguments_.size(); ++i) {
        QL_REQUIRE(arguments_[i] != NULL, "unexpected error: argument " << i << " is null");
        totalNumberOfParameters_ += arguments_[i]->size();
    }
}

void CrossAssetModel::checkModelConsistency() const {
    QL_REQUIRE(components(CrossAssetModel::AssetType::IR) > 0, "at least one IR component must be given");
    QL_REQUIRE(
        components(CrossAssetModel::AssetType::IR) + components(CrossAssetModel::AssetType::FX) +
                components(CrossAssetModel::AssetType::INF) + components(CrossAssetModel::AssetType::CR) +
                components(CrossAssetModel::AssetType::EQ) + components(CrossAssetModel::AssetType::COM) +
                components(CrossAssetModel::AssetType::CrState) ==
            p_.size(),
        "the parametrizations must be given in the following order: ir, "
        "fx, inf, cr, eq, com, found "
            << components(CrossAssetModel::AssetType::IR) << " ir, " << components(CrossAssetModel::AssetType::FX)
            << " bs, " << components(CrossAssetModel::AssetType::INF) << " inf, "
            << components(CrossAssetModel::AssetType::CR) << " cr, " << components(CrossAssetModel::AssetType::EQ)
            << " eq, " << components(CrossAssetModel::AssetType::COM) << " com, "
            << components(CrossAssetModel::AssetType::CrState) << "but there are " << p_.size()
            << " parametrizations given in total");
}

void CrossAssetModel::calibrateIrLgm1fVolatilitiesIterative(
    const Size ccy, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    lgm(ccy)->calibrateVolatilitiesIterative(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateIrLgm1fReversionsIterative(
    const Size ccy, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    lgm(ccy)->calibrateReversionsIterative(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateIrLgm1fGlobal(const Size ccy,
                                             const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers,
                                             OptimizationMethod& method, const EndCriteria& endCriteria,
                                             const Constraint& constraint, const std::vector<Real>& weights) {
    lgm(ccy)->calibrate(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateBsVolatilitiesIterative(
    const AssetType& assetType, const Size idx, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers,
    OptimizationMethod& method, const EndCriteria& endCriteria, const Constraint& constraint,
    const std::vector<Real>& weights) {
    QL_REQUIRE(assetType == CrossAssetModel::AssetType::FX || assetType == CrossAssetModel::AssetType::EQ,
               "Unsupported AssetType for BS calibration");
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(assetType, 0, idx, i));
    }
    update();
}

void CrossAssetModel::calibrateBsVolatilitiesGlobal(
    const AssetType& assetType, const Size aIdx, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers,
    OptimizationMethod& method, const EndCriteria& endCriteria, const Constraint& constraint,
    const std::vector<Real>& weights) {
    QL_REQUIRE(assetType == CrossAssetModel::AssetType::FX || assetType == CrossAssetModel::AssetType::EQ,
               "Unsupported AssetType for BS calibration");
    calibrate(helpers, method, endCriteria, constraint, weights, MoveParameter(assetType, 0, aIdx, Null<Size>()));
    update();
}

void CrossAssetModel::calibrateInfDkVolatilitiesIterative(
    const Size index, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights,
                  MoveParameter(CrossAssetModel::AssetType::INF, 0, index, i));
    }
    update();
}

void CrossAssetModel::calibrateInfDkReversionsIterative(
    const Size index, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights,
                  MoveParameter(CrossAssetModel::AssetType::INF, 1, index, i));
    }
    update();
}

void CrossAssetModel::calibrateInfDkVolatilitiesGlobal(
    const Size index, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    calibrate(helpers, method, endCriteria, constraint, weights,
              MoveParameter(CrossAssetModel::AssetType::INF, 0, index, Null<Size>()));
    update();
}

void CrossAssetModel::calibrateInfDkReversionsGlobal(
    const Size index, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    calibrate(helpers, method, endCriteria, constraint, weights,
              MoveParameter(CrossAssetModel::AssetType::INF, 1, index, Null<Size>()));
    update();
}

void CrossAssetModel::calibrateInfJyGlobal(Size index, const vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& helpers,
                                           OptimizationMethod& method, const EndCriteria& endCriteria,
                                           const map<Size, bool>& toCalibrate, const Constraint& constraint,
                                           const vector<Real>& weights) {

    // Initialise the parameters to move first to get the size.
    vector<bool> fixedParams = MoveParameter(CrossAssetModel::AssetType::INF, 0, index, Null<Size>());
    std::fill(fixedParams.begin(), fixedParams.end(), true);

    // Update fixedParams with parameters that need to be calibrated.
    for (const auto& kv : toCalibrate) {
        if (kv.second) {
            vector<bool> tmp = MoveParameter(CrossAssetModel::AssetType::INF, kv.first, index, Null<Size>());
            std::transform(fixedParams.begin(), fixedParams.end(), tmp.begin(), fixedParams.begin(),
                           std::logical_and<bool>());
        }
    }

    // Perform the calibration
    calibrate(helpers, method, endCriteria, constraint, weights, fixedParams);

    update();
}

void CrossAssetModel::calibrateInfJyIterative(Size mIdx, Size pIdx,
                                              const vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& helpers,
                                              OptimizationMethod& method, const EndCriteria& endCriteria,
                                              const Constraint& constraint, const vector<Real>& weights) {

    for (Size i = 0; i < helpers.size(); ++i) {
        vector<QuantLib::ext::shared_ptr<CalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights,
                  MoveParameter(CrossAssetModel::AssetType::INF, pIdx, mIdx, i));
    }

    update();
}

void CrossAssetModel::calibrateCrLgm1fVolatilitiesIterative(
    const Size index, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights,
                  MoveParameter(CrossAssetModel::AssetType::CR, 0, index, i));
    }
    update();
}

void CrossAssetModel::calibrateCrLgm1fReversionsIterative(
    const Size index, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights,
                  MoveParameter(CrossAssetModel::AssetType::CR, 1, index, i));
    }
    update();
}

std::pair<Real, Real> CrossAssetModel::infdkV(const Size i, const Time t, const Time T) {
    Size ccy = ccyIndex(infdk(i)->currency());
    cache_key k = {i, ccy, t, T};
    boost::unordered_map<cache_key, std::pair<Real, Real>>::const_iterator it = cache_infdkI_.find(k);
    Real V0, V_tilde;

    if (it == cache_infdkI_.end()) {
        V0 = infV(i, ccy, 0, t);
        V_tilde = infV(i, ccy, t, T) - infV(i, ccy, 0, T) + infV(i, ccy, 0, t);
        cache_infdkI_.insert(std::make_pair(k, std::make_pair(V0, V_tilde)));
    } else {
        // take V0 and V_tilde from cache
        V0 = it->second.first;
        V_tilde = it->second.second;
    }
    return std::make_pair(V0, V_tilde);
}

std::pair<Real, Real> CrossAssetModel::infdkI(const Size i, const Time t, const Time T, const Real z, const Real y) {
    QL_REQUIRE(t < T || close_enough(t, T), "infdkI: t (" << t << ") <= T (" << T << ") required");
    Real V0, V_tilde;
    std::pair<Real, Real> Vs = infdkV(i, t, T);
    V0 = Vs.first;
    V_tilde = Vs.second;
    Real Hyt = Hy(i).eval(*this, t);
    Real HyT = Hy(i).eval(*this, T);

    // TODO account for seasonality ...
    // compute final results depending on z and y
    const auto& zts = infdk(i)->termStructure();
    auto dc = irlgm1f(0)->termStructure()->dayCounter();
    bool indexIsInterpolated = true; // FIXME, though in line with the comment below
    Real growth_t = inflationGrowth(zts, t, dc, indexIsInterpolated);
    Real It = growth_t * std::exp(Hyt * z - y - V0);
    Real Itilde_t_T = inflationGrowth(zts, T, dc, indexIsInterpolated) / growth_t * std::exp((HyT - Hyt) * z + V_tilde);
    // concerning interpolation there is an inaccuracy here: if the index
    // is not interpolated, we still simulate the index value as of t
    // (and T), although we should go back to t, T which corresponds to
    // the last actual publication time of the index => is the approximation
    // here in this sense good enough that we can tolerate this?
    return std::make_pair(It, Itilde_t_T);
}

Real CrossAssetModel::infdkYY(const Size i, const Time t, const Time S, const Time T, const Real z, const Real y,
                              const Real irz) {
    Size ccy = ccyIndex(infdk(i)->currency());

    // Set Convexity adjustment set to 1.
    // TODO: Add calculation for DK convexity adjustment
    Real C_tilde = 1;

    Real I_tildeS = infdkI(i, t, S, z, y).second;
    Real I_tildeT = infdkI(i, t, T, z, y).second;
    Real Pn_t_T = lgm(ccy)->discountBond(t, T, irz);

    Real yySwaplet = (I_tildeT / I_tildeS) * Pn_t_T * C_tilde - Pn_t_T;

    return yySwaplet;
}

std::pair<Real, Real> CrossAssetModel::crlgm1fS(const Size i, const Size ccy, const Time t, const Time T, const Real z,
                                                const Real y) const {
    QL_REQUIRE(ccy < components(CrossAssetModel::AssetType::IR),
               "ccy index (" << ccy << ") must be in 0..." << (components(CrossAssetModel::AssetType::IR) - 1));
    QL_REQUIRE(t < T || close_enough(t, T), "crlgm1fS: t (" << t << ") <= T (" << T << ") required");
    QL_REQUIRE(modelType(CrossAssetModel::AssetType::CR, i) == CrossAssetModel::ModelType::LGM1F,
               "model at " << i << " is not CR-LGM1F");
    cache_key k = {i, ccy, t, T};
    boost::unordered_map<cache_key, std::pair<Real, Real>>::const_iterator it = cache_crlgm1fS_.find(k);
    Real V0, V_tilde;
    Real Hlt = Hl(i).eval(*this, t);
    Real HlT = Hl(i).eval(*this, T);

    if (it == cache_crlgm1fS_.end()) {
        // compute V0 and V_tilde
        if (ccy == 0) {
            // domestic credit
            Real Hzt = Hz(0).eval(*this, t);
            Real HzT = Hz(0).eval(*this, T);
            Real zetal0 = zetal(i).eval(*this, t);
            Real zetal1 = integral(*this, P(Hl(i), al(i), al(i)), 0.0, t);
            Real zetal2 = integral(*this, P(Hl(i), Hl(i), al(i), al(i)), 0.0, t);
            Real zetanl0 = integral(*this, P(rzl(0, i), az(0), al(i)), 0.0, t);
            Real zetanl1 = integral(*this, P(rzl(0, i), Hl(i), az(0), al(i)), 0.0, t);
            // opposite signs for last two terms in the book
            V0 = 0.5 * Hlt * Hlt * zetal0 - Hlt * zetal1 + 0.5 * zetal2 + Hzt * Hlt * zetanl0 - Hzt * zetanl1;
            V_tilde = -0.5 * (HlT * HlT - Hlt * Hlt) * zetal0 + (HlT - Hlt) * zetal1 -
                      (HzT * HlT - Hzt * Hlt) * zetanl0 + (HzT - Hzt) * zetanl1;
        } else {
            // foreign credit
            V0 = crV(i, ccy, 0, t);
            V_tilde = crV(i, ccy, t, T) - crV(i, ccy, 0, T) + crV(i, ccy, 0, t);
        }
        cache_crlgm1fS_.insert(std::make_pair(k, std::make_pair(V0, V_tilde)));
    } else {
        // take V0 and V_tilde from cache
        V0 = it->second.first;
        V_tilde = it->second.second;
    }
    // compute final results depending on z and y
    // opposite sign for V0 in the book
    Real St = crlgm1f(i)->termStructure()->survivalProbability(t) * std::exp(-Hlt * z + y - V0);
    Real Stilde_t_T = crlgm1f(i)->termStructure()->survivalProbability(T) /
                      crlgm1f(i)->termStructure()->survivalProbability(t) * std::exp(-(HlT - Hlt) * z + V_tilde);
    return std::make_pair(St, Stilde_t_T);
}

std::pair<Real, Real> CrossAssetModel::crcirppS(const Size i, const Time t, const Time T, const Real y,
                                                const Real s) const {
    QL_REQUIRE(modelType(CrossAssetModel::AssetType::CR, i) == CrossAssetModel::ModelType::CIRPP,
               "model at " << i << " is not CR-CIR");
    if (close_enough(t, T))
        return std::make_pair(s, 1.0);
    else
        return std::make_pair(s, crcirppModel_[i]->survivalProbability(t, T, y));
}

Real CrossAssetModel::infV(const Size i, const Size ccy, const Time t, const Time T) const {
    Real HyT = Hy(i).eval(*this, T);
    Real HdT = irlgm1f(0)->H(T);
    Real rhody = correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::INF, i, 0, 0);
    Real V;
    if (ccy == 0) {
        V = 0.5 * (HyT * HyT * (zetay(i).eval(*this, T) - zetay(i).eval(*this, t)) -
                   2.0 * HyT * integral(*this, P(Hy(i), ay(i), ay(i)), t, T) +
                   integral(*this, P(Hy(i), Hy(i), ay(i), ay(i)), t, T)) -
            rhody * HdT *
                (HyT * integral(*this, P(az(0), ay(i)), t, T) - integral(*this, P(az(0), Hy(i), ay(i)), t, T));
    } else {
        Real HfT = irlgm1f(ccy)->H(T);
        Real rhofy = correlation(CrossAssetModel::AssetType::IR, ccy, CrossAssetModel::AssetType::INF, i, 0, 0);
        Real rhoxy = correlation(CrossAssetModel::AssetType::FX, ccy - 1, CrossAssetModel::AssetType::INF, i, 0, 0);
        V = 0.5 * (HyT * HyT * (zetay(i).eval(*this, T) - zetay(i).eval(*this, t)) -
                   2.0 * HyT * integral(*this, P(Hy(i), ay(i), ay(i)), t, T) +
                   integral(*this, P(Hy(i), Hy(i), ay(i), ay(i)), t, T)) -
            rhody * (HyT * integral(*this, P(Hz(0), az(0), ay(i)), t, T) -
                     integral(*this, P(Hz(0), az(0), Hy(i), ay(i)), t, T)) -
            rhofy * (HfT * HyT * integral(*this, P(az(ccy), ay(i)), t, T) -
                     HfT * integral(*this, P(az(ccy), Hy(i), ay(i)), t, T) -
                     HyT * integral(*this, P(Hz(ccy), az(ccy), ay(i)), t, T) +
                     integral(*this, P(Hz(ccy), az(ccy), Hy(i), ay(i)), t, T)) +
            rhoxy * (HyT * integral(*this, P(sx(ccy - 1), ay(i)), t, T) -
                     integral(*this, P(sx(ccy - 1), Hy(i), ay(i)), t, T));
    }
    return V;
}

Real CrossAssetModel::crV(const Size i, const Size ccy, const Time t, const Time T) const {
    Real HlT = Hl(i).eval(*this, T);
    Real HfT = Hz(ccy).eval(*this, T);
    Real rhodl = correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::CR, i, 0, 0);
    Real rhofl = correlation(CrossAssetModel::AssetType::IR, ccy, CrossAssetModel::AssetType::CR, i, 0, 0);
    Real rhoxl = correlation(CrossAssetModel::AssetType::FX, ccy - 1, CrossAssetModel::AssetType::CR, i, 0, 0);
    return 0.5 * (HlT * HlT * (zetal(i).eval(*this, T) - zetal(i).eval(*this, t)) -
                  2.0 * HlT * integral(*this, P(Hl(i), al(i), al(i)), t, T) +
                  integral(*this, P(Hl(i), Hl(i), al(i), al(i)), t, T)) +
           rhodl * (HlT * integral(*this, P(Hz(0), az(0), al(i)), t, T) -
                    integral(*this, P(Hz(0), az(0), Hl(i), al(i)), t, T)) +
           rhofl * (HfT * HlT * integral(*this, P(az(ccy), al(i)), t, T) -
                    HfT * integral(*this, P(az(ccy), Hl(i), al(i)), t, T) -
                    HlT * integral(*this, P(Hz(ccy), az(ccy), al(i)), t, T) +
                    integral(*this, P(Hz(ccy), az(ccy), Hl(i), al(i)), t, T)) -
           rhoxl * (HlT * integral(*this, P(sx(ccy - 1), al(i)), t, T) -
                    integral(*this, P(sx(ccy - 1), Hl(i), al(i)), t, T));
}

Handle<ZeroInflationTermStructure> inflationTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, Size index) {

    if (model->modelType(CrossAssetModel::AssetType::INF, index) == CrossAssetModel::ModelType::DK) {
        return model->infdk(index)->termStructure();
    } else if (model->modelType(CrossAssetModel::AssetType::INF, index) == CrossAssetModel::ModelType::JY) {
        return model->infjy(index)->realRate()->termStructure();
    } else {
        QL_FAIL("Expected inflation model to be either DK or JY.");
    }
}

void CrossAssetModel::appendToFixedParameterVector(const AssetType t, const AssetType v, const Size param,
                                                   const Size index, const Size i, std::vector<bool>& res) {
    for (Size j = 0; j < components(t); ++j) {
        for (Size k = 0; k < arguments(t, j); ++k) {
            std::vector<bool> tmp1(p_[idx(t, j)]->parameter(k)->size(), true);
            if ((param == Null<Size>() || k == param) && t == v && index == j) {
                for (Size ii = 0; ii < tmp1.size(); ++ii) {
                    if (i == Null<Size>() || i == ii) {
                        tmp1[ii] = false;
                    }
                }
            }
            res.insert(res.end(), tmp1.begin(), tmp1.end());
        }
    }
}

std::vector<bool> CrossAssetModel::MoveParameter(const AssetType t, const Size param, const Size index, const Size i) {
    QL_REQUIRE(param == Null<Size>() || param < arguments(t, index), "parameter for " << t << " at " << index << " ("
                                                                                      << param << ") out of bounds 0..."
                                                                                      << arguments(t, index) - 1);
    std::vector<bool> res(0);
    appendToFixedParameterVector(CrossAssetModel::AssetType::IR, t, param, index, i, res);
    appendToFixedParameterVector(CrossAssetModel::AssetType::FX, t, param, index, i, res);
    appendToFixedParameterVector(CrossAssetModel::AssetType::INF, t, param, index, i, res);
    appendToFixedParameterVector(CrossAssetModel::AssetType::CR, t, param, index, i, res);
    appendToFixedParameterVector(CrossAssetModel::AssetType::EQ, t, param, index, i, res);
    appendToFixedParameterVector(CrossAssetModel::AssetType::COM, t, param, index, i, res);
    return res;
}

} // namespace QuantExt
