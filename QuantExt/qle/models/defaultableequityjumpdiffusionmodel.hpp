/*
 Copyright (C) 2021 Quaternion Risk Management Ltd.

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

/*! \file qle/models/defaultableequityjumpdiffusionmodel.hpp */

#pragma once

#include <qle/models/marketobserver.hpp>
#include <qle/models/modelbuilder.hpp>
#include <qle/indexes/equityindex.hpp>

#include <ql/methods/finitedifferences/meshers/fdm1dmesher.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/types.hpp>

#include <boost/enable_shared_from_this.hpp>

namespace QuantExt {

using QuantLib::Handle;
using QuantLib::Real;

class DefaultableEquityJumpDiffusionModel;

class DefaultableEquityJumpDiffusionModelBuilder : public ModelBuilder {
public:
    enum class BootstrapMode { Alternating, Simultaneously };

    DefaultableEquityJumpDiffusionModelBuilder(
        const std::vector<Real>& stepTimes, const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equity,
        const Handle<QuantLib::BlackVolTermStructure>& volatility,
        const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve, const Real p = 0.0, const Real eta = 1.0,
        const bool staticMesher = false, const Size timeStepsPerYear = 24, const Size stateGridPoints = 100,
        const Real mesherEpsilon = 1E-4, const Real mesherScaling = 1.5, const Real mesherConcentration = Null<Real>(),
        const BootstrapMode mode = BootstrapMode::Alternating, const bool enforceFokkerPlanckBootstrap = false,
        const bool calibrate = true, const bool adjustEquityVolatility = true, const bool adjustEquityForward = true);

    Handle<DefaultableEquityJumpDiffusionModel> model() const;

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

private:
    void performCalculations() const override;
    bool calibrationPointsChanged(const bool updateCache) const;

    // input data
    std::vector<Real> stepTimes_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equity_;
    Handle<BlackVolTermStructure> volatility_;
    Handle<DefaultProbabilityTermStructure> creditCurve_;
    Real p_, eta_;
    bool staticMesher_;
    Size timeStepsPerYear_;
    Size stateGridPoints_;
    Real mesherEpsilon_;
    Real mesherScaling_;
    Real mesherConcentration_;
    BootstrapMode bootstrapMode_;
    bool enforceFokkerPlanckBootstrap_;
    bool calibrate_;
    bool adjustEquityVolatility_;
    bool adjustEquityForward_;

    bool forceCalibration_ = false;
    QuantLib::ext::shared_ptr<MarketObserver> marketObserver_;

    mutable std::vector<Real> cachedForwards_;
    mutable std::vector<Real> cachedVariances_;

    mutable RelinkableHandle<DefaultableEquityJumpDiffusionModel> model_;
};

class DefaultableEquityJumpDiffusionModel : public QuantLib::Observable,
                                            public QuantLib::Observer,
                                            public QuantLib::ext::enable_shared_from_this<DefaultableEquityJumpDiffusionModel> {
public:
    /*   Jump-Diffusion model for a defaultable equity

         dS / S(t^-) = (r(t) - q(t) + \eta h(t, S(t^-))) dt + \sigma(t) dW(t) - \eta dN(t)

         with h(t, S(t)) = h0(t) (S(0)/S(t))^p and h0(t), sigma(t) piecewise flat w.r.t. a given time grid.

         eta       is a given, fixed model parameter (default loss fraction for the equity price)
         p         is a given, fixed model parameter
         r(t)      is the equity forecast curve
         q(t)      is the equity dividend curve
         h(t,S)    is calibrated to the given credit curve
         \sigma(t) is calibrated to the given equity vol surface

         Reference: Andersen, L., and Buffum, D.: Calibration and Implementation of Convertible Bond Models (2002)

         If adjustEquityVolatility = false, the market equity volatitlies will be used without adjustment accounting
         for the hazard rate h(t). This option is only available for p=0.

         If adjustEquityForward = false, the equity drift wil not be adjusted by \eta h(t, S(t^-)).
    */

    DefaultableEquityJumpDiffusionModel(const std::vector<Real>& stepTimes, const std::vector<Real>& h0,
                                        const std::vector<Real>& sigma,
                                        const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equity,
                                        const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
                                        const DayCounter& volDayCounter, const Real p = 0.0, const Real eta = 1.0,
                                        const bool adjustEquityForward = true);

    // inspectors for input data
    const std::vector<Real>& stepTimes() const;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equity() const;
    Real totalBlackVariance() const;
    const DayCounter& volDayCounter() const;
    Handle<QuantLib::DefaultProbabilityTermStructure> creditCurve() const;
    Real eta() const;
    Real p() const;
    bool adjustEquityForward() const;

    Real timeFromReference(const Date& d) const;

    // piecewise constant model parameters w.r.t. stepTimes()
    const std::vector<Real>& h0() const;
    const std::vector<Real>& sigma() const;

    // inspectors for model functions
    Real h(const Real t, const Real S) const;
    Real h0(const Real t) const;
    Real r(const Real t) const;
    Real q(const Real t) const;
    Real sigma(const Real t) const;

    // annualised dividend yield over 0 <= s < t
    Real dividendYield(const Real s, const Real t) const;

    // bootstrap the model parameters
    void bootstrap(const Handle<QuantLib::BlackVolTermStructure>& volatility, const bool staticMesher,
                   const Size timeStepsPerYear, const Size stateGridPoints = 100, const Real mesherEpsilon = 1E-4,
                   const Real mesherScaling = 1.5, const Real mesherConcentration = Null<Real>(),
                   const DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode bootstrapMode =
                       DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode::Alternating,
                   const bool enforceFokkerPlanckBootstrap = false, const bool adjustEquityVolatility = true) const;

    // Observer interface
    void update() override;

private:
    QuantLib::Size getTimeIndex(const Real t) const;

    // input data
    std::vector<Real> stepTimes_;
    mutable std::vector<Real> h0_, sigma_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equity_;
    Handle<DefaultProbabilityTermStructure> creditCurve_;
    DayCounter volDayCounter_;
    Real p_, eta_;
    bool adjustEquityForward_;

    // mesher used to solve Fokker-Planck equation
    mutable QuantLib::ext::shared_ptr<Fdm1dMesher> mesher_;

    // input black variance at last time step
    mutable Real totalBlackVariance_ = 1.0;

    // step size to compute r, q with finite differences
    static constexpr Real fh_ = 1E-4;
};

} // namespace QuantExt
