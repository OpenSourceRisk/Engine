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

#include <qle/models/cirppparametrization.hpp>
#include <qle/models/crcirpp.hpp>
#include <qle/models/crlgm1fparametrization.hpp>
#include <qle/models/eqbsparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/infdkparametrization.hpp>
#include <qle/models/infjyparameterization.hpp>
#include <qle/models/lgm.hpp>

#include <qle/processes/crossassetstateprocess.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/integrals/integral.hpp>
#include <ql/math/matrix.hpp>
#include <ql/models/model.hpp>

#include <boost/bind.hpp>

namespace QuantExt {
using namespace QuantLib;

namespace CrossAssetModelTypes {
//! Cross Asset Type
//! \ingroup crossassetmodel
enum AssetType { IR, FX, INF, CR, EQ, AUX };
static constexpr Size crossAssetModelAssetTypes = 6;

std::ostream& operator<<(std::ostream& out, const AssetType& type);

/*! the model types supported by the CrossAssetModel or derived classes;
  a model type may applicable to several asset types (like BS for FX, EQ) */
enum ModelType { LGM1F, BS, DK, CIRPP, JY };

} // namespace CrossAssetModelTypes

//! Choice of measure
struct Measure {
    enum Type { LGM, BA };
};

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
    CrossAssetModel(const std::vector<boost::shared_ptr<Parametrization>>& parametrizations,
                    const Matrix& correlation = Matrix(), SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None,
                    Measure::Type measure = Measure::LGM);

    /*! IR-FX model based constructor */
    CrossAssetModel(const std::vector<boost::shared_ptr<LinearGaussMarkovModel>>& currencyModels,
                    const std::vector<boost::shared_ptr<FxBsParametrization>>& fxParametrizations,
                    const Matrix& correlation = Matrix(), SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None,
                    Measure::Type measure = Measure::LGM);

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

    /*! model type of a component */
    ModelType modelType(const AssetType t, const Size i) const;

    /*! Choice of probability measure */
    Measure::Type measure() const { return measure_; }

    /*! return index for currency (0 = domestic, 1 = first
      foreign currency and so on) */
    Size ccyIndex(const Currency& ccy) const;

    /*! return index for equity (0 = first equity) */
    Size eqIndex(const std::string& eqName) const;

    /*! return index for inflation (0 = first inflation index) */
    Size infIndex(const std::string& index) const;

    /*! return index for credit (0 = first credit name) */
    Size crName(const std::string& name) const;

    /*! observer and linked calibrated model interface */
    void update();
    void generateArguments();

    /*! the vector of parametrizations */
    const std::vector<boost::shared_ptr<Parametrization>>& parametrizations() const { return p_; }

    /*! components per asset class, see below for specific model type inspectors */
    const boost::shared_ptr<Parametrization> ir(const Size ccy) const;
    const boost::shared_ptr<Parametrization> fx(const Size ccy) const;
    const boost::shared_ptr<Parametrization> inf(const Size i) const;
    const boost::shared_ptr<Parametrization> cr(const Size i) const;
    const boost::shared_ptr<Parametrization> eq(const Size i) const;

    /*! LGM1F components, ccy=0 refers to the domestic currency */
    const boost::shared_ptr<LinearGaussMarkovModel> lgm(const Size ccy) const;

    const boost::shared_ptr<IrLgm1fParametrization> irlgm1f(const Size ccy) const;

    /*! LGM measure numeraire */
    Real numeraire(const Size ccy, const Time t, const Real x,
                   Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    /*! Bank account measure numeraire B(t) as a function of drifted LGM state variable x and drift-free auxiliary state
     * variable y */
    Real bankAccountNumeraire(const Size ccy, const Time t, const Real x, const Real y,
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

    //! Inflation JY component
    const boost::shared_ptr<InfJyParameterization> infjy(const Size i) const;

    /*! CR LGM 1F components */
    const boost::shared_ptr<CrLgm1fParametrization> crlgm1f(const Size i) const;

    /*! CR CIR++ components */
    const boost::shared_ptr<CrCirpp> crcirppModel(const Size i) const;
    const boost::shared_ptr<CrCirppParametrization> crcirpp(const Size i) const;

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

    /*! get salvaging algorithm */
    SalvagingAlgorithm::Type salvagingAlgorithm() const { return salvaging_; }

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

    /*! returns (S(t), S^tilde(t,T)) in the notation of the book */
    std::pair<Real, Real> crcirppS(const Size i, const Time t, const Time T, const Real y, const Real s) const;

    /*! tentative: more generic interface that is agnostic of the model type - so far only for CR */
    virtual Handle<DefaultProbabilityTermStructure> crTs(const Size i) const;
    virtual std::pair<Real, Real> crS(const Size i, const Size ccy, const Time t, const Time T, const Real z,
                                      const Real y) const;

    /*! calibration procedures */

    /*! calibrate irlgm1f volatilities to a sequence of ir options with
        expiry times equal to step times in the parametrization */
    void calibrateIrLgm1fVolatilitiesIterative(const Size ccy,
                                               const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                               OptimizationMethod& method, const EndCriteria& endCriteria,
                                               const Constraint& constraint = Constraint(),
                                               const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate irlgm1f reversion to a sequence of ir options with
        maturities equal to step times in the parametrization */
    void calibrateIrLgm1fReversionsIterative(const Size ccy,
                                             const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                             OptimizationMethod& method, const EndCriteria& endCriteria,
                                             const Constraint& constraint = Constraint(),
                                             const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate irlgm1f parameters for one ccy globally to a set
        of ir options */
    void calibrateIrLgm1fGlobal(const Size ccy, const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                OptimizationMethod& method, const EndCriteria& endCriteria,
                                const Constraint& constraint = Constraint(),
                                const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate eq or fx volatilities to a sequence of options with
            expiry times equal to step times in the parametrization */
    void calibrateBsVolatilitiesIterative(const AssetType& assetType, const Size aIdx,
                                          const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                          OptimizationMethod& method, const EndCriteria& endCriteria,
                                          const Constraint& constraint = Constraint(),
                                          const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate eq/fx volatilities globally to a set of fx options */
    void calibrateBsVolatilitiesGlobal(const AssetType& assetType, const Size aIdx,
                                       const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                       OptimizationMethod& method, const EndCriteria& endCriteria,
                                       const Constraint& constraint = Constraint(),
                                       const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate infdk volatilities to a sequence of cpi options with
        expiry times equal to step times in the parametrization */
    void calibrateInfDkVolatilitiesIterative(const Size index,
                                             const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                             OptimizationMethod& method, const EndCriteria& endCriteria,
                                             const Constraint& constraint = Constraint(),
                                             const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate infdk reversions to a sequence of cpi options with
        maturity times equal to step times in the parametrization */
    void calibrateInfDkReversionsIterative(const Size index,
                                           const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                           OptimizationMethod& method, const EndCriteria& endCriteria,
                                           const Constraint& constraint = Constraint(),
                                           const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate infdk volatilities globally to a sequence of cpi cap/floors */
    void calibrateInfDkVolatilitiesGlobal(const Size index,
                                          const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                          OptimizationMethod& method, const EndCriteria& endCriteria,
                                          const Constraint& constraint = Constraint(),
                                          const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate infdk reversions globally to a sequence of cpi cap/floors */
    void calibrateInfDkReversionsGlobal(const Size index,
                                        const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                        OptimizationMethod& method, const EndCriteria& endCriteria,
                                        const Constraint& constraint = Constraint(),
                                        const std::vector<Real>& weights = std::vector<Real>());

    /*! Calibrate JY inflation parameters globally.

        The parameter \p toCalibrate indicates which parameters of the JY inflation model that we want to calibrate.
        The map key should be in {0, 1, 2} where 0 indicates the real rate volatility, 1 indicates the real rate
        reversion and 2 indicates the inflation index volatility. The value is \c true if we wish to calibrate
        the parameter and \p false if we do not want to calibrate it.
    */
    void calibrateInfJyGlobal(QuantLib::Size index,
                              const std::vector<boost::shared_ptr<QuantLib::CalibrationHelper>>& helpers,
                              QuantLib::OptimizationMethod& method, const QuantLib::EndCriteria& endCriteria,
                              const std::map<QuantLib::Size, bool>& toCalibrate,
                              const QuantLib::Constraint& constraint = QuantLib::Constraint(),
                              const std::vector<QuantLib::Real>& weights = std::vector<QuantLib::Real>());

    /*! Calibrate a single JY inflation parameter iteratively.

        Calibrate one of real rate volatility, real rate reversion or inflation index volatility. The
        \p parameterIndex indicates the parameter that should be calibrated where 0 indicates the real rate
        volatility, 1 indicates the real rate reversion and 2 indicates the inflation index volatility.
    */
    void calibrateInfJyIterative(QuantLib::Size inflationModelIndex, QuantLib::Size parameterIndex,
                                 const std::vector<boost::shared_ptr<QuantLib::CalibrationHelper>>& helpers,
                                 QuantLib::OptimizationMethod& method, const QuantLib::EndCriteria& endCriteria,
                                 const QuantLib::Constraint& constraint = QuantLib::Constraint(),
                                 const std::vector<QuantLib::Real>& weights = std::vector<QuantLib::Real>());

    /*! calibrate crlgm1f volatilities to a sequence of cds options with
        expiry times equal to step times in the parametrization */
    void calibrateCrLgm1fVolatilitiesIterative(const Size index,
                                               const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                               OptimizationMethod& method, const EndCriteria& endCriteria,
                                               const Constraint& constraint = Constraint(),
                                               const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate crlgm1f reversions to a sequence of cds options with
        maturity times equal to step times in the parametrization */
    void calibrateCrLgm1fReversionsIterative(const Size index,
                                             const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& helpers,
                                             OptimizationMethod& method, const EndCriteria& endCriteria,
                                             const Constraint& constraint = Constraint(),
                                             const std::vector<Real>& weights = std::vector<Real>());

    /* ... add more calibration procedures here ... */

protected:
    /* ctor to be used in extensions, initialize is not called */
    CrossAssetModel(const std::vector<boost::shared_ptr<Parametrization>>& parametrizations, const Matrix& correlation,
                    SalvagingAlgorithm::Type salvaging, Measure::Type measure, const bool)
        : LinkableCalibratedModel(), p_(parametrizations), rho_(correlation), salvaging_(salvaging), measure_(measure) {
    }

    /*! number of arguments for a component */
    Size arguments(const AssetType t, const Size i) const;

    /*! index of component in the arguments vector, by offset */
    Size aIdx(const AssetType t, const Size i, const Size offset = 0) const;

    /*! asset and model type for given parametrization */
    virtual std::pair<AssetType, ModelType> getComponentType(const Size i) const;
    /*! number of parameters for given parametrization */
    virtual Size getNumberOfParameters(const Size i) const;
    /*! number of brownians for given parametrization */
    virtual Size getNumberOfBrownians(const Size i) const;
    /*! number of state variables for given parametrization */
    virtual Size getNumberOfStateVariables(const Size i) const;
    /*! helper function to init component indices */
    void updateIndices(const AssetType& t, const Size i, const Size cIdx, const Size pIdx, const Size aIdx);

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

    // components per asset type
    std::vector<Size> components_;
    // indices per asset type and component number within asset type
    std::vector<std::vector<Size>> idx_, cIdx_, pIdx_, aIdx_, brownians_, stateVariables_, numArguments_;
    // counter
    Size totalDimension_, totalNumberOfBrownians_, totalNumberOfParameters_;
    // model type per asset type and component number within asset type
    std::vector<std::vector<ModelType>> modelType_;
    // parametrizations, models
    std::vector<boost::shared_ptr<Parametrization>> p_;
    std::vector<boost::shared_ptr<LinearGaussMarkovModel>> lgm_;
    std::vector<boost::shared_ptr<CrCirpp>> crcirppModel_;
    Matrix rho_;
    SalvagingAlgorithm::Type salvaging_;
    Measure::Type measure_;
    mutable boost::shared_ptr<Integrator> integrator_;
    boost::shared_ptr<CrossAssetStateProcess> stateProcessExact_, stateProcessEuler_;

    /* calibration constraints */

    void appendToFixedParameterVector(const AssetType t, const AssetType v, const Size param, const Size index,
                                      const Size i, std::vector<bool>& res) {
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

    // move parameter param (e.g. vol, reversion, or all if null) of asset type component t / index at step i (or at all
    // steps if i is null)
    Disposable<std::vector<bool>> MoveParameter(const AssetType t, const Size param, const Size index, const Size i) {
        QL_REQUIRE(param == Null<Size>() || param < arguments(t, index),
                   "parameter for " << t << " at " << index << " (" << param << ") out of bounds 0..."
                                    << arguments(t, index) - 1);
        std::vector<bool> res(0);
        appendToFixedParameterVector(IR, t, param, index, i, res);
        appendToFixedParameterVector(FX, t, param, index, i, res);
        appendToFixedParameterVector(INF, t, param, index, i, res);
        appendToFixedParameterVector(CR, t, param, index, i, res);
        appendToFixedParameterVector(EQ, t, param, index, i, res);
        if (measure_ == Measure::BA)
            appendToFixedParameterVector(AUX, t, param, index, i, res);
        return res;
    }
};

//! Utility function to return a handle to the inflation term structure given the inflation index.
QuantLib::Handle<QuantLib::ZeroInflationTermStructure>
inflationTermStructure(const boost::shared_ptr<CrossAssetModel>& model, QuantLib::Size index);

// inline

inline const boost::shared_ptr<StochasticProcess>
CrossAssetModel::stateProcess(CrossAssetStateProcess::discretization disc) const {
    return disc == CrossAssetStateProcess::exact ? stateProcessExact_ : stateProcessEuler_;
}

inline Size CrossAssetModel::dimension() const { return totalDimension_; }

inline Size CrossAssetModel::brownians() const { return totalNumberOfBrownians_; }

inline Size CrossAssetModel::totalNumberOfParameters() const { return totalNumberOfParameters_; }

inline const boost::shared_ptr<Parametrization> CrossAssetModel::ir(const Size ccy) const { return p_[idx(IR, ccy)]; }

inline const boost::shared_ptr<Parametrization> CrossAssetModel::fx(const Size ccy) const { return p_[idx(FX, ccy)]; }

inline const boost::shared_ptr<Parametrization> CrossAssetModel::inf(const Size i) const { return p_[idx(INF, i)]; }

inline const boost::shared_ptr<Parametrization> CrossAssetModel::cr(const Size i) const { return p_[idx(CR, i)]; }

inline const boost::shared_ptr<Parametrization> CrossAssetModel::eq(const Size i) const {
    return boost::static_pointer_cast<Parametrization>(p_[idx(EQ, i)]);
}

inline const boost::shared_ptr<LinearGaussMarkovModel> CrossAssetModel::lgm(const Size ccy) const {
    boost::shared_ptr<LinearGaussMarkovModel> tmp = lgm_[idx(IR, ccy)];
    QL_REQUIRE(tmp, "model at " << ccy << " is not IR-LGM1F");
    return tmp;
}

inline const boost::shared_ptr<IrLgm1fParametrization> CrossAssetModel::irlgm1f(const Size ccy) const {
    return lgm(ccy)->parametrization();
}

inline const boost::shared_ptr<InfDkParametrization> CrossAssetModel::infdk(const Size i) const {
    boost::shared_ptr<InfDkParametrization> tmp = boost::dynamic_pointer_cast<InfDkParametrization>(p_[idx(INF, i)]);
    QL_REQUIRE(tmp, "model at " << i << " is not INF-DK");
    return tmp;
}

inline const boost::shared_ptr<InfJyParameterization> CrossAssetModel::infjy(const Size i) const {
    auto tmp = boost::dynamic_pointer_cast<InfJyParameterization>(p_[idx(INF, i)]);
    QL_REQUIRE(tmp, "model at " << i << " is not INF-JY");
    return tmp;
}

inline const boost::shared_ptr<CrLgm1fParametrization> CrossAssetModel::crlgm1f(const Size i) const {
    boost::shared_ptr<CrLgm1fParametrization> tmp = boost::dynamic_pointer_cast<CrLgm1fParametrization>(p_[idx(CR, i)]);
    QL_REQUIRE(tmp, "model at " << i << " is not CR-LGM");
    return tmp;
}

inline const boost::shared_ptr<CrCirpp> CrossAssetModel::crcirppModel(const Size i) const {
    boost::shared_ptr<CrCirpp> tmp = crcirppModel_[i];
    QL_REQUIRE(tmp, "model at " << i << " is not CR-CIRPP");
    return tmp;
}

inline const boost::shared_ptr<CrCirppParametrization> CrossAssetModel::crcirpp(const Size i) const {
    boost::shared_ptr<CrCirppParametrization> tmp = boost::dynamic_pointer_cast<CrCirppParametrization>(p_[idx(CR, i)]);
    QL_REQUIRE(tmp, "model at " << i << " is not CR-CIRPP");
    return tmp;
}

inline const boost::shared_ptr<EqBsParametrization> CrossAssetModel::eqbs(const Size name) const {
    boost::shared_ptr<EqBsParametrization> tmp = boost::dynamic_pointer_cast<EqBsParametrization>(p_[idx(EQ, name)]);
    QL_REQUIRE(tmp, "model at " << name << " is not EQ-BS");
    return tmp;
}

inline Real CrossAssetModel::numeraire(const Size ccy, const Time t, const Real x,
                                       Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->numeraire(t, x, discountCurve);
}

inline Real CrossAssetModel::bankAccountNumeraire(const Size ccy, const Time t, const Real x, const Real y,
                                                  Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->bankAccountNumeraire(t, x, y, discountCurve);
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
    boost::shared_ptr<FxBsParametrization> tmp = boost::dynamic_pointer_cast<FxBsParametrization>(p_[idx(FX, ccy)]);
    QL_REQUIRE(tmp, "model at " << ccy << " is not FX-BS");
    return tmp;
}

inline const Matrix& CrossAssetModel::correlation() const { return rho_; }

inline const boost::shared_ptr<Integrator> CrossAssetModel::integrator() const { return integrator_; }

inline Handle<DefaultProbabilityTermStructure> CrossAssetModel::crTs(const Size i) const {
    if (modelType(CR, i) == LGM1F)
        return crlgm1f(i)->termStructure();
    if (modelType(CR, i) == CIRPP)
        return crcirpp(i)->termStructure();
    QL_FAIL("model at " << i << " is not CR-*");
}

inline std::pair<Real, Real> CrossAssetModel::crS(const Size i, const Size ccy, const Time t, const Time T,
                                                  const Real z, const Real y) const {
    if (modelType(CR, i) == LGM1F)
        return crlgm1fS(i, ccy, t, T, z, y);
    if (modelType(CR, i) == CIRPP) {
        QL_REQUIRE(ccy == 0, "CrossAssetModelPlus::crS() only implemented for ccy=0, got " << ccy);
        return crcirppS(i, t, T, z, y);
    }
    QL_FAIL("model at " << i << " is not CR-*");
}

} // namespace QuantExt

#endif
