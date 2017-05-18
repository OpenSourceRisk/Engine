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

/*! \file qle/models/crossassetmodelimpliedeqvoltermstructure.hpp
    \brief dynamic black volatility term structure
    \ingroup crossassetmodel
*/

#ifndef quantext_crossassetmodel_implied_eq_volatility_termstructure_hpp
#define quantext_crossassetmodel_implied_eq_volatility_termstructure_hpp

#include <qle/models/crossassetmodel.hpp>

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Analytic Cross-Asset LGM Equity Option Engine
/*! \ingroup crossassetmodel
 */
class AnalyticXAssetLgmEquityOptionEngine;

//! Cross Asset Model Implied EQ Term Structure
/*! The termstructure as the reference date of the model at construction,
  you can vary this and the relevant state variables using the state() and
  move() methods.

  \ingroup crossassetmodel
*/

class CrossAssetModelImpliedEqVolTermStructure : public BlackVolTermStructure {
public:
    CrossAssetModelImpliedEqVolTermStructure(const boost::shared_ptr<CrossAssetModel>& model, const Size equityIndex,
                                             BusinessDayConvention bdc = Following, const DayCounter& dc = DayCounter(),
                                             const bool purelyTimeBased = false);

    void referenceDate(const Date& d);
    void referenceTime(const Time t);
    void state(const Real eqIr, const Real logEq);
    void move(const Date& d, const Real eqIr, const Real logEq);
    void move(const Time t, const Real eqIr, const Real logEq);

    /* VolatilityTermStructure interface */
    Real minStrike() const;
    Real maxStrike() const;
    /* TermStructure interface */
    Date maxDate() const;
    Time maxTime() const;
    const Date& referenceDate() const;
    /* Observer interface */
    void update();

    Size equityIndex() const { return eqIndex_; }
    Size eqCcyIndex() const { return model_->ccyIndex(model_->eqbs(eqIndex_)->currency()); }

protected:
    /* BlackVolTermStructure interface */
    Real blackVarianceImpl(Time t, Real strike) const;
    Volatility blackVolImpl(Time t, Real strike) const;

private:
    const boost::shared_ptr<CrossAssetModel> model_;
    const Size eqIndex_;
    const bool purelyTimeBased_;
    const boost::shared_ptr<AnalyticXAssetLgmEquityOptionEngine> engine_;
    Date referenceDate_;
    Real relativeTime_, eqIr_, eq_;
};

} // namespace QuantExt

#endif
