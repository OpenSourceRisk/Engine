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
 \brief Black volatility surface modeled as variance surface
 */

#ifndef quantext_black_variance_surface_sparse_hpp
#define quantext_black_variance_surface_sparse_hpp

#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/interpolators/optioninterpolator2d.hpp>

namespace QuantExt {

//! Black volatility surface based on sparse matrix.
//!  \ingroup termstructures
class BlackVarianceSurfaceSparse : public QuantLib::BlackVarianceTermStructure,
                                   public OptionInterpolator2d<QuantLib::Linear, QuantLib::Linear> {

public:
    BlackVarianceSurfaceSparse(const QuantLib::Date& referenceDate, const QuantLib::Calendar& cal,
                               const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& strikes,
                               const std::vector<QuantLib::Volatility>& volatilities,
                               const QuantLib::DayCounter& dayCounter, bool lowerStrikeConstExtrap = true,
                               bool upperStrikeConstExtrap = true, 
                               QuantLib::BlackVolTimeExtrapolation timeExtrapolation
                                = QuantLib::BlackVolTimeExtrapolation::FlatVolatility);

    enum class TimeInterpolationMethod { Linear, Flat };
    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override { return QuantLib::Date::maxDate(); }
    const QuantLib::Date& referenceDate() const override { return OptionInterpolator2d::referenceDate(); }
    QuantLib::DayCounter dayCounter() const override { return OptionInterpolator2d::dayCounter(); }
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Real minStrike() const override { return 0; }
    QuantLib::Real maxStrike() const override { return QL_MAX_REAL; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(QuantLib::AcyclicVisitor&) override;
    //@}

protected:
    virtual QuantLib::Real blackVarianceImpl(QuantLib::Time t, QuantLib::Real strike) const override;

    QuantLib::BlackVolTimeExtrapolation timeExtrapolation_;
};

// inline definitions

inline void BlackVarianceSurfaceSparse::accept(QuantLib::AcyclicVisitor& v) {
    QuantLib::Visitor<BlackVarianceSurfaceSparse>* v1 =
        dynamic_cast<QuantLib::Visitor<BlackVarianceSurfaceSparse>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        QuantLib::BlackVarianceTermStructure::accept(v);
}
} // namespace QuantExt

#endif
