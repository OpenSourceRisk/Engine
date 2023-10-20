/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file blacktriangulationatmvol.hpp
    \brief Black volatility surface that implies an ATM vol based on triangulation
    \ingroup termstructures
 */
#pragma once

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Black volatility surface that implies an ATM vol based on triangulation
/*! This class is used when one wants to proxy a volatility like XAU/EUR using
 *  XAU/USD, EUR/USD and a correlation. It uses the cosing rule.
 *  The correlation can be implied from a vol (if you had XAU/EUR) or historically estimated.
 *  This class is just ATM, otherwise there is a degree of freedom in selecting strikes.
 *
 *  One application of this is SIMM sensis, where the vol for XAU/EUR must be broken down
 *  into XAU/USD and EUR/USD.
 *
 *  Other methods for building a full surface exist, but to keep things simple we just do ATM
 *
 *  \ingroup termstructures
 */
class BlackTriangulationATMVolTermStructure : public BlackVolatilityTermStructure {
public:
    //! Constructor takes two BlackVolTermStructure and a correlation
    /*! Attributes like referenceDate, settlementDays, Calendar, etc are taken from vol1
     */
    BlackTriangulationATMVolTermStructure(const Handle<BlackVolTermStructure>& vol1,
                                          const Handle<BlackVolTermStructure>& vol2,
                                          const Handle<CorrelationTermStructure>& rho, const bool staticVol2 = false)
        : BlackVolatilityTermStructure(vol1->businessDayConvention(), vol1->dayCounter()), vol1_(vol1), vol2_(vol2),
          rho_(rho), staticVol2_(staticVol2) {
        registerWith(vol1_);
        registerWith(vol2_);
        registerWith(rho_);
        enableExtrapolation(vol1_->allowsExtrapolation() && vol2_->allowsExtrapolation());
    }
    //! \name TermStructure interface
    //@{
    const Date& referenceDate() const override { return vol1_->referenceDate(); }
    Date maxDate() const override { return std::min(vol1_->maxDate(), vol2_->maxDate()); }
    Natural settlementDays() const override { return vol1_->settlementDays(); }
    Calendar calendar() const override { return vol1_->calendar(); }
    //! \name Observer interface
    //@{
    void update() override { notifyObservers(); }
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Real minStrike() const override { return 0; }
    Real maxStrike() const override { return QL_MAX_REAL; }
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}
protected:
    virtual Volatility blackVolImpl(Time t, Real) const override {
        Real c = rho_->correlation(t);
        Volatility v1 = vol1_->blackVol(t, Null<Real>());
        Real v2 = Null<Real>();
        if (staticVol2_) {
            if (auto tmp = staticVolCache_.find(t); tmp != staticVolCache_.end()) {
                v2 = tmp->second;
            } else {
                v2 = vol2_->blackVol(t, Null<Real>());
                staticVolCache_[t] = v2;
            }
        } else {
            v2 = vol2_->blackVol(t, Null<Real>());
        }
        return std::sqrt(std::max(0.0, v1 * v1 + v2 * v2 - 2.0 * c * v1 * v2));
    }

private:
    Handle<BlackVolTermStructure> vol1_;
    Handle<BlackVolTermStructure> vol2_;
    Handle<CorrelationTermStructure> rho_;
    bool staticVol2_;
    mutable std::map<double, double> staticVolCache_;
};

// inline definitions
inline void BlackTriangulationATMVolTermStructure::accept(AcyclicVisitor& v) {
    Visitor<BlackTriangulationATMVolTermStructure>* v1 =
        dynamic_cast<Visitor<BlackTriangulationATMVolTermStructure>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        BlackVolatilityTermStructure::accept(v);
}
} // namespace QuantExt
