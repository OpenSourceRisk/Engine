/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef qle_models_i
#define qle_models_i

%include common.i
%include date.i
%include termstructures.i
%include stochasticprocess.i
%include vectors.i

%shared_ptr(QuantExt::CrossAssetStateProcess)
namespace QuantExt {
class CrossAssetStateProcess;
}

%shared_ptr(QuantExt::ModelBuilder)
%nodefaultctor QuantExt::ModelBuilder;
namespace QuantExt {
class ModelBuilder : public LazyObject {
public:
    virtual void recalibrate() const;
    virtual void forceRecalculate();
    virtual bool requiresRecalibration() const = 0;
    virtual void newCalcWithoutRecalibration() const;
};
}

%shared_ptr(QuantExt::LinkableCalibratedModel)
namespace QuantExt {
class LinkableCalibratedModel {
public:
    EndCriteria::Type endCriteria() const;
    const Array& problemValues() const;
    Array params() const;
    virtual void setParams(const Array& params);
    virtual void setParam(Size idx, const Real value);
};
}

%shared_ptr(QuantExt::Parametrization)
%nodefaultctor QuantExt::Parametrization;
namespace QuantExt {
class Parametrization {
public:
    virtual const Currency& currency() const;
    virtual const Array& parameterTimes(const Size) const;
    virtual Size numberOfParameters() const;
    virtual Array parameterValues(const Size) const;
    virtual void update() const;
    const std::string& name() const;
    virtual Real direct(const Size, const Real x) const;
    virtual Real inverse(const Size, const Real y) const;
};
}

%shared_ptr(QuantExt::IrModel)
%nodefaultctor QuantExt::IrModel;
namespace QuantExt {
class IrModel : public QuantExt::LinkableCalibratedModel {
public:
    enum class Measure { LGM, BA };
    virtual QuantExt::IrModel::Measure measure() const = 0;
    virtual const QuantLib::ext::shared_ptr<QuantExt::Parametrization> parametrizationBase() const = 0;
    virtual Handle<YieldTermStructure> termStructure() const = 0;
    virtual Size n() const = 0;
    virtual Size m() const = 0;
    virtual Size n_aux() const = 0;
    virtual Size m_aux() const = 0;
    virtual QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const = 0;
};
}

%shared_ptr(QuantExt::FxBsParametrization)
%nodefaultctor QuantExt::FxBsParametrization;
namespace QuantExt {
class FxBsParametrization : public QuantExt::Parametrization {
public:
    virtual Real variance(const Time t) const = 0;
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<Quote> fxSpotToday() const;
};
}

%feature("notabstract") QuantExt::FxBsConstantParametrization;
%shared_ptr(QuantExt::FxBsConstantParametrization)
namespace QuantExt {
class FxBsConstantParametrization : public QuantExt::FxBsParametrization {
public:
    FxBsConstantParametrization(const Currency& currency, const Handle<Quote>& fxSpotToday, const Real sigma);
};
}

%feature("notabstract") QuantExt::FxBsPiecewiseConstantParametrization;
%shared_ptr(QuantExt::FxBsPiecewiseConstantParametrization)
namespace QuantExt {
class FxBsPiecewiseConstantParametrization : public QuantExt::FxBsParametrization {
public:
    FxBsPiecewiseConstantParametrization(const Currency& currency, const Handle<Quote>& fxSpotToday,
                                         const Array& times, const Array& sigma);
};
}

%shared_ptr(QuantExt::EqBsParametrization)
%nodefaultctor QuantExt::EqBsParametrization;
namespace QuantExt {
class EqBsParametrization : public QuantExt::Parametrization {
public:
    virtual Real variance(const Time t) const = 0;
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<Quote> eqSpotToday() const;
    const Handle<Quote> fxSpotToday() const;
    const Handle<YieldTermStructure> equityIrCurveToday() const;
    const Handle<YieldTermStructure> equityDivYieldCurveToday() const;
};
}

%feature("notabstract") QuantExt::EqBsConstantParametrization;
%shared_ptr(QuantExt::EqBsConstantParametrization)
namespace QuantExt {
class EqBsConstantParametrization : public QuantExt::EqBsParametrization {
public:
    EqBsConstantParametrization(const Currency& currency, const std::string& eqName,
                                const Handle<Quote>& eqSpotToday, const Handle<Quote>& fxSpotToday,
                                const Real sigma, const Handle<YieldTermStructure>& eqIrCurveToday,
                                const Handle<YieldTermStructure>& eqDivYieldCurveToday);
};
}

%shared_ptr(QuantExt::IrLgm1fParametrization)
%nodefaultctor QuantExt::IrLgm1fParametrization;
namespace QuantExt {
class IrLgm1fParametrization : public QuantExt::Parametrization {
public:
    virtual Real zeta(const Time t) const = 0;
    virtual Real H(const Time t) const = 0;
    virtual Real alpha(const Time t) const;
    virtual Real kappa(const Time t) const;
    virtual Real Hprime(const Time t) const;
    virtual Real Hprime2(const Time t) const;
    virtual Real hullWhiteSigma(const Time t) const;
    const Handle<YieldTermStructure> termStructure() const;
    Real& shift();
    Real& scaling();
};
}

%feature("notabstract") QuantExt::IrLgm1fConstantParametrization;
%shared_ptr(QuantExt::IrLgm1fConstantParametrization)
namespace QuantExt {
class IrLgm1fConstantParametrization : public QuantExt::IrLgm1fParametrization {
public:
    IrLgm1fConstantParametrization(const Currency& currency, const Handle<YieldTermStructure>& termStructure,
                                   const Real alpha, const Real kappa,
                                   const std::string& name = std::string());
};
}

%feature("notabstract") QuantExt::IrLgm1fPiecewiseConstantParametrization;
%shared_ptr(QuantExt::IrLgm1fPiecewiseConstantParametrization)
namespace QuantExt {
class IrLgm1fPiecewiseConstantParametrization : public QuantExt::IrLgm1fParametrization {
public:
    IrLgm1fPiecewiseConstantParametrization(const Currency& currency, const Handle<YieldTermStructure>& termStructure,
                                            const Array& alphaTimes, const Array& alpha,
                                            const Array& kappaTimes, const Array& kappa,
                                            const std::string& name = std::string());
};
}

%shared_ptr(QuantExt::IrHwParametrization)
%nodefaultctor QuantExt::IrHwParametrization;
namespace QuantExt {
class IrHwParametrization : public QuantExt::Parametrization {
public:
    virtual Matrix sigma_x(const Time t) const = 0;
    virtual Array kappa(const Time t) const = 0;
    virtual Matrix y(const Time t) const;
    virtual Array g(const Time t, const Time T) const;
    const Handle<YieldTermStructure> termStructure() const;
    Size n() const;
    Size m() const;
};
}

%feature("notabstract") QuantExt::IrHwConstantParametrization;
%shared_ptr(QuantExt::IrHwConstantParametrization)
namespace QuantExt {
class IrHwConstantParametrization : public QuantExt::IrHwParametrization {
public:
    IrHwConstantParametrization(const Currency& currency, const Handle<YieldTermStructure>& termStructure,
                                const Matrix& sigma, const Array& kappa,
                                const std::string& name = std::string());
};
}

%shared_ptr(QuantExt::InfDkParametrization)
%nodefaultctor QuantExt::InfDkParametrization;
namespace QuantExt {
class InfDkParametrization : public QuantExt::Parametrization {
public:
    virtual Real zeta(const Time t) const = 0;
    virtual Real H(const Time t) const = 0;
    virtual Real alpha(const Time t) const;
    virtual Real kappa(const Time t) const;
    virtual Real Hprime(const Time t) const;
    virtual Real Hprime2(const Time t) const;
    virtual Real hullWhiteSigma(const Time t) const;
};
}

%feature("notabstract") QuantExt::LinearGaussMarkovModel;
%shared_ptr(QuantExt::LinearGaussMarkovModel)
namespace QuantExt {
class LinearGaussMarkovModel : public QuantExt::IrModel {
public:
    enum class Discretization { Euler, Exact, ExactGlobal };
    LinearGaussMarkovModel(const QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization>& parametrization,
                           const QuantExt::IrModel::Measure measure = QuantExt::IrModel::Measure::LGM,
                           const QuantExt::LinearGaussMarkovModel::Discretization discretization =
                               QuantExt::LinearGaussMarkovModel::Discretization::Euler,
                           const bool evaluateBankAccount = true);
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const override;
    const QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization> parametrization() const;
    Real numeraire(const Time t, const Real x,
                   const Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;
    Real bankAccountNumeraire(const Time t, const Real x, const Real y,
                              const Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;
    Real discountBond(const Time t, const Time T, const Real x,
                      Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;
};
}

%feature("notabstract") QuantExt::HwModel;
%shared_ptr(QuantExt::HwModel)
namespace QuantExt {
class HwModel : public QuantExt::IrModel {
public:
    enum class Discretization { Euler, Exact };
    HwModel(const QuantLib::ext::shared_ptr<QuantExt::IrHwParametrization>& parametrization,
            const QuantExt::IrModel::Measure measure = QuantExt::IrModel::Measure::BA,
            const QuantExt::HwModel::Discretization discretization = QuantExt::HwModel::Discretization::Euler,
            const bool evaluateBankAccount = true);
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const override;
    const QuantLib::ext::shared_ptr<QuantExt::IrHwParametrization> parametrization() const;
};
}

// SWIG cannot auto-upcast shared_ptr<Derived> to shared_ptr<Base> in typemaps.
// Use ext:: (not QuantLib::ext::) to match the type descriptor that %shared_ptr generates.
%extend QuantExt::LinearGaussMarkovModel {
    LinearGaussMarkovModel(const ext::shared_ptr<QuantExt::IrLgm1fConstantParametrization>& p,
                           QuantExt::IrModel::Measure measure = QuantExt::IrModel::Measure::LGM,
                           QuantExt::LinearGaussMarkovModel::Discretization discretization =
                               QuantExt::LinearGaussMarkovModel::Discretization::Euler,
                           bool evaluateBankAccount = true) {
        return new QuantExt::LinearGaussMarkovModel(
            ext::static_pointer_cast<QuantExt::IrLgm1fParametrization>(p),
            measure, discretization, evaluateBankAccount);
    }
    LinearGaussMarkovModel(const ext::shared_ptr<QuantExt::IrLgm1fPiecewiseConstantParametrization>& p,
                           QuantExt::IrModel::Measure measure = QuantExt::IrModel::Measure::LGM,
                           QuantExt::LinearGaussMarkovModel::Discretization discretization =
                               QuantExt::LinearGaussMarkovModel::Discretization::Euler,
                           bool evaluateBankAccount = true) {
        return new QuantExt::LinearGaussMarkovModel(
            ext::static_pointer_cast<QuantExt::IrLgm1fParametrization>(p),
            measure, discretization, evaluateBankAccount);
    }
}

%extend QuantExt::HwModel {
    HwModel(const ext::shared_ptr<QuantExt::IrHwConstantParametrization>& p,
            QuantExt::IrModel::Measure measure = QuantExt::IrModel::Measure::BA,
            QuantExt::HwModel::Discretization discretization = QuantExt::HwModel::Discretization::Euler,
            bool evaluateBankAccount = true) {
        return new QuantExt::HwModel(
            ext::static_pointer_cast<QuantExt::IrHwParametrization>(p),
            measure, discretization, evaluateBankAccount);
    }
}

// Vector templates for model constructors — use ext:: to match %shared_ptr type descriptors.
%template(ParametrizationVector) std::vector<ext::shared_ptr<QuantExt::Parametrization>>;
%template(IrModelVector) std::vector<ext::shared_ptr<QuantExt::IrModel>>;
%template(FxBsParametrizationVector) std::vector<ext::shared_ptr<QuantExt::FxBsParametrization>>;

// Explicit upcast helpers: use ext:: (not QuantLib::ext::) to match %shared_ptr type descriptors.
%inline %{
ext::shared_ptr<QuantExt::Parametrization>
toParametrization(const ext::shared_ptr<QuantExt::IrLgm1fConstantParametrization>& p) {
    return ext::static_pointer_cast<QuantExt::Parametrization>(p);
}
ext::shared_ptr<QuantExt::Parametrization>
toParametrization(const ext::shared_ptr<QuantExt::IrLgm1fPiecewiseConstantParametrization>& p) {
    return ext::static_pointer_cast<QuantExt::Parametrization>(p);
}
ext::shared_ptr<QuantExt::Parametrization>
toParametrization(const ext::shared_ptr<QuantExt::FxBsConstantParametrization>& p) {
    return ext::static_pointer_cast<QuantExt::Parametrization>(p);
}
ext::shared_ptr<QuantExt::Parametrization>
toParametrization(const ext::shared_ptr<QuantExt::FxBsPiecewiseConstantParametrization>& p) {
    return ext::static_pointer_cast<QuantExt::Parametrization>(p);
}
ext::shared_ptr<QuantExt::FxBsParametrization>
toFxBsParametrization(const ext::shared_ptr<QuantExt::FxBsConstantParametrization>& p) {
    return ext::static_pointer_cast<QuantExt::FxBsParametrization>(p);
}
ext::shared_ptr<QuantExt::FxBsParametrization>
toFxBsParametrization(const ext::shared_ptr<QuantExt::FxBsPiecewiseConstantParametrization>& p) {
    return ext::static_pointer_cast<QuantExt::FxBsParametrization>(p);
}
ext::shared_ptr<QuantExt::IrModel>
toIrModel(const ext::shared_ptr<QuantExt::LinearGaussMarkovModel>& p) {
    return ext::static_pointer_cast<QuantExt::IrModel>(p);
}
ext::shared_ptr<QuantExt::IrModel>
toIrModel(const ext::shared_ptr<QuantExt::HwModel>& p) {
    return ext::static_pointer_cast<QuantExt::IrModel>(p);
}
%}

%feature("notabstract") QuantExt::CrossAssetModel;
%shared_ptr(QuantExt::CrossAssetModel)
namespace QuantExt {
class CrossAssetModel : public QuantExt::LinkableCalibratedModel {
public:
    enum class AssetType : Size { IR = 0, FX = 1, INF = 2, CR = 3, EQ = 4, COM = 5, CrState = 6 };
    enum class ModelType { LGM1F, HW, BS, LV, DK, CIRPP, JY, GENERIC };
    enum class Discretization { Euler, Exact, BestMarginalDiscretization };

    CrossAssetModel(const std::vector<ext::shared_ptr<QuantExt::Parametrization>>& parametrizations,
                    const Matrix& correlation = Matrix(),
                    const SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None,
                    const QuantExt::IrModel::Measure measure = QuantExt::IrModel::Measure::LGM,
                    const QuantExt::CrossAssetModel::Discretization discretization =
                        QuantExt::CrossAssetModel::Discretization::Exact);

    CrossAssetModel(const std::vector<ext::shared_ptr<QuantExt::IrModel>>& currencyModels,
                    const std::vector<ext::shared_ptr<QuantExt::FxBsParametrization>>& fxParametrizations,
                    const Matrix& correlation = Matrix(),
                    const SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None,
                    const QuantExt::IrModel::Measure measure = QuantExt::IrModel::Measure::LGM,
                    const QuantExt::CrossAssetModel::Discretization discretization =
                        QuantExt::CrossAssetModel::Discretization::Exact);

    QuantLib::ext::shared_ptr<QuantExt::CrossAssetStateProcess> stateProcess() const;
    Size dimension() const;
    Size brownians() const;
    Size auxBrownians() const;
    Size totalNumberOfParameters() const;
    Size components(const QuantExt::CrossAssetModel::AssetType t) const;
    QuantExt::CrossAssetModel::ModelType modelType(const QuantExt::CrossAssetModel::AssetType t, const Size i) const;
    QuantExt::IrModel::Measure measure() const;
    const std::vector<ext::shared_ptr<QuantExt::Parametrization>>& parametrizations() const;
    const Matrix& correlation() const;
    QuantExt::CrossAssetModel::Discretization discretization() const;
};
}

%shared_ptr(QuantExt::FxEqOptionHelper)
namespace QuantExt {
class FxEqOptionHelper : public BlackCalibrationHelper {
public:
    FxEqOptionHelper(const Period& maturity, const Calendar& calendar, const Real strike,
                     const Handle<Quote> spot, const Handle<Quote> volatility,
                     const Handle<YieldTermStructure>& domesticYield,
                     const Handle<YieldTermStructure>& foreignYield,
                     BlackCalibrationHelper::CalibrationErrorType errorType =
                         BlackCalibrationHelper::RelativePriceError);
    FxEqOptionHelper(const Date& exerciseDate, const Real strike,
                     const Handle<Quote> spot, const Handle<Quote> volatility,
                     const Handle<YieldTermStructure>& domesticYield,
                     const Handle<YieldTermStructure>& foreignYield,
                     BlackCalibrationHelper::CalibrationErrorType errorType =
                         BlackCalibrationHelper::RelativePriceError);
};
}

%shared_ptr(QuantExt::Bucketing)
namespace QuantExt {
class Bucketing {
public:
    Bucketing(const Real lowerBound, const Real upperBound, const Size n);
    const std::vector<Real>& upperBucketBound() const;
    Size index(const Real x) const;
    Size buckets() const;
};
}

%shared_ptr(QuantExt::HullWhiteBucketing)
namespace QuantExt {
class HullWhiteBucketing : public QuantExt::Bucketing {
public:
    HullWhiteBucketing(const Real lowerBound, const Real upperBound, const Size n);
    const Array& probability() const;
    const Array& averageLoss() const;
};

void sanitiseTransitionMatrix(Matrix& m);
void checkTransitionMatrix(const Matrix& t);
void checkGeneratorMatrix(const Matrix& g);
Matrix generator(const Matrix& t, const Real horizon = 1.0);
}

#endif
