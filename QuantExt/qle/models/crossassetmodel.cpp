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

namespace CrossAssetModelTypes {

std::ostream& operator<<(std::ostream& out, const AssetType& type) {
    switch (type) {
    case IR:
        return out << "IR";
    case FX:
        return out << "FX";
    case INF:
        return out << "INF";
    case CR:
        return out << "CR";
    case EQ:
        return out << "EQ";
    case AUX:
        return out << "AUX";
    default:
        QL_FAIL("Did not recognise cross asset model type " << static_cast<int>(type) << ".");
    }
}

} // namespace CrossAssetModelTypes

CrossAssetModel::CrossAssetModel(const std::vector<boost::shared_ptr<Parametrization>>& parametrizations,
                                 const Matrix& correlation, SalvagingAlgorithm::Type salvaging, Measure::Type measure)
    : LinkableCalibratedModel(), p_(parametrizations), rho_(correlation), salvaging_(salvaging), measure_(measure) {
    initialize();
}

CrossAssetModel::CrossAssetModel(const std::vector<boost::shared_ptr<LinearGaussMarkovModel>>& currencyModels,
                                 const std::vector<boost::shared_ptr<FxBsParametrization>>& fxParametrizations,
                                 const Matrix& correlation, SalvagingAlgorithm::Type salvaging, Measure::Type measure)
    : LinkableCalibratedModel(), lgm_(currencyModels), rho_(correlation), salvaging_(salvaging), measure_(measure) {
    for (Size i = 0; i < currencyModels.size(); ++i) {
        p_.push_back(currencyModels[i]->parametrization());
    }
    for (Size i = 0; i < fxParametrizations.size(); ++i) {
        p_.push_back(fxParametrizations[i]);
    }
    initialize();
}

Size CrossAssetModel::components(const AssetType t) const { return components_[t]; }

Size CrossAssetModel::ccyIndex(const Currency& ccy) const {
    Size i = 0;
    while (i < components(IR) && ir(i)->currency() != ccy)
        ++i;
    QL_REQUIRE(i < components(IR), "currency " << ccy.code() << " not present in cross asset model");
    return i;
}

Size CrossAssetModel::eqIndex(const std::string& name) const {
    Size i = 0;
    while (i < components(EQ) && eq(i)->name() != name)
        ++i;
    QL_REQUIRE(i < components(EQ), "equity name " << name << " not present in cross asset model");
    return i;
}

Size CrossAssetModel::infIndex(const std::string& index) const {
    Size i = 0;
    while (i < components(INF) && inf(i)->name() != index)
        ++i;
    QL_REQUIRE(i < components(INF), "inflation index " << index << " not present in cross asset model");
    return i;
}

Size CrossAssetModel::crName(const std::string& name) const {
    Size i = 0;
    while (i < components(CR) && cr(i)->name() != name)
        ++i;
    QL_REQUIRE(i < components(INF), "credit name " << name << " not present in cross asset model");
    return i;
}

void CrossAssetModel::update() {
    cache_crlgm1fS_.clear();
    cache_infdkI_.clear();
    for (Size i = 0; i < p_.size(); ++i) {
        p_[i]->update();
    }
    stateProcessExact_->flushCache();
    stateProcessEuler_->flushCache();
    notifyObservers();
}

void CrossAssetModel::generateArguments() { update(); }

Size CrossAssetModel::brownians(const AssetType t, const Size i) const {
    QL_REQUIRE(brownians_[t].size() > i,
               "CrossAssetModel::brownians(): asset class " << t << ", component " << i << " not known.");
    return brownians_[t][i];
}

Size CrossAssetModel::stateVariables(const AssetType t, const Size i) const {
    QL_REQUIRE(stateVariables_[t].size() > i,
               "CrossAssetModel::stateVariables(): asset class " << t << ", component " << i << " not known.");
    return stateVariables_[t][i];
}

Size CrossAssetModel::arguments(const AssetType t, const Size i) const {
    QL_REQUIRE(numArguments_[t].size() > i,
               "CrossAssetModel::arguments(): asset class " << t << ", component " << i << " not known.");
    return numArguments_[t][i];
}

ModelType CrossAssetModel::modelType(const AssetType t, const Size i) const {
    QL_REQUIRE(modelType_[t].size() > i,
               "CrossAssetModel::modelType(): asset class " << t << ", component " << i << " not known.");
    return modelType_[t][i];
}

Size CrossAssetModel::idx(const AssetType t, const Size i) const {
    QL_REQUIRE(idx_[t].size() > i, "CrossAssetModel::idx(): asset class " << t << ", component " << i << " not known.");
    return idx_[t][i];
}

Size CrossAssetModel::cIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < brownians(t, i), "c-offset (" << offset << ") for asset class " << t << " and index " << i
                                                      << " must be in 0..." << brownians(t, i) - 1);
    QL_REQUIRE(cIdx_[t].size() > i,
               "CrossAssetModel::cIdx(): asset class " << t << ", component " << i << " not known.");
    return cIdx_[t][i] + offset;
}

Size CrossAssetModel::pIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < stateVariables(t, i), "p-offset (" << offset << ") for asset class " << t << " and index " << i
                                                           << " must be in 0..." << stateVariables(t, i) - 1);
    QL_REQUIRE(pIdx_[t].size() > i,
               "CrossAssetModel::pIdx(): asset class " << t << ", component " << i << " not known.");
    return pIdx_[t][i] + offset;
}

Size CrossAssetModel::aIdx(const AssetType t, const Size i, const Size offset) const {
    QL_REQUIRE(offset < arguments(t, i), "a-offset (" << offset << ") for asset class " << t << " and index " << i
                                                      << " must be in 0..." << arguments(t, i) - 1);
    QL_REQUIRE(aIdx_[t].size() > i,
               "CrossAssetModel::aIdx(): asset class " << t << ", component " << i << " not known.");
    return aIdx_[t][i];
}

const Real& CrossAssetModel::correlation(const AssetType s, const Size i, const AssetType t, const Size j,
                                         const Size iOffset, const Size jOffset) const {
    return rho_(cIdx(s, i, iOffset), cIdx(t, j, jOffset));
}

void CrossAssetModel::correlation(const AssetType s, const Size i, const AssetType t, const Size j, const Real value,
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
    initStateProcess();
}

void CrossAssetModel::initDefaultIntegrator() {
    setIntegrationPolicy(boost::make_shared<SimpsonIntegral>(1.0E-8, 100), true);
}

void CrossAssetModel::initStateProcess() {
    stateProcessEuler_ = boost::make_shared<CrossAssetStateProcess>(this, CrossAssetStateProcess::euler, salvaging_);
    stateProcessExact_ = boost::make_shared<CrossAssetStateProcess>(this, CrossAssetStateProcess::exact, salvaging_);
}

void CrossAssetModel::setIntegrationPolicy(const boost::shared_ptr<Integrator> integrator,
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
    integrator_ = boost::make_shared<PiecewiseIntegral>(integrator, allTimes, true);
}

std::pair<AssetType, ModelType> CrossAssetModel::getComponentType(const Size i) const {
    if (boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i]))
        return std::make_pair(IR, LGM1F);
    if (boost::dynamic_pointer_cast<FxBsParametrization>(p_[i]))
        return std::make_pair(FX, BS);
    if (boost::dynamic_pointer_cast<InfDkParametrization>(p_[i]))
        return std::make_pair(INF, DK);
    if (boost::dynamic_pointer_cast<InfJyParameterization>(p_[i]))
        return std::make_pair(INF, JY);
    if (boost::dynamic_pointer_cast<CrLgm1fParametrization>(p_[i]))
        return std::make_pair(CR, LGM1F);
    if (boost::dynamic_pointer_cast<CrCirppParametrization>(p_[i]))
        return std::make_pair(CR, CIRPP);
    if (boost::dynamic_pointer_cast<EqBsParametrization>(p_[i]))
        return std::make_pair(EQ, BS);
    QL_FAIL("parametrization " << i << " has unknown type");
}

Size CrossAssetModel::getNumberOfParameters(const Size i) const { return p_[i]->numberOfParameters(); }

Size CrossAssetModel::getNumberOfBrownians(const Size i) const {
    if (boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i]))
        return 1;
    if (boost::dynamic_pointer_cast<FxBsParametrization>(p_[i]))
        return 1;
    if (boost::dynamic_pointer_cast<InfDkParametrization>(p_[i]))
        return 1;
    if (boost::dynamic_pointer_cast<InfJyParameterization>(p_[i]))
        return 2;
    if (boost::dynamic_pointer_cast<CrLgm1fParametrization>(p_[i]))
        return 1;
    if (boost::dynamic_pointer_cast<CrCirppParametrization>(p_[i]))
        return 1;
    if (boost::dynamic_pointer_cast<EqBsParametrization>(p_[i]))
        return 1;
    QL_FAIL("parametrization " << i << " has unknown type");
}

Size CrossAssetModel::getNumberOfStateVariables(const Size i) const {
    if (boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i]))
        return 1;
    if (boost::dynamic_pointer_cast<FxBsParametrization>(p_[i]))
        return 1;
    if (boost::dynamic_pointer_cast<InfDkParametrization>(p_[i]))
        return 2;
    if (boost::dynamic_pointer_cast<InfJyParameterization>(p_[i]))
        return 2;
    if (boost::dynamic_pointer_cast<CrLgm1fParametrization>(p_[i]))
        return 2;
    if (boost::dynamic_pointer_cast<CrCirppParametrization>(p_[i]))
        return 2;
    if (boost::dynamic_pointer_cast<EqBsParametrization>(p_[i]))
        return 1;
    QL_FAIL("parametrization " << i << " has unknown type");
}

void CrossAssetModel::updateIndices(const AssetType& t, const Size i, const Size cIdx, const Size pIdx,
                                    const Size aIdx) {
    idx_[t].push_back(i);
    modelType_[t].push_back(getComponentType(i).second);
    brownians_[t].push_back(getNumberOfBrownians(i));
    stateVariables_[t].push_back(getNumberOfStateVariables(i));
    numArguments_[t].push_back(getNumberOfParameters(i));
    cIdx_[t].push_back(cIdx);
    pIdx_[t].push_back(pIdx);
    aIdx_[t].push_back(aIdx);
}

void CrossAssetModel::initializeParametrizations() {

    // count the parametrizations and check their order and their support

    Size i = 0, j;
    Size cIdxTmp = 0, pIdxTmp = 0, aIdxTmp = 0;
    components_.resize(crossAssetModelAssetTypes, 0);
    idx_.resize(crossAssetModelAssetTypes);
    cIdx_.resize(crossAssetModelAssetTypes);
    pIdx_.resize(crossAssetModelAssetTypes);
    aIdx_.resize(crossAssetModelAssetTypes);
    brownians_.resize(crossAssetModelAssetTypes);
    stateVariables_.resize(crossAssetModelAssetTypes);
    numArguments_.resize(crossAssetModelAssetTypes);
    modelType_.resize(crossAssetModelAssetTypes);

    // IR parametrizations

    bool genericCtor = lgm_.empty();
    j = 0;
    while (i < p_.size() && getComponentType(i).first == IR) {
        // initialize lgm model, if generic constructor was used
        if (genericCtor) {
            if (getComponentType(i).second == LGM1F) {
                lgm_.push_back(boost::make_shared<LinearGaussMarkovModel>(
                    boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[i])));
            } else {
                lgm_.push_back(boost::shared_ptr<LinearGaussMarkovModel>());
            }
        }
        updateIndices(IR, i, cIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
    }
    components_[IR] = j;

    // FX parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == FX) {
        updateIndices(FX, i, cIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
    }
    components_[FX] = j;

    QL_REQUIRE(components_[IR] > 0, "at least one ir parametrization must be given");

    QL_REQUIRE(components_[FX] == components_[IR] - 1, "there must be n-1 fx "
                                                       "for n ir parametrizations, found "
                                                           << components_[IR] << " ir and " << components_[FX]
                                                           << " fx parametrizations");

    // check currencies

    // without an order or a hash function on Currency this seems hard
    // to do in a simpler way ...
    Size uniqueCurrencies = 0;
    std::vector<Currency> currencies;
    for (Size i = 0; i < components_[IR]; ++i) {
        Size tmp = 1;
        for (Size j = 0; j < i; ++j) {
            if (ir(i)->currency() == currencies[j])
                tmp = 0;
        }
        uniqueCurrencies += tmp;
        currencies.push_back(ir(i)->currency());
    }
    QL_REQUIRE(uniqueCurrencies == components_[IR], "there are duplicate currencies "
                                                    "in the set of ir "
                                                    "parametrizations");
    for (Size i = 0; i < components_[FX]; ++i) {
        QL_REQUIRE(fx(i)->currency() == ir(i + 1)->currency(),
                   "fx parametrization #" << i << " must be for currency of ir parametrization #" << (i + 1)
                                          << ", but they are " << fx(i)->currency() << " and "
                                          << irlgm1f(i + 1)->currency() << " respectively");
    }

    // Inf parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == INF) {
        updateIndices(INF, i, cIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
        // we do not check the currency, if not present among the model's
        // currencies, it will throw below
    }
    components_[INF] = j;

    // Cr parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == CR) {

        if (getComponentType(i).second == CIRPP) {
            auto tmp = boost::dynamic_pointer_cast<CrCirppParametrization>(p_[i]);
            QL_REQUIRE(tmp, "CrossAssetModelPlus::initializeParametrizations(): expected CrCirppParametrization");
            crcirppModel_.push_back(boost::make_shared<CrCirpp>(tmp));
        } else
            crcirppModel_.push_back(boost::shared_ptr<CrCirpp>());

        updateIndices(CR, i, cIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
        // we do not check the currency, if not present among the model's
        // currencies, it will throw below
    }
    components_[CR] = j;

    // Eq parametrizations

    j = 0;
    while (i < p_.size() && getComponentType(i).first == EQ) {
        updateIndices(EQ, i, cIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += getNumberOfBrownians(i);
        pIdxTmp += getNumberOfStateVariables(i);
        aIdxTmp += getNumberOfParameters(i);
        ++j;
        ++i;
    }
    components_[EQ] = j;

    // check the equity currencies to ensure they are covered by CrossAssetModel
    for (Size i = 0; i < components(EQ); ++i) {
        Currency eqCcy = eq(i)->currency();
        try {
            Size eqCcyIdx = ccyIndex(eqCcy);
            QL_REQUIRE(eqCcyIdx < components_[IR], "Invalid currency for equity " << eqbs(i)->name());
        } catch (...) {
            QL_FAIL("Invalid currency (" << eqCcy.code() << ") for equity " << eqbs(i)->name());
        }
    }

    if (measure_ == Measure::BA) {

        QL_REQUIRE(components_[INF] == 0, "CAM in BA measure does not support INF components yet");
        QL_REQUIRE(components_[EQ] == 0, "CAM in BA measure does not support EQ components yet");
        QL_REQUIRE(components_[CR] == 0, "CAM in BA measure does not support CR components yet");

        // AUX variable for BA measure simulations

        components_[AUX] = 1;
        updateIndices(AUX, i, cIdxTmp, pIdxTmp, aIdxTmp);
        cIdxTmp += 1;
        pIdxTmp += 1;
    }

    // Summary statistics

    totalDimension_ = pIdxTmp;
    totalNumberOfBrownians_ = cIdxTmp;

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

    SymmetricSchurDecomposition ssd(rho_);
    for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
        QL_REQUIRE(ssd.eigenvalues()[i] >= 0.0,
                   "correlation matrix has negative eigenvalue at " << i << " (" << ssd.eigenvalues()[i] << ")");
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
    QL_REQUIRE(components(IR) > 0, "at least one IR component must be given");
    QL_REQUIRE(components(IR) + components(FX) + components(INF) + components(CR) + components(EQ) + components(AUX) ==
                   p_.size(),
               "the parametrizations must be given in the following order: ir, "
               "fx, inf, cr, eq, found "
                   << components(IR) << " ir, " << components(FX) << " bs, " << components(INF) << " inf, "
                   << components(CR) << " cr, " << components(EQ) << " eq, " << components(AUX)
                   << " aux parametrizations, "
                   << "but there are " << p_.size() << " parametrizations given in total");
}

void CrossAssetModel::calibrateIrLgm1fVolatilitiesIterative(
    const Size ccy, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    lgm(ccy)->calibrateVolatilitiesIterative(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateIrLgm1fReversionsIterative(
    const Size ccy, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    lgm(ccy)->calibrateReversionsIterative(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateIrLgm1fGlobal(const Size ccy,
                                             const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                             OptimizationMethod& method, const EndCriteria& endCriteria,
                                             const Constraint& constraint, const std::vector<Real>& weights) {
    lgm(ccy)->calibrate(helpers, method, endCriteria, constraint, weights);
    update();
}

void CrossAssetModel::calibrateBsVolatilitiesIterative(
    const AssetType& assetType, const Size idx, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
    OptimizationMethod& method, const EndCriteria& endCriteria, const Constraint& constraint,
    const std::vector<Real>& weights) {
    QL_REQUIRE(assetType == FX || assetType == EQ, "Unsupported AssetType for BS calibration");
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(assetType, 0, idx, i));
    }
    update();
}

void CrossAssetModel::calibrateBsVolatilitiesGlobal(
    const AssetType& assetType, const Size aIdx, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
    OptimizationMethod& method, const EndCriteria& endCriteria, const Constraint& constraint,
    const std::vector<Real>& weights) {
    QL_REQUIRE(assetType == FX || assetType == EQ, "Unsupported AssetType for BS calibration");
    calibrate(helpers, method, endCriteria, constraint, weights, MoveParameter(assetType, 0, aIdx, Null<Size>()));
    update();
}

void CrossAssetModel::calibrateInfDkVolatilitiesIterative(
    const Size index, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(INF, 0, index, i));
    }
    update();
}

void CrossAssetModel::calibrateInfDkReversionsIterative(
    const Size index, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(INF, 1, index, i));
    }
    update();
}

void CrossAssetModel::calibrateInfDkVolatilitiesGlobal(
    const Size index, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    calibrate(helpers, method, endCriteria, constraint, weights, MoveParameter(INF, 0, index, Null<Size>()));
    update();
}

void CrossAssetModel::calibrateInfDkReversionsGlobal(
    const Size index, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    calibrate(helpers, method, endCriteria, constraint, weights, MoveParameter(INF, 1, index, Null<Size>()));
    update();
}

void CrossAssetModel::calibrateInfJyGlobal(Size index, const vector<boost::shared_ptr<CalibrationHelper>>& helpers,
                                           OptimizationMethod& method, const EndCriteria& endCriteria,
                                           const map<Size, bool>& toCalibrate, const Constraint& constraint,
                                           const vector<Real>& weights) {

    // Initialise the parameters to move first to get the size.
    vector<bool> fixedParams = MoveParameter(INF, 0, index, Null<Size>());
    std::fill(fixedParams.begin(), fixedParams.end(), true);

    // Update fixedParams with parameters that need to be calibrated.
    for (const auto& kv : toCalibrate) {
        if (kv.second) {
            vector<bool> tmp = MoveParameter(INF, kv.first, index, Null<Size>());
            std::transform(fixedParams.begin(), fixedParams.end(), tmp.begin(), fixedParams.begin(),
                           std::logical_and<bool>());
        }
    }

    // Perform the calibration
    calibrate(helpers, method, endCriteria, constraint, weights, fixedParams);

    update();
}

void CrossAssetModel::calibrateInfJyIterative(Size mIdx, Size pIdx,
                                              const vector<boost::shared_ptr<CalibrationHelper>>& helpers,
                                              OptimizationMethod& method, const EndCriteria& endCriteria,
                                              const Constraint& constraint, const vector<Real>& weights) {

    for (Size i = 0; i < helpers.size(); ++i) {
        vector<boost::shared_ptr<CalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(INF, pIdx, mIdx, i));
    }

    update();
}

void CrossAssetModel::calibrateCrLgm1fVolatilitiesIterative(
    const Size index, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(CR, 0, index, i));
    }
    update();
}

void CrossAssetModel::calibrateCrLgm1fReversionsIterative(
    const Size index, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveParameter(CR, 1, index, i));
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
    Real Hyt = Hy(i).eval(this, t);
    Real HyT = Hy(i).eval(this, T);

    // TODO account for seasonality ...
    // compute final results depending on z and y
    const auto& zts = infdk(i)->termStructure();
    auto dc = irlgm1f(0)->termStructure()->dayCounter();
    Real growth_t = inflationGrowth(zts, t, dc);
    Real It = growth_t * std::exp(Hyt * z - y - V0);
    Real Itilde_t_T = inflationGrowth(zts, T, dc) / growth_t * std::exp((HyT - Hyt) * z + V_tilde);
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
    QL_REQUIRE(ccy < components(IR), "ccy index (" << ccy << ") must be in 0..." << (components(IR) - 1));
    QL_REQUIRE(t < T || close_enough(t, T), "crlgm1fS: t (" << t << ") <= T (" << T << ") required");
    QL_REQUIRE(modelType(CR, i) == LGM1F, "model at " << i << " is not CR-LGM1F");
    cache_key k = {i, ccy, t, T};
    boost::unordered_map<cache_key, std::pair<Real, Real>>::const_iterator it = cache_crlgm1fS_.find(k);
    Real V0, V_tilde;
    Real Hlt = Hl(i).eval(this, t);
    Real HlT = Hl(i).eval(this, T);

    if (it == cache_crlgm1fS_.end()) {
        // compute V0 and V_tilde
        if (ccy == 0) {
            // domestic credit
            Real Hzt = Hz(0).eval(this, t);
            Real HzT = Hz(0).eval(this, T);
            Real zetal0 = zetal(i).eval(this, t);
            Real zetal1 = integral(this, P(Hl(i), al(i), al(i)), 0.0, t);
            Real zetal2 = integral(this, P(Hl(i), Hl(i), al(i), al(i)), 0.0, t);
            Real zetanl0 = integral(this, P(rzl(0, i), az(0), al(i)), 0.0, t);
            Real zetanl1 = integral(this, P(rzl(0, i), Hl(i), az(0), al(i)), 0.0, t);
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
    QL_REQUIRE(modelType(CR, i) == CIRPP, "model at " << i << " is not CR-CIR")
    if (close_enough(t, T))
        return std::make_pair(s, 1.0);
    else
        return std::make_pair(s, crcirppModel_[i]->survivalProbability(t, T, y));
}

Real CrossAssetModel::infV(const Size i, const Size ccy, const Time t, const Time T) const {
    Real HyT = Hy(i).eval(this, T);
    Real HdT = irlgm1f(0)->H(T);
    Real rhody = correlation(IR, 0, INF, i, 0, 0);
    Real V;
    if (ccy == 0) {
        V = 0.5 * (HyT * HyT * (zetay(i).eval(this, T) - zetay(i).eval(this, t)) -
                   2.0 * HyT * integral(this, P(Hy(i), ay(i), ay(i)), t, T) +
                   integral(this, P(Hy(i), Hy(i), ay(i), ay(i)), t, T)) -
            rhody * HdT * (HyT * integral(this, P(az(0), ay(i)), t, T) - integral(this, P(az(0), Hy(i), ay(i)), t, T));
    } else {
        Real HfT = irlgm1f(ccy)->H(T);
        Real rhofy = correlation(IR, ccy, INF, i, 0, 0);
        Real rhoxy = correlation(FX, ccy - 1, INF, i, 0, 0);
        V = 0.5 * (HyT * HyT * (zetay(i).eval(this, T) - zetay(i).eval(this, t)) -
                   2.0 * HyT * integral(this, P(Hy(i), ay(i), ay(i)), t, T) +
                   integral(this, P(Hy(i), Hy(i), ay(i), ay(i)), t, T)) -
            rhody * (HyT * integral(this, P(Hz(0), az(0), ay(i)), t, T) -
                     integral(this, P(Hz(0), az(0), Hy(i), ay(i)), t, T)) -
            rhofy * (HfT * HyT * integral(this, P(az(ccy), ay(i)), t, T) -
                     HfT * integral(this, P(az(ccy), Hy(i), ay(i)), t, T) -
                     HyT * integral(this, P(Hz(ccy), az(ccy), ay(i)), t, T) +
                     integral(this, P(Hz(ccy), az(ccy), Hy(i), ay(i)), t, T)) +
            rhoxy * (HyT * integral(this, P(sx(ccy - 1), ay(i)), t, T) -
                     integral(this, P(sx(ccy - 1), Hy(i), ay(i)), t, T));
    }
    return V;
}

Real CrossAssetModel::crV(const Size i, const Size ccy, const Time t, const Time T) const {
    Real HlT = Hl(i).eval(this, T);
    Real HfT = Hz(ccy).eval(this, T);
    Real rhodl = correlation(IR, 0, CR, i, 0, 0);
    Real rhofl = correlation(IR, ccy, CR, i, 0, 0);
    Real rhoxl = correlation(FX, ccy - 1, CR, i, 0, 0);
    return 0.5 * (HlT * HlT * (zetal(i).eval(this, T) - zetal(i).eval(this, t)) -
                  2.0 * HlT * integral(this, P(Hl(i), al(i), al(i)), t, T) +
                  integral(this, P(Hl(i), Hl(i), al(i), al(i)), t, T)) +
           rhodl * (HlT * integral(this, P(Hz(0), az(0), al(i)), t, T) -
                    integral(this, P(Hz(0), az(0), Hl(i), al(i)), t, T)) +
           rhofl * (HfT * HlT * integral(this, P(az(ccy), al(i)), t, T) -
                    HfT * integral(this, P(az(ccy), Hl(i), al(i)), t, T) -
                    HlT * integral(this, P(Hz(ccy), az(ccy), al(i)), t, T) +
                    integral(this, P(Hz(ccy), az(ccy), Hl(i), al(i)), t, T)) -
           rhoxl *
               (HlT * integral(this, P(sx(ccy - 1), al(i)), t, T) - integral(this, P(sx(ccy - 1), Hl(i), al(i)), t, T));
}

Handle<ZeroInflationTermStructure> inflationTermStructure(const boost::shared_ptr<CrossAssetModel>& model, Size index) {

    if (model->modelType(INF, index) == DK) {
        return model->infdk(index)->termStructure();
    } else if (model->modelType(INF, index) == JY) {
        return model->infjy(index)->realRate()->termStructure();
    } else {
        QL_FAIL("Expected inflation model to be either DK or JY.");
    }
}

} // namespace QuantExt
