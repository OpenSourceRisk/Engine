/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 Copyright (C) 2022 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file blackvolsurfacedelta.hpp
 \brief Black volatility surface based on delta
 \ingroup termstructures
 */

#ifndef quantext_black_variance_surface_delta_hpp
#define quantext_black_variance_surface_delta_hpp

#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/math/interpolation.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <qle/termstructures/fxsmilesection.hpp>

namespace QuantExt {
using namespace QuantLib;

class InterpolatedSmileSection : public FxSmileSection {
public:
    //! Supported interpolation methods
    enum class InterpolationMethod { Linear, NaturalCubic, FinancialCubic, CubicSpline };

    //! ctor
    InterpolatedSmileSection(Real spot, Real rd, Real rf, Time t, const std::vector<Real>& strikes,
                             const std::vector<Volatility>& vols, InterpolationMethod method,
                             bool flatExtrapolation = false);

    Volatility volatility(Real strike) const override;

    //! \name Inspectors
    //@{
    const std::vector<Real>& strikes() const { return strikes_; }
    const std::vector<Volatility>& volatilities() const { return vols_; }
    //@}

private:
    Interpolation interpolator_;
    std::vector<Real> strikes_;
    std::vector<Volatility> vols_;
    bool flatExtrapolation_;
};

class ConstantSmileSection : public FxSmileSection {
public:
    //! ctor
    ConstantSmileSection(const Volatility vol) : vol_(vol) {}

    Volatility volatility(Real strike) const override { return vol_; }

    //! \name Inspectors
    //@{
    const Volatility volatility() const { return vol_; }
    //@}

private:
    Volatility vol_;
};

//! Abstract Black volatility surface based on delta
//!  \ingroup termstructures
class BlackVolatilitySurfaceDelta : public BlackVolatilityTermStructure {
public:
    BlackVolatilitySurfaceDelta(Date referenceDate, const std::vector<Date>& dates, const std::vector<Real>& putDeltas,
                                const std::vector<Real>& callDeltas, bool hasAtm, const Matrix& blackVolMatrix,
                                const DayCounter& dayCounter, const Calendar& cal, const Handle<Quote>& spot,
                                const Handle<YieldTermStructure>& domesticTS,
                                const Handle<YieldTermStructure>& foreignTS,
                                DeltaVolQuote::DeltaType dt = DeltaVolQuote::DeltaType::Spot,
                                DeltaVolQuote::AtmType at = DeltaVolQuote::AtmType::AtmDeltaNeutral,
                                boost::optional<QuantLib::DeltaVolQuote::DeltaType> atmDeltaType = boost::none,
                                const Period& switchTenor = 0 * Days,
                                DeltaVolQuote::DeltaType ltdt = DeltaVolQuote::DeltaType::Fwd,
                                DeltaVolQuote::AtmType ltat = DeltaVolQuote::AtmType::AtmDeltaNeutral,
                                boost::optional<QuantLib::DeltaVolQuote::DeltaType> longTermAtmDeltaType = boost::none,
                                InterpolatedSmileSection::InterpolationMethod interpolationMethod =
                                    InterpolatedSmileSection::InterpolationMethod::Linear,
                                bool flatExtrapolation = true);

    //! \name TermStructure interface
    //@{
    Date maxDate() const override { return Date::maxDate(); }
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

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Date>& dates() const { return dates_; }
    //@}

    //! Return an FxSmile for the time t
    /*! Note the smile does not observe the spot or YTS handles, it will
     *  not update when they change.
     *
     *  This is not really FX specific
     */
    QuantLib::ext::shared_ptr<FxSmileSection> blackVolSmile(Time t) const;

    QuantLib::ext::shared_ptr<FxSmileSection> blackVolSmile(const QuantLib::Date& d) const;

protected:
    virtual Volatility blackVolImpl(Time t, Real strike) const override;

private:
    std::vector<Date> dates_;
    std::vector<Time> times_;

    std::vector<Real> putDeltas_;
    std::vector<Real> callDeltas_;
    bool hasAtm_;
    std::vector<QuantLib::ext::shared_ptr<BlackVarianceCurve> > interpolators_;

    Handle<Quote> spot_;
    Handle<YieldTermStructure> domesticTS_;
    Handle<YieldTermStructure> foreignTS_;

    DeltaVolQuote::DeltaType dt_;
    DeltaVolQuote::AtmType at_;
    boost::optional<QuantLib::DeltaVolQuote::DeltaType> atmDeltaType_;
    Period switchTenor_;
    DeltaVolQuote::DeltaType ltdt_;
    DeltaVolQuote::AtmType ltat_;
    boost::optional<QuantLib::DeltaVolQuote::DeltaType> longTermAtmDeltaType_;

    InterpolatedSmileSection::InterpolationMethod interpolationMethod_;
    bool flatExtrapolation_;

    Real switchTime_;

    // calculate forward for time $t$
    Real forward(Time t) const;
};

// inline definitions

inline void BlackVolatilitySurfaceDelta::accept(AcyclicVisitor& v) {
    Visitor<BlackVolatilitySurfaceDelta>* v1 = dynamic_cast<Visitor<BlackVolatilitySurfaceDelta>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        BlackVolatilityTermStructure::accept(v);
}

} // namespace QuantExt

#endif
