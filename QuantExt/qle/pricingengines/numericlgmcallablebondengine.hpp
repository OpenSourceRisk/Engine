/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file fdlgmcallablebondengine.hpp */

#pragma once

#include <qle/instruments/callablebond.hpp>
#include <qle/instruments/cashflowresults.hpp>
#include <qle/models/lgm.hpp>
#include <qle/models/lgmconvolutionsolver2.hpp>
#include <qle/models/lgmfdsolver.hpp>
#include <qle/pricingengines/forwardenabledbondengine.hpp>

#include <ql/termstructures/defaulttermstructure.hpp>

namespace QuantExt {

class NumericLgmCallableBondEngineBase {
public:
    NumericLgmCallableBondEngineBase(
        const QuantLib::ext::shared_ptr<LgmBackwardSolver>& solver, const Size americanExerciseTimeStepsPerYear = 24,
        const Handle<QuantLib::YieldTermStructure>& referenceCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& discountingSpread = Handle<QuantLib::Quote>(),
        const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve =
            Handle<QuantLib::DefaultProbabilityTermStructure>(),
        const Handle<QuantLib::YieldTermStructure>& incomeCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& recoveryRate = Handle<QuantLib::Quote>(), const bool spreadOnIncome = true,
        const bool generateAdditionalResults = true);

protected:
    // inputs set in ctor
    QuantLib::ext::shared_ptr<LgmBackwardSolver> solver_;
    Size americanExerciseTimeStepsPerYear_;
    Handle<QuantLib::YieldTermStructure> referenceCurve_;
    Handle<QuantLib::Quote> discountingSpread_;
    Handle<QuantLib::DefaultProbabilityTermStructure> creditCurve_;
    Handle<QuantLib::YieldTermStructure> incomeCurve_;
    Handle<QuantLib::Quote> recoveryRate_;
    bool spreadOnIncome_;
    bool generateAdditionalResults_;

    // inputs set for calculation in derived classes
    mutable Date npvDate_;
    mutable Date settlementDate_;
    mutable bool conditionalOnSurvival_;
    mutable std::vector<CashFlowResults>* cfResults_;
    mutable CallableBond::arguments* instrArgs_;

    // outputs
    mutable Real npv_, settlementValue_;
    mutable std::map<std::string, boost::any> additionalResults_;

    void calculate() const;
};

class NumericLgmCallableBondEngine : public CallableBond::engine,
                                     public NumericLgmCallableBondEngineBase,
                                     public QuantExt::ForwardEnabledBondEngine {
public:
    NumericLgmCallableBondEngine(
        const Handle<LGM>& model, const Real sy, const Size ny, const Real sx, const Size nx,
        const Size americanExerciseTimeStepsPerYear = 24,
        const Handle<QuantLib::YieldTermStructure>& referenceCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& discountingSpread = Handle<QuantLib::Quote>(),
        const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve =
            Handle<QuantLib::DefaultProbabilityTermStructure>(),
        const Handle<QuantLib::YieldTermStructure>& incomeCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& recoveryRate = Handle<QuantLib::Quote>(), const bool spreadOnIncome = true,
        const bool generateAdditionalResults = true);
    NumericLgmCallableBondEngine(
        const Handle<LGM>& model, const Real maxTime = 50.0,
        const QuantLib::FdmSchemeDesc scheme = QuantLib::FdmSchemeDesc::Douglas(), const Size stateGridPoints = 64,
        const Size timeStepsPerYear = 24, const Real mesherEpsilon = 1E-4,
        const Size americanExerciseTimeStepsPerYear = 24,
        const Handle<QuantLib::YieldTermStructure>& referenceCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& discountingSpread = Handle<QuantLib::Quote>(),
        const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve =
            Handle<QuantLib::DefaultProbabilityTermStructure>(),
        const Handle<QuantLib::YieldTermStructure>& incomeCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& recoveryRate = Handle<QuantLib::Quote>(), const bool spreadOnIncome = true,
        const bool generateAdditionalResults = true);

    //! ForwardEnabledBondEngine interface
    std::pair<Real, Real> forwardPrice(const QuantLib::Date& forwardNpvDate, const QuantLib::Date& settlementDate,
                                       const bool conditionalOnSurvival = true,
                                       std::vector<CashFlowResults>* const cfResults = nullptr,
                                       QuantLib::Leg* const expectedCashflows = nullptr) const override;

private:
    void calculate() const override;
};

} // namespace QuantExt
