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

/*! \file numericlgmswaptionengine.hpp
    \brief numeric engine for bermudan swaptions in the LGM model

        \ingroup engines
*/

#ifndef quantext_numeric_lgm_swaption_engine_hpp
#define quantext_numeric_lgm_swaption_engine_hpp

#include <qle/models/lgm.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/pricingengines/lgmconvolutionsolver.hpp>

#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/nonstandardswaption.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>

namespace QuantExt {

//! Numerical engine for bermudan swaptions in the LGM model
/*!
    All fixed coupons with start date greater or equal to the respective
    option expiry are considered to be
    part of the exercise into right.

    \warning Cash ParYieldCurve-settled swaptions are not supported

    Reference: Hagan, Methodology for callable swaps and Bermudan
               exercise into swaptions
*/

/*! Base class from which we derive the engines for both the Swaption
  and NonstandardSwaption instrument

  \ingroup engines
*/
class NumericLgmSwaptionEngineBase : protected LgmConvolutionSolver {
protected:
    NumericLgmSwaptionEngineBase(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                                 const Real sx, const Size nx, const Handle<YieldTermStructure>& discountCurve);

    virtual ~NumericLgmSwaptionEngineBase() {}

    Real calculate() const;
    std::map<std::string, boost::any> additionalResults() const;

    virtual Real conditionalSwapValue(Real x, Real t, const Date expiry0) const = 0;

    Real rebatePv(const Real x, const Real t, const Size exerciseIndex) const;

    mutable boost::shared_ptr<Exercise> exercise_;
    mutable boost::shared_ptr<IborIndex> iborIndex_, iborIndexCorrected_;
    mutable boost::shared_ptr<LgmImpliedYieldTermStructure> iborModelCurve_;
    const Handle<YieldTermStructure> discountCurve_;
}; // NnumercLgmSwaptionEngineBase

//! Engine for Swaption instrument
/*! \ingroup engines
 */
class NumericLgmSwaptionEngine : public GenericEngine<Swaption::arguments, Swaption::results>,
                                 public NumericLgmSwaptionEngineBase {
public:
    NumericLgmSwaptionEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                             const Real sx, const Size nx,
                             const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>())
        : GenericEngine<Swaption::arguments, Swaption::results>(),
          NumericLgmSwaptionEngineBase(model, sy, ny, sx, nx, discountCurve) {
        if (!discountCurve_.empty())
            registerWith(discountCurve_);
        registerWith(LgmConvolutionSolver::model());
    }

    void calculate() const;

protected:
    Real conditionalSwapValue(Real x, Real t, const Date expiry0) const;
};

//! Engine for NonstandardSwaption
/*! \ingroup engines */
class NumericLgmNonstandardSwaptionEngine
    : public GenericEngine<NonstandardSwaption::arguments, NonstandardSwaption::results>,
      public NumericLgmSwaptionEngineBase {
public:
    NumericLgmNonstandardSwaptionEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy,
                                        const Size ny, const Real sx, const Size nx,
                                        const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>())
        : GenericEngine<NonstandardSwaption::arguments, NonstandardSwaption::results>(),
          NumericLgmSwaptionEngineBase(model, sy, ny, sx, nx, discountCurve) {
        if (!discountCurve_.empty())
            registerWith(discountCurve_);
        registerWith(LgmConvolutionSolver::model());
    }

    void calculate() const;

protected:
    Real conditionalSwapValue(Real x, Real t, const Date expiry0) const;
};

} // namespace QuantExt

#endif
