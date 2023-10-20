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

#include <ql/instruments/nonstandardswaption.hpp>
#include <ql/instruments/swaption.hpp>
#include <qle/instruments/multilegoption.hpp>
#include <qle/models/lgmconvolutionsolver2.hpp>
#include <qle/models/lgmvectorised.hpp>

#include <ql/pricingengines/genericmodelengine.hpp>

namespace QuantExt {

class NumericLgmMultiLegOptionEngineBase : protected LgmConvolutionSolver2 {
public:
    NumericLgmMultiLegOptionEngineBase(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy,
                                       const Size ny, const Real sx, const Size nx,
                                       const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>());

protected:
    void calculate() const;

    Handle<YieldTermStructure> discountCurve_;

    // inputs set by derived classes
    mutable std::vector<Leg> legs_;
    mutable std::vector<Real> payer_;
    mutable std::vector<Currency> currency_;
    mutable boost::shared_ptr<Exercise> exercise_;
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
    NumericLgmMultiLegOptionEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                                   const Real sx, const Size nx,
                                   const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>());

    void calculate() const override;
};

class NumericLgmSwaptionEngine : public QuantLib::GenericEngine<Swaption::arguments, Swaption::results>,
                                 public NumericLgmMultiLegOptionEngineBase {
public:
    NumericLgmSwaptionEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                             const Real sx, const Size nx,
                             const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>());

    void calculate() const override;
};

class NumericLgmNonstandardSwaptionEngine
    : public QuantLib::GenericEngine<NonstandardSwaption::arguments, NonstandardSwaption::results>,
      public NumericLgmMultiLegOptionEngineBase {
public:
    NumericLgmNonstandardSwaptionEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy,
                                        const Size ny, const Real sx, const Size nx,
                                        const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>());

    void calculate() const override;
};

} // namespace QuantExt
