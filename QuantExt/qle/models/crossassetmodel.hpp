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

/*! \file models/crossassetmodel.hpp
    \brief cross asset model
    \ingroup crossassetmodel
*/

#ifndef quantext_crossasset_model_hpp
#define quantext_crossasset_model_hpp

#include <qle/models/crlgm1fparametrization.hpp>
#include <qle/models/eqbsparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/infdkparametrization.hpp>
#include <qle/models/lgm.hpp>

#include <qle/processes/crossassetstateprocess.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/integrals/integral.hpp>
#include <ql/math/matrix.hpp>
#include <ql/models/model.hpp>

#include <boost/bind.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace CrossAssetModelTypes {
//! Cross Asset Type
//! \ingroup crossassetmodel
enum AssetType { IR, FX, INF, CR, EQ };
} // namespace CrossAssetModelTypes

using namespace CrossAssetModelTypes;

//! Cross Asset Model
/*! \ingroup crossassetmodel
 */
class CrossAssetModel : public LinkableCalibratedModel {
public:
    /*! Parametrizations must be given in the following order
        - IR  (first parametrization defines the domestic currency)
        - FX  (for all pairs domestic-ccy defined by the IR models)
        - INF (optionally, ccy must be a subset of the IR ccys)
        - CR  (optionally, ccy must be a subset of the IR ccys)
        - EQ  (for all names equity currency defined in Parametrization)
        If the correlation matrix is not given, it is initialized
        as the unit matrix (and can be customized after
        construction of the model).
    */
    CrossAssetModel(const std::vector<boost::shared_ptr<Parametrization> >& parametrizations,
                    const Matrix& correlation = Matrix(),
                    SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None);

    /*! IR-FX model based constructor */
    CrossAssetModel(const std::vector<boost::shared_ptr<LinearGaussMarkovModel> >& currencyModels,
                    const std::vector<boost::shared_ptr<FxBsParametrization> >& fxParametrizations,
                    const Matrix& correlation = Matrix(),
                    SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None);

    /*! returns the state process with a given discretization */
    const boost::shared_ptr<StochasticProcess>
    stateProcess(CrossAssetStateProcess::discretization disc = CrossAssetStateProcess::exact) const;

    /*! total dimension of model (sum of number of state variables) */
    Size dimension() const;

    /*! total number of Brownian motions (this is less or equal to dimension) */
    Size brownians() const;

    /*! total number of parameters that can be calibrated */
    Size totalNumberOfParameters() const;

    /*! number of components for an asset class */
    Size components(const AssetType t) const;

    /*! number of brownian motions for a component */
    Size brownians(const AssetType t, const Size i) const;

    /*! number of state variables for a component */
    Size stateVariables(const AssetType t, const Size i) const;

    /*! return index for currency (0 = domestic, 1 = first
      foreign currency and so on) */
    Size ccyIndex(const Currency& ccy) const;

    /*! return index for equity (0 = first equity) */
    Size eqIndex(const std::string& eqName) const;

    /*! return index for inflation (0 = first inflation index) */
    Size infIndex(const std::string& index) const;

    /*! observer and linked calibrated model interface */
    void update();
    void generateArguments();

    /*! LGM1F components, ccy=0 refers to the domestic currency */
    const boost::shared_ptr<LinearGaussMarkovModel> lgm(const Size ccy) const;

    const boost::shared_ptr<IrLgm1fParametrization> irlgm1f(const Size ccy) const;

    Real numeraire(const Size ccy, const Time t, const Real x,
                   Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    Real discountBond(const Size ccy, const Time t, const Time T, const Real x,
                      Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    Real reducedDiscountBond(const Size ccy, const Time t, const Time T, const Real x,
                             Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    Real discountBondOption(const Size ccy, Option::Type type, const Real K, const Time t, const Time S, const Time T,
                            Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    /*! FXBS components, ccy=0 referes to the first foreign currency,
        so it corresponds to ccy+1 if you want to get the corresponding
        irmgl1f component */
    const boost::shared_ptr<FxBsParametrization> fxbs(const Size ccy) const;

    /*! INF DK components */
    const boost::shared_ptr<InfDkParametrization> infdk(const Size i) const;

    /*! CR LGM 1F components */
    const boost::shared_ptr<CrLgm1fParametrization> crlgm1f(const Size i) const;

    /*! EQBS components */
    const boost::shared_ptr<EqBsParametrization> eqbs(const Size ccy) const;

    /* ... add more components here ...*/

    /*! correlation linking the different marginal models, note that
        the use of asset class pairs specific inspectors is
        recommended instead of the global matrix directly */
    const Matrix& correlation() const;

    /*! check if correlation matrix is valid */
    void checkCorrelationMatrix() const;

    /*! index of component in the parametrization vector */
    Size idx(const AssetType t, const Size i) const;

    /*! index of component in the correlation matrix, by offset */
    Size cIdx(const AssetType t, const Size i, const Size offset = 0) const;

    /*! index of component in the stochastic process array, by offset */
    Size pIdx(const AssetType t, const Size i, const Size offset = 0) const;

    /*! correlation between two components */
    const Real& correlation(const AssetType s, const Size i, const AssetType t, const Size j, const Size iOffset = 0,
                            const Size jOffset = 0) const;
    /*! set correlation */
    void correlation(const AssetType s, const Size i, const AssetType t, const Size j, const Real value,
                     const Size iOffset = 0, const Size jOffset = 0);

    /*! analytical moments require numerical integration,
      which can be customized here */
    void setIntegrationPolicy(const boost::shared_ptr<Integrator> integrator,
                              const bool usePiecewiseIntegration = true) const;
    const boost::shared_ptr<Integrator> integrator() const;

    /*! return (V(t), V^tilde(t,T)) in the notation of the book */
    std::pair<Real, Real> infdkV(const Size i, const Time t, const Time T);

    /*! return (I(t), I^tilde(t,T)) in the notation of the book, note that
        I(0) is normalized to 1 here, i.e. you have to multiply the result
        with the index value (as of the base date of the inflation ts) */
    std::pair<Real, Real> infdkI(const Size i, const Time t, const Time T, const Real z, const Real y);

    /*! return YoYIIS(t) in the notation of the book, the year on year
        swaplet price from S to T, at time t */
    Real infdkYY(const Size i, const Time t, const Time S, const Time T, const Real z, const Real y, const Real irz);

    /*! returns (S(t), S^tilde(t,T)) in the notation of the book */
    std::pair<Real, Real> crlgm1fS(const Size i, const Size ccy, const Time t, const Time T, const Real z,
                                   const Real y) const;

    /*! calibration procedures */

    /*! calibrate irlgm1f volatilities to a sequence of ir options with
        expiry times equal to step times in the parametrization */
    void calibrateIrLgm1fVolatilitiesIterative(const Size ccy,
                                               const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                               OptimizationMethod& method, const EndCriteria& endCriteria,
                                               const Constraint& constraint = Constraint(),
                                               const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate irlgm1f reversion to a sequence of ir options with
        maturities equal to step times in the parametrization */
    void calibrateIrLgm1fReversionsIterative(const Size ccy,
                                             const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                             OptimizationMethod& method, const EndCriteria& endCriteria,
                                             const Constraint& constraint = Constraint(),
                                             const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate irlgm1f parameters for one ccy globally to a set
        of ir options */
    void calibrateIrLgm1fGlobal(const Size ccy, const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                OptimizationMethod& method, const EndCriteria& endCriteria,
                                const Constraint& constraint = Constraint(),
                                const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate eq or fx volatilities to a sequence of options with
            expiry times equal to step times in the parametrization */
    void calibrateBsVolatilitiesIterative(const AssetType& assetType, const Size aIdx,
                                          const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                          OptimizationMethod& method, const EndCriteria& endCriteria,
                                          const Constraint& constraint = Constraint(),
                                          const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate eq/fx volatilities globally to a set of fx options */
    void calibrateBsVolatilitiesGlobal(const AssetType& assetType, const Size aIdx,
                                       const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                       OptimizationMethod& method, const EndCriteria& endCriteria,
                                       const Constraint& constraint = Constraint(),
                                       const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate infdk volatilities to a sequence of cpi options with
        expiry times equal to step times in the parametrization */
    void calibrateInfDkVolatilitiesIterative(const Size index,
                                             const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                             OptimizationMethod& method, const EndCriteria& endCriteria,
                                             const Constraint& constraint = Constraint(),
                                             const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate infdk reversions to a sequence of cpi options with
        maturity times equal to step times in the parametrization */
    void calibrateInfDkReversionsIterative(const Size index,
                                           const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                           OptimizationMethod& method, const EndCriteria& endCriteria,
                                           const Constraint& constraint = Constraint(),
                                           const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate infdk volatilities globally to a sequence of cpi cap/floors */
    void calibrateInfDkVolatilitiesGlobal(const Size index,
                                          const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                          OptimizationMethod& method, const EndCriteria& endCriteria,
                                          const Constraint& constraint = Constraint(),
                                          const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate infdk reversions globally to a sequence of cpi cap/floors */
    void calibrateInfDkReversionsGlobal(const Size index,
                                        const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                        OptimizationMethod& method, const EndCriteria& endCriteria,
                                        const Constraint& constraint = Constraint(),
                                        const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate crlgm1f volatilities to a sequence of cds options with
        expiry times equal to step times in the parametrization */
    void calibrateCrLgm1fVolatilitiesIterative(const Size index,
                                               const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                               OptimizationMethod& method, const EndCriteria& endCriteria,
                                               const Constraint& constraint = Constraint(),
                                               const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate crlgm1f reversions to a sequence of cds options with
        maturity times equal to step times in the parametrization */
    void calibrateCrLgm1fReversionsIterative(const Size index,
                                             const std::vector<boost::shared_ptr<CalibrationHelper> >& helpers,
                                             OptimizationMethod& method, const EndCriteria& endCriteria,
                                             const Constraint& constraint = Constraint(),
                                             const std::vector<Real>& weights = std::vector<Real>());

    /* ... add more calibration procedures here ... */

protected:
    /* ctor to be used in extensions, initialize is not called */
    CrossAssetModel(const std::vector<boost::shared_ptr<Parametrization> >& parametrizations, const Matrix& correlation,
                    SalvagingAlgorithm::Type salvaging, const bool)
        : LinkableCalibratedModel(), p_(parametrizations), rho_(correlation), salvaging_(salvaging) {}

    /*! number of arguments for a component */
    Size arguments(const AssetType t, const Size i) const;

    /*! index of component in the arguments vector, by offset */
    Size aIdx(const AssetType t, const Size i, const Size offset = 0) const;

    /* init methods */
    virtual void initialize();
    virtual void initializeParametrizations();
    virtual void initializeCorrelation();
    virtual void initializeArguments();
    virtual void finalizeArguments();
    virtual void checkModelConsistency() const;
    virtual void initDefaultIntegrator();
    virtual void initStateProcess();

    /* helper function for infdkI, crlgm1fS */
    Real infV(const Size idx, const Size ccy, const Time t, const Time T) const;
    Real crV(const Size idx, const Size ccy, const Time t, const Time T) const;

    // cache for infdkI, crlgm1fS method
    struct cache_key {
        Size i, ccy;
        double t, T;
        bool operator==(const cache_key& o) const { return (i == o.i) && (ccy == o.ccy) && (t == o.t) && (T == o.T); }
    };

    struct cache_hasher : std::unary_function<cache_key, std::size_t> {
        std::size_t operator()(cache_key const& x) const {
            std::size_t seed = 0;
            boost::hash_combine(seed, x.i);
            boost::hash_combine(seed, x.ccy);
            boost::hash_combine(seed, x.t);
            boost::hash_combine(seed, x.T);
            return seed;
        }
    };

    mutable boost::unordered_map<cache_key, std::pair<Real, Real>, cache_hasher> cache_crlgm1fS_, cache_infdkI_;

    /* members */

    Size nIrLgm1f_, nFxBs_, nInfDk_, nCrLgm1f_, nEqBs_;
    Size totalNumberOfParameters_;
    std::vector<boost::shared_ptr<Parametrization> > p_;
    std::vector<boost::shared_ptr<LinearGaussMarkovModel> > lgm_;
    Matrix rho_;
    SalvagingAlgorithm::Type salvaging_;
    mutable boost::shared_ptr<Integrator> integrator_;
    boost::shared_ptr<CrossAssetStateProcess> stateProcessExact_, stateProcessEuler_;

    /* calibration constraints */

    // move parameter param (e.g. vol, reversion) of asset type component t / index
    // at step i (or at all steps if i is null)
    Disposable<std::vector<bool> > MoveParameter(const AssetType t, const Size param, const Size index, const Size i) {
        switch (t) {
        case IR:
            QL_REQUIRE(param <= 1, "irlgm1f parameter " << param << " out of bounds 0...1");
            QL_REQUIRE(i < irlgm1f(index)->parameter(param)->size(),
                       "irlgm1f parameter (" << param << ") index (" << i << ") for ccy " << index
                                             << " out of bounds 0..." << irlgm1f(index)->parameter(param)->size() - 1);
            break;
        case FX:
            QL_REQUIRE(param == 0, "fxbs parameter " << param << " out of bounds 0...0");
            QL_REQUIRE(i < fxbs(index)->parameter(param)->size(),
                       "fxbs volatility index (" << i << ") for ccy " << index << " out of bounds 0..."
                                                 << fxbs(index)->parameter(param)->size() - 1);
            break;
        case INF:
            QL_REQUIRE(param <= 1, "infdk parameter " << param << " out of bounds 0...1");
            QL_REQUIRE(i < infdk(index)->parameter(param)->size(),
                       "infdk parameter (" << param << ") index (" << i << ") for inflation component " << index
                                           << " out of bounds 0..." << infdk(index)->parameter(param)->size() - 1);
            break;
        case CR:
            QL_REQUIRE(param <= 1, "crlgm1f parameter " << param << " out of bounds 0...1");
            QL_REQUIRE(i < crlgm1f(index)->parameter(param)->size(),
                       "crlgm1f parameter (" << param << ") index (" << i << ") for credit component " << index
                                             << " out of bounds 0..." << crlgm1f(index)->parameter(param)->size() - 1);
            break;
        case EQ:
            QL_REQUIRE(param == 0, "eqbs parameter " << param << " out of bounds 0...0");
            QL_REQUIRE(i < eqbs(index)->parameter(param)->size(),
                       "eqbs volatility index (" << i << ") for index " << index << " out of bounds 0..."
                                                 << eqbs(index)->parameter(param)->size() - 1);
            break;
        default:
            QL_FAIL("asset type not recognised");
        }
        std::vector<bool> res(0);
        for (Size j = 0; j < nIrLgm1f_; ++j) {
            std::vector<bool> tmp1(p_[idx(IR, j)]->parameter(0)->size(), true);
            std::vector<bool> tmp2(p_[idx(IR, j)]->parameter(1)->size(), true);
            std::vector<std::vector<bool> > tmp;
            tmp.push_back(tmp1);
            tmp.push_back(tmp2);
            if (t == IR && index == j) {
                for (Size ii = 0; ii < tmp[param].size(); ++ii) {
                    if (i == Null<Size>() || i == ii) {
                        tmp[param][ii] = false;
                    }
                }
            }
            res.insert(res.end(), tmp[0].begin(), tmp[0].end());
            res.insert(res.end(), tmp[1].begin(), tmp[1].end());
        }
        for (Size j = 0; j < nFxBs_; ++j) {
            std::vector<bool> tmp(p_[idx(FX, j)]->parameter(0)->size(), true);
            if (t == FX && index == j) {
                for (Size ii = 0; ii < tmp.size(); ++ii) {
                    if (i == Null<Size>() || i == ii) {
                        tmp[i] = false;
                    }
                }
            }
            res.insert(res.end(), tmp.begin(), tmp.end());
        }
        for (Size j = 0; j < nInfDk_; ++j) {
            std::vector<bool> tmp1(p_[idx(INF, j)]->parameter(0)->size(), true);
            std::vector<bool> tmp2(p_[idx(INF, j)]->parameter(1)->size(), true);
            std::vector<std::vector<bool> > tmp;
            tmp.push_back(tmp1);
            tmp.push_back(tmp2);
            if (t == INF && index == j) {
                for (Size ii = 0; ii < tmp[param].size(); ++ii) {
                    if (i == Null<Size>() || i == ii) {
                        tmp[param][ii] = false;
                    }
                }
            }
            res.insert(res.end(), tmp[0].begin(), tmp[0].end());
            res.insert(res.end(), tmp[1].begin(), tmp[1].end());
        }
        for (Size j = 0; j < nCrLgm1f_; ++j) {
            std::vector<bool> tmp1(p_[idx(CR, j)]->parameter(0)->size(), true);
            std::vector<bool> tmp2(p_[idx(CR, j)]->parameter(1)->size(), true);
            std::vector<std::vector<bool> > tmp;
            tmp.push_back(tmp1);
            tmp.push_back(tmp2);
            if (t == CR && index == j) {
                for (Size ii = 0; ii < tmp[param].size(); ++ii) {
                    if (i == Null<Size>() || i == ii) {
                        tmp[param][ii] = false;
                    }
                }
            }
            res.insert(res.end(), tmp[0].begin(), tmp[0].end());
            res.insert(res.end(), tmp[1].begin(), tmp[1].end());
        }
        for (Size j = 0; j < nEqBs_; ++j) {
            std::vector<bool> tmp(p_[idx(EQ, j)]->parameter(0)->size(), true);
            if (t == EQ && index == j) {
                for (Size ii = 0; ii < tmp.size(); ++ii) {
                    if (i == Null<Size>() || i == ii) {
                        tmp[i] = false;
                    }
                }
            }
            res.insert(res.end(), tmp.begin(), tmp.end());
        }
        return res;
    }
};

// inline

inline const boost::shared_ptr<StochasticProcess>
CrossAssetModel::stateProcess(CrossAssetStateProcess::discretization disc) const {
    return disc == CrossAssetStateProcess::exact ? stateProcessExact_ : stateProcessEuler_;
}

inline Size CrossAssetModel::dimension() const {
    // this assumes specific models, as soon as other model types are added
    // this formula has to be generalized as well
    return nIrLgm1f_ * 1 + nFxBs_ * 1 + nInfDk_ * 2 + nCrLgm1f_ * 2 + nEqBs_ * 1;
}

inline Size CrossAssetModel::brownians() const {
    // this assumes specific models, as soon as other model types are added
    // this formula has to be generalized as well
    return nIrLgm1f_ * 1 + nFxBs_ * 1 + nInfDk_ * 1 + nCrLgm1f_ * 1 + nEqBs_ * 1;
}

inline Size CrossAssetModel::totalNumberOfParameters() const { return totalNumberOfParameters_; }

inline const boost::shared_ptr<LinearGaussMarkovModel> CrossAssetModel::lgm(const Size ccy) const {
    return lgm_[idx(IR, ccy)];
}

inline const boost::shared_ptr<IrLgm1fParametrization> CrossAssetModel::irlgm1f(const Size ccy) const {
    return lgm(ccy)->parametrization();
}

inline const boost::shared_ptr<InfDkParametrization> CrossAssetModel::infdk(const Size i) const {
    return boost::static_pointer_cast<InfDkParametrization>(p_[idx(INF, i)]);
}

inline const boost::shared_ptr<CrLgm1fParametrization> CrossAssetModel::crlgm1f(const Size i) const {
    return boost::static_pointer_cast<CrLgm1fParametrization>(p_[idx(CR, i)]);
}

inline const boost::shared_ptr<EqBsParametrization> CrossAssetModel::eqbs(const Size name) const {
    return boost::dynamic_pointer_cast<EqBsParametrization>(p_[idx(EQ, name)]);
}

inline Real CrossAssetModel::numeraire(const Size ccy, const Time t, const Real x,
                                       Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->numeraire(t, x, discountCurve);
}

inline Real CrossAssetModel::discountBond(const Size ccy, const Time t, const Time T, const Real x,
                                          Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->discountBond(t, T, x, discountCurve);
}

inline Real CrossAssetModel::reducedDiscountBond(const Size ccy, const Time t, const Time T, const Real x,
                                                 Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->reducedDiscountBond(t, T, x, discountCurve);
}

inline Real CrossAssetModel::discountBondOption(const Size ccy, Option::Type type, const Real K, const Time t,
                                                const Time S, const Time T,
                                                Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->discountBondOption(type, K, t, S, T, discountCurve);
}

inline const boost::shared_ptr<FxBsParametrization> CrossAssetModel::fxbs(const Size ccy) const {
    return boost::dynamic_pointer_cast<FxBsParametrization>(p_[idx(FX, ccy)]);
}

inline const Matrix& CrossAssetModel::correlation() const { return rho_; }

inline const boost::shared_ptr<Integrator> CrossAssetModel::integrator() const { return integrator_; }

} // namespace QuantExt

#endif
