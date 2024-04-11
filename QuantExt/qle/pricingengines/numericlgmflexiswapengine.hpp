/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/numericlgmflexiswapengine.hpp
    \brief numeric engine for flexi swaps in the LGM model
*/

#pragma once

#include <qle/instruments/flexiswap.hpp>

#include <qle/models/lgm.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/pricingengines/lgmconvolutionsolver.hpp>

#include <ql/indexes/iborindex.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>

namespace QuantExt {

//! Numerical engine for flexi swaps in the LGM model
/*! This is a modifed version of qle/pricingengines/numericlgmswaptionengine.hpp
    Reference: F. Jamshidian, Replication of Flexi-swaps, January 2005

    There are two implementations of the rollback

    a) SingleSwaptions: price each swaption on its own, using the grid rollback
    b) SwaptionArray: price all swaptions simultaneously by rolling back suitable
       Arrays instead of Reals

    For a large swaption basket b) is faster than a). The two methods can be specified
    explicitly or the Automatic mode can be used which uses a) if the "effective number
    of full swaptions" is below the given singleSwaptionThreshold and b) otherwise.

    Here, the effective number of full swaptions is defined to be the sum of event dates
    of all the swaptions in the basket divided by the number of event dates of the full
    underlying.
*/

class NumericLgmFlexiSwapEngineBase : public LgmConvolutionSolver {
public:
    enum class Method { SwaptionArray, SingleSwaptions, Automatic };
    NumericLgmFlexiSwapEngineBase(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                                  const Real sx, const Size nx,
                                  const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                  const Method method = Method::Automatic, const Real singleSwaptionThreshold = 20.0);

protected:
    // returns option value, underlying value
    std::pair<Real, Real> calculate() const;
    // helper method, compute underlying value w.r.t. an assumed unit notional
    Real underlyingValue(const Real, const Real, const Date&, const Size, const Size, const Real, const Real) const;
    // parameters
    const Handle<YieldTermStructure> discountCurve_;
    const Method method_;
    const Real singleSwaptionThreshold_;
    //
    mutable QuantLib::ext::shared_ptr<IborIndex> iborModelIndex_;
    mutable QuantLib::ext::shared_ptr<LgmImpliedYieldTermStructure> iborModelCurve_;
    // arguments to be set by derived pricing engines before calling calculate()
    mutable VanillaSwap::Type type;
    mutable std::vector<Real> fixedNominal, floatingNominal;
    mutable std::vector<Date> fixedResetDates;
    mutable std::vector<Date> fixedPayDates;
    mutable std::vector<Time> floatingAccrualTimes;
    mutable std::vector<Date> floatingResetDates;
    mutable std::vector<Date> floatingFixingDates;
    mutable std::vector<Date> floatingPayDates;
    mutable std::vector<Real> fixedCoupons;
    mutable std::vector<Real> fixedRate;
    mutable std::vector<Real> floatingGearings;
    mutable std::vector<Real> floatingSpreads;
    mutable std::vector<Real> cappedRate;
    mutable std::vector<Real> flooredRate;
    mutable std::vector<Real> floatingCoupons;
    mutable QuantLib::ext::shared_ptr<IborIndex> iborIndex;
    mutable std::vector<Real> lowerNotionalBound;
    mutable QuantLib::Position::Type optionPosition;
    mutable std::vector<bool> notionalCanBeDecreased;
}; // NumericLgmFlexiSwapEngineBase

class NumericLgmFlexiSwapEngine : public GenericEngine<FlexiSwap::arguments, FlexiSwap::results>,
                                  public NumericLgmFlexiSwapEngineBase {
public:
    NumericLgmFlexiSwapEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                              const Real sx, const Size nx,
                              const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                              const Method method = Method::Automatic, const Real singleSwaptionThreshold = 20.0);

private:
    void calculate() const override;
}; // NnumercLgmFlexiSwapEngine

} // namespace QuantExt
