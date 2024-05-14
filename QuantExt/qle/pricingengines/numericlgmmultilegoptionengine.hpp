/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#pragma once

#include <qle/instruments/multilegoption.hpp>
#include <qle/models/lgmbackwardsolver.hpp>
#include <qle/models/lgmvectorised.hpp>

#include <ql/instruments/nonstandardswaption.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>

namespace QuantExt {

class NumericLgmMultiLegOptionEngineBase {
public:
    NumericLgmMultiLegOptionEngineBase(const QuantLib::ext::shared_ptr<LgmBackwardSolver>& solver,
                                       const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                       const Size americanExerciseTimeStepsPerYear = 24);

    static bool instrumentIsHandled(const MultiLegOption& m, std::vector<std::string>& messages);

protected:
    static bool instrumentIsHandled(const std::vector<Leg>& legs, const std::vector<bool>& payer,
                                    const std::vector<Currency>& currency, const QuantLib::ext::shared_ptr<Exercise>& exercise,
                                    const Settlement::Type& settlementType, const Settlement::Method& settlementMethod,
                                    std::vector<std::string>& messages);

    struct CashflowInfo {
        bool isPartOfUnderlying(const Real optionTime) const;
        bool canBeEstimated(const Real optionTime) const;
        bool mustBeEstimated(const Real optionTime) const;
        Real requiredSimulationTime() const;
        Real couponRatio(const Real time) const;
        RandomVariable pv(const LgmVectorised& lgm, const Real t, const RandomVariable& state,
                          const Handle<YieldTermStructure>& discountCurve) const;
        Real couponStartTime_ = Null<Real>();            // filled for classes derived from Coupon
        Real couponEndTime_ = Null<Real>();              // filled for classes derived from Coupon
        Real belongsToUnderlyingMaxTime_ = Null<Real>(); // this is always filled
        Real maxEstimationTime_ = Null<Real>();          // either this or exactEstimationTime is filled
        Real exactEstimationTime_ = Null<Real>();        // ...
        std::function<RandomVariable(const LgmVectorised&, const Real, const RandomVariable&,
                                     const Handle<YieldTermStructure>&)>
            calculator_; // always a valid function
    };

    CashflowInfo buildCashflowInfo(const Size i, const Size j) const;

    void calculate() const;

    // inputs set in ctor
    QuantLib::ext::shared_ptr<LgmBackwardSolver> solver_;
    Handle<YieldTermStructure> discountCurve_;
    Size americanExerciseTimeStepsPerYear_;

    // inputs set by derived classes
    mutable std::vector<Leg> legs_;
    mutable std::vector<bool> payer_;
    mutable std::vector<Currency> currency_;
    mutable QuantLib::ext::shared_ptr<Exercise> exercise_;
    mutable Settlement::Type settlementType_;
    mutable Settlement::Method settlementMethod_;

    // outputs
    mutable Real npv_, underlyingNpv_;
    mutable std::map<std::string, boost::any> additionalResults_;
};

class NumericLgmMultiLegOptionEngine
    : public QuantLib::GenericEngine<MultiLegOption::arguments, MultiLegOption::results>,
      public NumericLgmMultiLegOptionEngineBase {
public:
    NumericLgmMultiLegOptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                                   const Real sx, const Size nx,
                                   const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                   const Size americanExerciseTimeStepsPerYear = 24);

    NumericLgmMultiLegOptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real maxTime = 50.0,
                                   const QuantLib::FdmSchemeDesc scheme = QuantLib::FdmSchemeDesc::Douglas(),
                                   const Size stateGridPoints = 64, const Size timeStepsPerYear = 24,
                                   const Real mesherEpsilon = 1E-4,
                                   const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                   const Size americanExerciseTimeStepsPerYear = 24);

    void calculate() const override;
};

class NumericLgmSwaptionEngine : public QuantLib::GenericEngine<Swaption::arguments, Swaption::results>,
                                 public NumericLgmMultiLegOptionEngineBase {
public:
    NumericLgmSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                             const Real sx, const Size nx,
                             const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                             const Size americanExerciseTimeStepsPerYear = 24);

    NumericLgmSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real maxTime = 50.0,
                             const QuantLib::FdmSchemeDesc scheme = QuantLib::FdmSchemeDesc::Douglas(),
                             const Size stateGridPoints = 64, const Size timeStepsPerYear = 24,
                             const Real mesherEpsilon = 1E-4,
                             const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                             const Size americanExerciseTimeStepsPerYear = 24);

    void calculate() const override;
};

class NumericLgmNonstandardSwaptionEngine
    : public QuantLib::GenericEngine<NonstandardSwaption::arguments, NonstandardSwaption::results>,
      public NumericLgmMultiLegOptionEngineBase {
public:
    NumericLgmNonstandardSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy,
                                        const Size ny, const Real sx, const Size nx,
                                        const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                        const Size americanExerciseTimeStepsPerYear = 24);

    NumericLgmNonstandardSwaptionEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                        const Real maxTime = 50.0,
                                        const QuantLib::FdmSchemeDesc scheme = QuantLib::FdmSchemeDesc::Douglas(),
                                        const Size stateGridPoints = 64, const Size timeStepsPerYear = 24,
                                        const Real mesherEpsilon = 1E-4,
                                        const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                        const Size americanExerciseTimeStepsPerYear = 24);

    void calculate() const override;
};

} // namespace QuantExt
