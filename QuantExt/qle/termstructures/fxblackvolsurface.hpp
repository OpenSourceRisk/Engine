/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file fxblackvolsurface.hpp
    \brief FX Black volatility surface that incorporates an FxSmile
  \ingroup termstructures
*/

#ifndef quantext_fx_black_vol_surface_hpp
#define quantext_fx_black_vol_surface_hpp

#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/math/interpolation.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/termstructures/fxvannavolgasmilesection.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Fx Black volatility surface
/*! This class calculates time/strike dependent Black volatilities
  \ingroup termstructures
*/
class FxBlackVolatilitySurface : public BlackVolatilityTermStructure {
public:
    FxBlackVolatilitySurface(const Date& referenceDate, const std::vector<Date>& dates,
                             const std::vector<Volatility>& atmVols, const std::vector<Volatility>& rr,
                             const std::vector<Volatility>& bf, const DayCounter& dayCounter, const Calendar& cal,
                             const Handle<Quote>& fxSpot, const Handle<YieldTermStructure>& domesticTS,
                             const Handle<YieldTermStructure>& foreignTS, bool requireMonotoneVariance = true,
                             const DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmType::AtmDeltaNeutral,
                             const DeltaVolQuote::DeltaType deltaType = DeltaVolQuote::DeltaType::Spot,
                             const Real delta = 0.25, const Period& switchTenor = 0 * Days,
                             const DeltaVolQuote::AtmType longTermAtmType = DeltaVolQuote::AtmType::AtmDeltaNeutral,
                             const DeltaVolQuote::DeltaType longTermDeltaType = DeltaVolQuote::DeltaType::Spot);

    //! \name TermStructure interface
    //@{
    DayCounter dayCounter() const override { return dayCounter_; }
    Date maxDate() const override { return maxDate_; }
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Real minStrike() const override { return 0; } // we allow 0 for ATM vols
    Real maxStrike() const override { return QL_MAX_REAL; }
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}
    //! Return an FxSmile for the time t
    /*! Note the smile does not observe the spot or YTS handles, it will
     *  not update when they change
     */
    QuantLib::ext::shared_ptr<FxSmileSection> blackVolSmile(Time t) const;

protected:
    virtual Volatility blackVolImpl(Time t, Real strike) const override;

    //! this must be implemented.
    virtual QuantLib::ext::shared_ptr<FxSmileSection> blackVolSmileImpl(Real spot, Real rd, Real rf, Time t, Volatility atm,
                                                                Volatility rr, Volatility bf) const = 0;

    std::vector<Time> times_;
    DayCounter dayCounter_;
    Handle<Quote> fxSpot_;
    Handle<YieldTermStructure> domesticTS_;
    Handle<YieldTermStructure> foreignTS_;
    BlackVarianceCurve atmCurve_;
    std::vector<Volatility> rr_;
    std::vector<Volatility> bf_;
    DeltaVolQuote::AtmType atmType_;
    DeltaVolQuote::DeltaType deltaType_;
    Real delta_;
    Period switchTenor_;
    DeltaVolQuote::AtmType longTermAtmType_;
    DeltaVolQuote::DeltaType longTermDeltaType_;
    Interpolation rrCurve_;
    Interpolation bfCurve_;
    Date maxDate_;
};

// inline definitions
inline void FxBlackVolatilitySurface::accept(AcyclicVisitor& v) {
    Visitor<FxBlackVolatilitySurface>* v1 = dynamic_cast<Visitor<FxBlackVolatilitySurface>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        BlackVolatilityTermStructure::accept(v);
}

//! Fx Black vanna volga volatility surface
/*! This class calculates time/strike dependent Black volatilities
     \ingroup termstructures
*/
class FxBlackVannaVolgaVolatilitySurface : public FxBlackVolatilitySurface {
public:
    FxBlackVannaVolgaVolatilitySurface(
        const Date& refDate, const std::vector<Date>& dates, const std::vector<Volatility>& atmVols,
        const std::vector<Volatility>& rr, const std::vector<Volatility>& bf, const DayCounter& dc, const Calendar& cal,
        const Handle<Quote>& fx, const Handle<YieldTermStructure>& dom, const Handle<YieldTermStructure>& fore,
        bool requireMonotoneVariance = true, const bool firstApprox = false,
        const DeltaVolQuote::AtmType atmType = DeltaVolQuote::AtmType::AtmDeltaNeutral,
        const DeltaVolQuote::DeltaType deltaType = DeltaVolQuote::DeltaType::Spot, const Real delta = 0.25,
        const Period& switchTenor = 0 * Days,
        const DeltaVolQuote::AtmType longTermAtmType = DeltaVolQuote::AtmType::AtmDeltaNeutral,
        const DeltaVolQuote::DeltaType longTermDeltaType = DeltaVolQuote::DeltaType::Spot)
        : FxBlackVolatilitySurface(refDate, dates, atmVols, rr, bf, dc, cal, fx, dom, fore, requireMonotoneVariance,
                                   atmType, deltaType, delta, switchTenor, longTermAtmType, longTermDeltaType),
          firstApprox_(firstApprox) {}

protected:
    bool firstApprox_;
    virtual QuantLib::ext::shared_ptr<FxSmileSection> blackVolSmileImpl(Real spot, Real rd, Real rf, Time t, Volatility atm,
                                                                Volatility rr, Volatility bf) const override;
};
} // namespace QuantExt

#endif
