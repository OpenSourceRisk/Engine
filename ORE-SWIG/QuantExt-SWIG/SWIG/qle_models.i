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

%{
using QuantExt::LinkableCalibratedModel;
using QuantExt::ModelBuilder;
using QuantExt::Parametrization;
using QuantExt::IrModel;
using QuantExt::FxBsParametrization;
using QuantExt::EqBsParametrization;
using QuantExt::IrLgm1fParametrization;
using QuantExt::IrHwParametrization;
using QuantExt::InfDkParametrization;
using QuantExt::LinearGaussMarkovModel;
using QuantExt::HwModel;
using QuantExt::CrossAssetModel;
using QuantExt::HullWhiteBucketing;
using QuantExt::Bucketing;
using QuantExt::sanitiseTransitionMatrix;
using QuantExt::checkTransitionMatrix;
using QuantExt::checkGeneratorMatrix;
using QuantExt::generator;
using QuantExt::CrossAssetStateProcess;
using QuantLib::Matrix;
using QuantLib::Array;
%}

%shared_ptr(CrossAssetStateProcess)
class CrossAssetStateProcess;

%shared_ptr(ModelBuilder)
%nodefaultctor ModelBuilder;
class ModelBuilder : public LazyObject {
public:
    virtual void recalibrate() const;
    virtual void forceRecalculate();
    virtual bool requiresRecalibration() const = 0;
    virtual void newCalcWithoutRecalibration() const;
};

%shared_ptr(LinkableCalibratedModel)
class LinkableCalibratedModel {
public:
    EndCriteria::Type endCriteria() const;
    const Array& problemValues() const;
    Array params() const;
    virtual void setParams(const Array& params);
    virtual void setParam(Size idx, const Real value);
};

%shared_ptr(Parametrization)
%nodefaultctor Parametrization;
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

%shared_ptr(IrModel)
%nodefaultctor IrModel;
class IrModel : public LinkableCalibratedModel {
public:
    enum class Measure { LGM, BA };
    virtual Measure measure() const = 0;
    virtual const QuantLib::ext::shared_ptr<Parametrization> parametrizationBase() const = 0;
    virtual Handle<YieldTermStructure> termStructure() const = 0;
    virtual Size n() const = 0;
    virtual Size m() const = 0;
    virtual Size n_aux() const = 0;
    virtual Size m_aux() const = 0;
    virtual QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const = 0;
};

%shared_ptr(FxBsParametrization)
%nodefaultctor FxBsParametrization;
class FxBsParametrization : public Parametrization {
public:
    virtual Real variance(const Time t) const = 0;
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<Quote> fxSpotToday() const;
};

%shared_ptr(EqBsParametrization)
%nodefaultctor EqBsParametrization;
class EqBsParametrization : public Parametrization {
public:
    virtual Real variance(const Time t) const = 0;
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<Quote> eqSpotToday() const;
    const Handle<Quote> fxSpotToday() const;
    const Handle<YieldTermStructure> equityIrCurveToday() const;
    const Handle<YieldTermStructure> equityDivYieldCurveToday() const;
};

%shared_ptr(IrLgm1fParametrization)
%nodefaultctor IrLgm1fParametrization;
class IrLgm1fParametrization : public Parametrization {
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

%shared_ptr(IrHwParametrization)
%nodefaultctor IrHwParametrization;
class IrHwParametrization : public Parametrization {
public:
    virtual Matrix sigma_x(const Time t) const = 0;
    virtual Array kappa(const Time t) const = 0;
    virtual Matrix y(const Time t) const;
    virtual Array g(const Time t, const Time T) const;
    const Handle<YieldTermStructure> termStructure() const;
    Size n() const;
    Size m() const;
};

%shared_ptr(InfDkParametrization)
%nodefaultctor InfDkParametrization;
class InfDkParametrization : public Parametrization {
public:
    virtual Real zeta(const Time t) const = 0;
    virtual Real H(const Time t) const = 0;
    virtual Real alpha(const Time t) const;
    virtual Real kappa(const Time t) const;
    virtual Real Hprime(const Time t) const;
    virtual Real Hprime2(const Time t) const;
    virtual Real hullWhiteSigma(const Time t) const;
};

%shared_ptr(LinearGaussMarkovModel)
class LinearGaussMarkovModel : public IrModel {
public:
    enum class Discretization { Euler, Exact };
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const override;
    const QuantLib::ext::shared_ptr<IrLgm1fParametrization> parametrization() const;
    Real numeraire(const Time t, const Real x,
                   const Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;
    Real bankAccountNumeraire(const Time t, const Real x, const Real y,
                              const Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;
    Real discountBond(const Time t, const Time T, const Real x,
                      Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;
};

%shared_ptr(HwModel)
class HwModel : public IrModel {
public:
    enum class Discretization { Euler, Exact };
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const override;
    const QuantLib::ext::shared_ptr<IrHwParametrization> parametrization() const;
};

%shared_ptr(CrossAssetModel)
%nodefaultctor CrossAssetModel;
class CrossAssetModel : public LinkableCalibratedModel {
public:
    enum class AssetType : Size { IR = 0, FX = 1, INF = 2, CR = 3, EQ = 4, COM = 5, CrState = 6 };
    enum class ModelType { LGM1F, HW, BS, LV, DK, CIRPP, JY, GENERIC };
    enum class Discretization { Euler, Exact, BestMarginalDiscretization };

    QuantLib::ext::shared_ptr<CrossAssetStateProcess> stateProcess() const;
    Size dimension() const;
    Size brownians() const;
    Size auxBrownians() const;
    Size totalNumberOfParameters() const;
    Size components(const AssetType t) const;
    ModelType modelType(const AssetType t, const Size i) const;
    IrModel::Measure measure() const;
    const std::vector<QuantLib::ext::shared_ptr<Parametrization>>& parametrizations() const;
    const Matrix& correlation() const;
    Discretization discretization() const;
};

%shared_ptr(Bucketing)
class Bucketing {
public:
    Bucketing(const Real lowerBound, const Real upperBound, const Size n);
    const std::vector<Real>& upperBucketBound() const;
    Size index(const Real x) const;
    Size buckets() const;
};

%shared_ptr(HullWhiteBucketing)
class HullWhiteBucketing : public Bucketing {
public:
    HullWhiteBucketing(const Real lowerBound, const Real upperBound, const Size n);
    const Array& probability() const;
    const Array& averageLoss() const;
};

void sanitiseTransitionMatrix(Matrix& m);
void checkTransitionMatrix(const Matrix& t);
void checkGeneratorMatrix(const Matrix& g);
Matrix generator(const Matrix& t, const Real horizon = 1.0);

#endif
