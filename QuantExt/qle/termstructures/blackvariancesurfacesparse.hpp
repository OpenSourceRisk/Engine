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

/*! \file blackvariancesurfacesparse.hpp
 \brief Black volatility surface modelled as variance surface
 */

#ifndef quantext_black_variance_surface_sparse_hpp
#define quantext_black_variance_surface_sparse_hpp

#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/math/interpolation.hpp>

namespace QuantExt {
using namespace QuantLib;

namespace detail {
struct CloseEnoughComparator {
    explicit CloseEnoughComparator(const Real v) : v_(v) {}
    bool operator()(const Real w) const { return close_enough(v_, w); }
    Real v_;
};
} // namespace detail


//! Black volatility surface based on sparse matrix.
//!  \ingroup termstructures
class BlackVarianceSurfaceSparse : public BlackVarianceTermStructure {

public:
    BlackVarianceSurfaceSparse(const QuantLib::Date& referenceDate, const Calendar& cal, const std::vector<Date>& dates,
                               const std::vector<Real>& strikes, const std::vector<Volatility>& volatilities,
                               const DayCounter& dayCounter);

    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const { return dayCounter_; }
    Date maxDate() const { return Date::maxDate(); }
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Real minStrike() const { return 0; }
    Real maxStrike() const { return QL_MAX_REAL; }

    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&);
    //@}
protected:
    virtual Real blackVarianceImpl(Time t, Real strike) const;

private:
    Real getVarForStrike(Real strike, const std::vector<Real>& strks, const std::vector<Real>& vars,
                         const QuantLib::Interpolation& intrp) const;

    DayCounter dayCounter_;
    std::vector<Date> expiries_;                            // expiries
    std::vector<Time> times_;                               // times
    std::vector<Interpolation> interpolations_;             // strike interpolations for each expiry
    std::vector<std::vector<Real> > strikes_;               // strikes for each expiry
    std::vector<std::vector<Real> > variances_;
};

// inline definitions

inline void BlackVarianceSurfaceSparse::accept(AcyclicVisitor& v) {
    Visitor<BlackVarianceSurfaceSparse>* v1 = dynamic_cast<Visitor<BlackVarianceSurfaceSparse>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        BlackVarianceTermStructure::accept(v);
}
} // namespace QuantExt

#endif
